// MIT License - see LICENSE
#include "qopenglpaintengine_p.h"

#include "qrasterpaintengine_p.h"
#include "qrasterizer_p.h"

#include <QtCore/qdebug.h>
#include <QtGui/qopenglcontext.h>
#include <QtGui/qopenglfunctions.h>
#include <QtOpenGL/qopenglpaintdevice.h>

#include <algorithm>
#include <cmath>
#include <map>
#include <memory>
#include <mutex>

namespace raster = QtRasterPrivate;

namespace {

const char kVertexBody[] = R"(
ATTRIBUTE vec2 a_pos;
ATTRIBUTE vec2 a_tex;
uniform vec2 u_size;
uniform float u_flip;
VARYING vec2 v_pos;
VARYING vec2 v_tex;
void main() {
    v_pos = a_pos;
    v_tex = a_tex;
    vec2 ndc = a_pos / u_size * 2.0 - 1.0;
    gl_Position = vec4(ndc.x, -ndc.y * u_flip, 0.0, 1.0);
}
)";

// The modes: 0 one color, 1 to 3 linear, radial and conical gradients (from a table of 256 colors), 4 an image,
// 5 an alpha mask in the color. The gradient formulas are those of the raster engine.
const char kFragmentBody[] = R"(
VARYING vec2 v_pos;
VARYING vec2 v_tex;
uniform int u_mode;
uniform vec4 u_color;
uniform mat3 u_toGradient;
uniform vec2 u_p1;
uniform vec2 u_p2;
uniform float u_radius;
uniform float u_angle;
uniform int u_spread;
uniform float u_opacity;
uniform sampler2D u_tex;

vec4 lookup(float t) {
    float i = floor(clamp(t * 255.0 + 0.5, 0.0, 255.0));
    return TEXTURE(u_tex, vec2((i + 0.5) / 256.0, 0.5));
}

void main() {
    vec4 c;
    if (u_mode == 0) {
        c = u_color;
    } else if (u_mode <= 3) {
        vec2 p = (u_toGradient * vec3(v_pos, 1.0)).xy;
        if (u_mode == 3) {
            float deg = degrees(atan(-(p.y - u_p1.y), p.x - u_p1.x));
            c = lookup(mod(deg - u_angle + 720.0, 360.0) / 360.0);
        } else {
            float t = 1.0;
            if (u_mode == 1) {
                vec2 v = u_p2 - u_p1;
                float len2 = dot(v, v);
                if (len2 > 0.0)
                    t = dot(p - u_p1, v) / len2;
            } else if (u_radius > 0.0) {
                vec2 d = p - u_p2;
                vec2 cd = u_p1 - u_p2;
                float a = dot(cd, cd) - u_radius * u_radius;
                if (a > -1e-6 * u_radius * u_radius) {
                    t = length(d) / u_radius;
                } else {
                    float b = dot(d, cd);
                    float e = dot(d, d);
                    t = (b - sqrt(max(0.0, b * b - a * e))) / a;
                }
            }
            if (u_spread == 1)
                t = t - floor(t);
            else if (u_spread == 2)
                t = abs(t - 2.0 * floor((t + 1.0) / 2.0));
            c = lookup(t);
        }
    } else if (u_mode == 4) {
        c = TEXTURE(u_tex, v_tex) * u_opacity;
    } else {
        c = u_color * TEXTURE(u_tex, v_tex).a;
    }
    FRAGCOLOR = c;
}
)";

using GenVertexArrays = void(QOPENGLF_APIENTRYP)(GLsizei, GLuint *);
using BindVertexArray = void(QOPENGLF_APIENTRYP)(GLuint);

GLuint compile(QOpenGLFunctions *f, GLenum type, const QByteArray &source) {
    const GLuint shader = f->glCreateShader(type);
    const char *text = source.constData();
    f->glShaderSource(shader, 1, &text, nullptr);
    f->glCompileShader(shader);
    GLint ok = 0;
    f->glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024] = {};
        f->glGetShaderInfoLog(shader, sizeof log, nullptr, log);
        qWarning("QOpenGL2PaintEngine: cannot compile a shader: %s", log);
        f->glDeleteShader(shader);
        return 0;
    }
    return shader;
}

void toRgba(QRgb p, unsigned char *out) {
    out[0] = static_cast<unsigned char>(qRed(p));
    out[1] = static_cast<unsigned char>(qGreen(p));
    out[2] = static_cast<unsigned char>(qBlue(p));
    out[3] = static_cast<unsigned char>(qAlpha(p));
}

} // namespace

struct QOpenGL2PaintEngine::Resources {
    GLuint program = 0;
    GLuint vbo = 0;
    GLuint uvbo = 0;
    GLuint vao = 0;
    GLuint lut = 0;
    GLuint texture = 0;
    GLint uSize = -1, uFlip = -1, uMode = -1, uColor = -1, uToGradient = -1, uP1 = -1, uP2 = -1, uRadius = -1,
          uAngle = -1, uSpread = -1, uOpacity = -1, uTex = -1;
    BindVertexArray bindVertexArray = nullptr;

    bool create(QOpenGLContext *context);
};

bool QOpenGL2PaintEngine::Resources::create(QOpenGLContext *context) {
    QOpenGLFunctions *f = context->functions();
    const QSurfaceFormat format = context->format();
    QByteArray vertexHead, fragmentHead;
    if (context->isOpenGLES()) {
        vertexHead = "#version 100\n#define ATTRIBUTE attribute\n#define VARYING varying\n";
        fragmentHead = "#version 100\n#ifdef GL_FRAGMENT_PRECISION_HIGH\nprecision highp float;\n#else\n"
                       "precision mediump float;\n#endif\n#define VARYING varying\n#define TEXTURE texture2D\n"
                       "#define FRAGCOLOR gl_FragColor\n";
    } else if (format.profile() == QSurfaceFormat::CoreProfile) {
        vertexHead = "#version 150\n#define ATTRIBUTE in\n#define VARYING out\n";
        fragmentHead = "#version 150\n#define VARYING in\n#define TEXTURE texture\nout vec4 fragColor;\n"
                       "#define FRAGCOLOR fragColor\n";
    } else {
        const bool v120 = format.majorVersion() > 2 || format.minorVersion() >= 1;
        vertexHead = QByteArray(v120 ? "#version 120\n" : "#version 110\n") +
                     "#define ATTRIBUTE attribute\n#define VARYING varying\n";
        fragmentHead = QByteArray(v120 ? "#version 120\n" : "#version 110\n") +
                       "#define VARYING varying\n#define TEXTURE texture2D\n#define FRAGCOLOR gl_FragColor\n";
    }
    const GLuint vs = compile(f, GL_VERTEX_SHADER, vertexHead + kVertexBody);
    const GLuint fs = compile(f, GL_FRAGMENT_SHADER, fragmentHead + kFragmentBody);
    if (!vs || !fs)
        return false;
    program = f->glCreateProgram();
    f->glAttachShader(program, vs);
    f->glAttachShader(program, fs);
    f->glBindAttribLocation(program, 0, "a_pos");
    f->glBindAttribLocation(program, 1, "a_tex");
    f->glLinkProgram(program);
    f->glDeleteShader(vs);
    f->glDeleteShader(fs);
    GLint ok = 0;
    f->glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024] = {};
        f->glGetProgramInfoLog(program, sizeof log, nullptr, log);
        qWarning("QOpenGL2PaintEngine: cannot link the shaders: %s", log);
        return false;
    }
    uSize = f->glGetUniformLocation(program, "u_size");
    uFlip = f->glGetUniformLocation(program, "u_flip");
    uMode = f->glGetUniformLocation(program, "u_mode");
    uColor = f->glGetUniformLocation(program, "u_color");
    uToGradient = f->glGetUniformLocation(program, "u_toGradient");
    uP1 = f->glGetUniformLocation(program, "u_p1");
    uP2 = f->glGetUniformLocation(program, "u_p2");
    uRadius = f->glGetUniformLocation(program, "u_radius");
    uAngle = f->glGetUniformLocation(program, "u_angle");
    uSpread = f->glGetUniformLocation(program, "u_spread");
    uOpacity = f->glGetUniformLocation(program, "u_opacity");
    uTex = f->glGetUniformLocation(program, "u_tex");
    f->glGenBuffers(1, &vbo);
    f->glGenBuffers(1, &uvbo);
    f->glGenTextures(1, &lut);
    f->glGenTextures(1, &texture);
    // A core profile draws only with a vertex array object bound.
    if (!context->isOpenGLES() && format.profile() == QSurfaceFormat::CoreProfile) {
        auto gen = reinterpret_cast<GenVertexArrays>( // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
            context->getProcAddress("glGenVertexArrays"));
        bindVertexArray = reinterpret_cast<BindVertexArray>( // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
            context->getProcAddress("glBindVertexArray"));
        if (gen && bindVertexArray)
            gen(1, &vao);
    }
    return true;
}

namespace {

struct ResourceMap {
    std::mutex mutex;
    std::map<QOpenGLContext *, std::unique_ptr<QOpenGL2PaintEngine::Resources>> map;
};

ResourceMap &resourceMap() {
    static ResourceMap *m = new ResourceMap; // never destroyed: contexts can go away during static destruction
    return *m;
}

// The program and buffers of the engine in `context`, made when the context first paints. The context must be current.
QOpenGL2PaintEngine::Resources *resourcesFor(QOpenGLContext *context) {
    ResourceMap &m = resourceMap();
    std::lock_guard<std::mutex> lock(m.mutex);
    auto it = m.map.find(context);
    if (it != m.map.end())
        return it->second.get();
    auto res = std::make_unique<QOpenGL2PaintEngine::Resources>();
    if (!res->create(context))
        return nullptr;
    // The objects go with the context; only the entry is dropped.
    QObject::connect(context, &QOpenGLContext::aboutToBeDestroyed, [context] {
        ResourceMap &rm = resourceMap();
        std::lock_guard<std::mutex> l(rm.mutex);
        rm.map.erase(context);
    });
    return m.map.emplace(context, std::move(res)).first->second.get();
}

} // namespace

QOpenGL2PaintEngine::~QOpenGL2PaintEngine() = default;

bool QOpenGL2PaintEngine::begin(QPaintDevice *device) {
    auto *glDevice = dynamic_cast<QOpenGLPaintDevice *>(device);
    if (!glDevice || isActive())
        return false;
    glDevice->ensureActiveTarget();
    m_context = QOpenGLContext::currentContext();
    if (!m_context) {
        qWarning("QPainter::begin(): painting with OpenGL needs a current context");
        return false;
    }
    m_f = m_context->functions();
    m_res = resourcesFor(m_context);
    if (!m_res)
        return false;
    m_device = glDevice;
    m_size = glDevice->size();
    m_flipped = glDevice->paintFlipped();
    setPaintDevice(device);
    m_state = QPaintEngineState();
    m_clipDirty = true;

    // Does the target have a stencil buffer?
    GLint bound = 0;
    m_f->glGetIntegerv(GL_FRAMEBUFFER_BINDING, &bound);
    GLint stencilBits = 0;
    if (m_context->isOpenGLES() || m_context->format().profile() != QSurfaceFormat::CoreProfile) {
        m_f->glGetIntegerv(GL_STENCIL_BITS, &stencilBits);
    } else {
        m_f->glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, bound ? GL_STENCIL_ATTACHMENT : GL_STENCIL,
                                                   GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE, &stencilBits);
    }
    while (m_f->glGetError() != GL_NO_ERROR) {
    }
    m_hasStencil = stencilBits >= 8;
    if (!m_hasStencil) {
        static bool warned = false;
        if (!warned) {
            warned = true;
            qWarning("QPainter: the OpenGL target has no stencil buffer; shapes that are not convex and clip "
                     "regions are drawn wrongly");
        }
    }

    m_f->glViewport(0, 0, m_size.width(), m_size.height());
    m_f->glDisable(GL_DEPTH_TEST);
    m_f->glDisable(GL_CULL_FACE);
    m_f->glDisable(GL_SCISSOR_TEST);
    m_f->glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    if (m_hasStencil) {
        m_f->glStencilMask(0xff);
        m_f->glClearStencil(0);
        m_f->glClear(GL_STENCIL_BUFFER_BIT);
    }
    m_f->glDisable(GL_STENCIL_TEST);
    m_f->glUseProgram(m_res->program);
    if (m_res->vao)
        m_res->bindVertexArray(m_res->vao);
    m_f->glUniform2f(m_res->uSize, static_cast<GLfloat>(std::max(1, m_size.width())),
                     static_cast<GLfloat>(std::max(1, m_size.height())));
    m_f->glUniform1f(m_res->uFlip, m_flipped ? -1.0f : 1.0f);
    m_f->glUniform1i(m_res->uTex, 0);
    m_f->glActiveTexture(GL_TEXTURE0);
    m_f->glEnable(GL_BLEND);
    setActive(true);
    return true;
}

bool QOpenGL2PaintEngine::end() {
    if (m_context && m_f) {
        m_f->glDisable(GL_STENCIL_TEST);
        m_f->glDisable(GL_SCISSOR_TEST);
        m_f->glDisable(GL_BLEND);
        m_f->glStencilMask(0xff);
        m_f->glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        m_f->glDisableVertexAttribArray(0);
        m_f->glDisableVertexAttribArray(1);
        m_f->glBindBuffer(GL_ARRAY_BUFFER, 0);
        m_f->glBindTexture(GL_TEXTURE_2D, 0);
        if (m_res && m_res->vao)
            m_res->bindVertexArray(0);
        m_f->glUseProgram(0);
    }
    m_context = nullptr;
    m_f = nullptr;
    m_res = nullptr;
    m_device = nullptr;
    setPaintDevice(nullptr);
    setActive(false);
    return true;
}

void QOpenGL2PaintEngine::updateState(const QPaintEngineState &state) {
    if (state.hasClipping() != m_clipping || (state.hasClipping() && !(state.clipRegion() == m_clipRegion)))
        m_clipDirty = true;
    m_state = state;
}

// Applies the clip and the blending; returns false if nothing can be drawn.
bool QOpenGL2PaintEngine::prepare() {
    if (!m_context)
        return false;
    if (m_clipDirty) {
        m_clipDirty = false;
        m_clipping = m_state.hasClipping();
        m_clipRegion = m_state.clipRegion();
        m_f->glDisable(GL_SCISSOR_TEST);
        if (!m_clipping) {
            m_clip = Clip::None;
        } else {
            const QRegion region = m_clipRegion.intersected(QRect(QPoint(0, 0), m_size));
            const std::vector<QRect> rects = region.rects();
            if (rects.empty()) {
                m_clip = Clip::Empty;
            } else if (rects.size() == 1 || !m_hasStencil) {
                const QRect r = rects.size() == 1 ? rects.front() : region.boundingRect();
                m_clip = Clip::Scissor;
                m_f->glScissor(r.x(), m_flipped ? r.y() : m_size.height() - r.y() - r.height(), r.width(), r.height());
            } else {
                // The high bit of the stencil buffer holds the clip; the low bits count windings.
                m_clip = Clip::Stencil;
                m_f->glStencilMask(0x80);
                m_f->glClear(GL_STENCIL_BUFFER_BIT);
                m_f->glEnable(GL_STENCIL_TEST);
                m_f->glStencilFunc(GL_ALWAYS, 0x80, 0x80);
                m_f->glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
                m_f->glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
                m_f->glUniform1i(m_res->uMode, Solid);
                std::vector<GLfloat> xy;
                for (const QRect &r : rects) {
                    const auto x0 = static_cast<GLfloat>(r.x()), y0 = static_cast<GLfloat>(r.y());
                    const auto x1 = static_cast<GLfloat>(r.x() + r.width()),
                               y1 = static_cast<GLfloat>(r.y() + r.height());
                    xy.insert(xy.end(), {x0, y0, x1, y0, x1, y1, x0, y0, x1, y1, x0, y1});
                }
                drawVertices(GL_TRIANGLES, xy);
                m_f->glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            }
        }
    }
    if (m_clip == Clip::Empty)
        return false;
    if (m_clip == Clip::Scissor)
        m_f->glEnable(GL_SCISSOR_TEST);
    applyBlend();
    return true;
}

void QOpenGL2PaintEngine::applyBlend() {
    switch (m_state.compositionMode()) {
    case QPainter::CompositionMode_Clear:
        m_f->glBlendFunc(GL_ZERO, GL_ZERO);
        break;
    case QPainter::CompositionMode_Source:
        m_f->glBlendFunc(GL_ONE, GL_ZERO);
        break;
    default:
        m_f->glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        break;
    }
}

void QOpenGL2PaintEngine::setSolid(const QColor &color) {
    const double a = std::clamp(color.alphaF() * m_state.opacity(), 0.0, 1.0);
    m_f->glUniform1i(m_res->uMode, Solid);
    m_f->glUniform4f(m_res->uColor, static_cast<GLfloat>(color.redF() * a), static_cast<GLfloat>(color.greenF() * a),
                     static_cast<GLfloat>(color.blueF() * a), static_cast<GLfloat>(a));
}

bool QOpenGL2PaintEngine::setBrush(const QBrush &brush) {
    if (brush.style() == Qt::NoBrush)
        return false;
    if (!brush.gradient()) {
        setSolid(brush.color());
        return true;
    }
    raster::Shader shader;
    if (!raster::gradientShader(brush, m_state.transform(), m_state.opacity(), &shader))
        return false;
    std::vector<unsigned char> rgba(shader.table.size() * 4);
    for (std::size_t i = 0; i < shader.table.size(); ++i)
        toRgba(shader.table[i], rgba.data() + i * 4);
    uploadTexture(m_res->lut, static_cast<int>(shader.table.size()), 1, rgba, false);
    const raster::Mat &m = shader.toGradient;
    const GLfloat matrix[9] = {static_cast<GLfloat>(m.a), static_cast<GLfloat>(m.b), 0.0f,
                               static_cast<GLfloat>(m.c), static_cast<GLfloat>(m.d), 0.0f,
                               static_cast<GLfloat>(m.e), static_cast<GLfloat>(m.f), 1.0f};
    m_f->glUniformMatrix3fv(m_res->uToGradient, 1, GL_FALSE, matrix);
    m_f->glUniform2f(m_res->uP1, static_cast<GLfloat>(shader.p1.x), static_cast<GLfloat>(shader.p1.y));
    m_f->glUniform2f(m_res->uP2, static_cast<GLfloat>(shader.p2.x), static_cast<GLfloat>(shader.p2.y));
    m_f->glUniform1f(m_res->uRadius, static_cast<GLfloat>(shader.radius));
    m_f->glUniform1f(m_res->uAngle, static_cast<GLfloat>(shader.angle));
    m_f->glUniform1i(m_res->uSpread, shader.spread == raster::Spread::Repeat    ? 1
                                     : shader.spread == raster::Spread::Reflect ? 2
                                                                                : 0);
    m_f->glUniform1i(m_res->uMode, shader.conical ? Conical : shader.radial ? Radial : Linear);
    return true;
}

void QOpenGL2PaintEngine::uploadTexture(GLuint texture, int width, int height, const std::vector<unsigned char> &rgba,
                                        bool smooth) {
    m_f->glBindTexture(GL_TEXTURE_2D, texture);
    m_f->glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    m_f->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    const GLint filter = smooth ? GL_LINEAR : GL_NEAREST;
    m_f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    m_f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
    m_f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    m_f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

void QOpenGL2PaintEngine::drawVertices(GLenum mode, const std::vector<GLfloat> &xy, const std::vector<GLfloat> *uv) {
    if (xy.empty())
        return;
    m_f->glBindBuffer(GL_ARRAY_BUFFER, m_res->vbo);
    m_f->glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(xy.size() * sizeof(GLfloat)), xy.data(), GL_STREAM_DRAW);
    m_f->glEnableVertexAttribArray(0);
    m_f->glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    if (uv) {
        m_f->glBindBuffer(GL_ARRAY_BUFFER, m_res->uvbo);
        m_f->glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(uv->size() * sizeof(GLfloat)), uv->data(),
                          GL_STREAM_DRAW);
        m_f->glEnableVertexAttribArray(1);
        m_f->glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    } else {
        m_f->glDisableVertexAttribArray(1);
        m_f->glVertexAttrib2f(1, 0.0f, 0.0f);
    }
    m_f->glDrawArrays(mode, 0, static_cast<GLsizei>(xy.size() / 2));
}

// Stencil, then cover: the polygons count their windings into the low bits of the stencil buffer, and a rectangle
// over them paints where the count says inside, clearing the count as it goes.
void QOpenGL2PaintEngine::fillPolygons(const raster::Polygons &polygons, bool evenOdd) {
    if (polygons.points.empty())
        return;
    std::vector<GLfloat> fan;
    double x0 = polygons.points.front().x, x1 = x0, y0 = polygons.points.front().y, y1 = y0;
    std::size_t begin = 0;
    for (const std::size_t end : polygons.ends) {
        if (end - begin >= 3) {
            const raster::Pt &p0 = polygons.points[begin];
            for (std::size_t i = begin + 1; i + 1 < end; ++i) {
                const raster::Pt &a = polygons.points[i], &b = polygons.points[i + 1];
                fan.insert(fan.end(),
                           {static_cast<GLfloat>(p0.x), static_cast<GLfloat>(p0.y), static_cast<GLfloat>(a.x),
                            static_cast<GLfloat>(a.y), static_cast<GLfloat>(b.x), static_cast<GLfloat>(b.y)});
            }
        }
        for (std::size_t i = begin; i < end; ++i) {
            const raster::Pt &p = polygons.points[i];
            if (!std::isfinite(p.x) || !std::isfinite(p.y))
                return;
            x0 = std::min(x0, p.x);
            x1 = std::max(x1, p.x);
            y0 = std::min(y0, p.y);
            y1 = std::max(y1, p.y);
        }
        begin = end;
    }
    if (fan.empty())
        return;
    const double limit = 1e6;
    const auto bx0 = static_cast<GLfloat>(std::floor(std::max(x0, -limit)));
    const auto by0 = static_cast<GLfloat>(std::floor(std::max(y0, -limit)));
    const auto bx1 = static_cast<GLfloat>(std::ceil(std::min(x1, limit)));
    const auto by1 = static_cast<GLfloat>(std::ceil(std::min(y1, limit)));
    const std::vector<GLfloat> cover = {bx0, by0, bx1, by0, bx1, by1, bx0, by0, bx1, by1, bx0, by1};
    if (!m_hasStencil) {
        drawVertices(GL_TRIANGLES, fan);
        return;
    }
    m_f->glEnable(GL_STENCIL_TEST);
    m_f->glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    m_f->glStencilMask(0x7f);
    m_f->glStencilFunc(GL_ALWAYS, 0, 0xff);
    if (evenOdd) {
        m_f->glStencilOp(GL_KEEP, GL_KEEP, GL_INVERT);
    } else {
        m_f->glStencilOpSeparate(GL_FRONT, GL_KEEP, GL_KEEP, GL_INCR_WRAP);
        m_f->glStencilOpSeparate(GL_BACK, GL_KEEP, GL_KEEP, GL_DECR_WRAP);
    }
    drawVertices(GL_TRIANGLES, fan);
    m_f->glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    if (m_clip == Clip::Stencil)
        m_f->glStencilFunc(GL_LESS, 0x80, 0xff); // inside the clip (0x80) with a count
    else
        m_f->glStencilFunc(GL_NOTEQUAL, 0, 0x7f);
    m_f->glStencilOp(GL_ZERO, GL_ZERO, GL_ZERO);
    drawVertices(GL_TRIANGLES, cover);
    m_f->glDisable(GL_STENCIL_TEST);
}

void QOpenGL2PaintEngine::fillRects(const std::vector<QRect> &rects) {
    std::vector<GLfloat> xy;
    xy.reserve(rects.size() * 12);
    for (const QRect &in : rects) {
        const QRect r = in.normalized();
        if (r.isEmpty())
            continue;
        const auto x0 = static_cast<GLfloat>(r.x()), y0 = static_cast<GLfloat>(r.y());
        const auto x1 = static_cast<GLfloat>(r.x() + r.width()), y1 = static_cast<GLfloat>(r.y() + r.height());
        xy.insert(xy.end(), {x0, y0, x1, y0, x1, y1, x0, y0, x1, y1, x0, y1});
    }
    if (xy.empty())
        return;
    if (m_clip == Clip::Stencil) {
        m_f->glEnable(GL_STENCIL_TEST);
        m_f->glStencilMask(0);
        m_f->glStencilFunc(GL_EQUAL, 0x80, 0x80);
        m_f->glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
    }
    drawVertices(GL_TRIANGLES, xy);
    if (m_clip == Clip::Stencil)
        m_f->glDisable(GL_STENCIL_TEST);
}

void QOpenGL2PaintEngine::drawRects(const QRect *rects, int rectCount) {
    if (!prepare())
        return;
    std::vector<QRect> penRects, brushRects;
    raster::rectsOfRects(rects, rectCount, m_state.pen(), m_state.brush().style() != Qt::NoBrush,
                         [&](const QRect &r, bool pen) { (pen ? penRects : brushRects).push_back(r); });
    if (!brushRects.empty()) {
        setSolid(m_state.brush().color());
        fillRects(brushRects);
    }
    if (!penRects.empty()) {
        setSolid(m_state.pen().color());
        fillRects(penRects);
    }
}

void QOpenGL2PaintEngine::drawLines(const QLine *lines, int lineCount) {
    if (!prepare())
        return;
    std::vector<QRect> rects;
    raster::rectsOfLines(lines, lineCount, m_state.pen(), [&](const QRect &r, bool) { rects.push_back(r); });
    setSolid(m_state.pen().color());
    fillRects(rects);
}

void QOpenGL2PaintEngine::drawPoints(const QPoint *points, int pointCount) {
    if (!prepare())
        return;
    std::vector<QRect> rects;
    raster::rectsOfPoints(points, pointCount, m_state.pen(), [&](const QRect &r, bool) { rects.push_back(r); });
    setSolid(m_state.pen().color());
    fillRects(rects);
}

void QOpenGL2PaintEngine::drawEllipse(const QRect &rect) {
    if (!prepare())
        return;
    std::vector<QRect> penRects, brushRects;
    raster::rectsOfEllipse(rect, m_state.pen(), m_state.brush().style() != Qt::NoBrush,
                           [&](const QRect &r, bool pen) { (pen ? penRects : brushRects).push_back(r); });
    if (!brushRects.empty()) {
        setSolid(m_state.brush().color());
        fillRects(brushRects);
    }
    if (!penRects.empty()) {
        setSolid(m_state.pen().color());
        fillRects(penRects);
    }
}

void QOpenGL2PaintEngine::drawImage(const QRect &rect, const QImage &image, const QRect &sourceRect) {
    if (image.isNull() || !prepare())
        return;
    const QRect sr = sourceRect.isNull() ? image.rect() : sourceRect.normalized().intersected(image.rect());
    if (sr.isEmpty())
        return;
    const QRect target = rect.isNull() ? QRect(rect.topLeft(), sr.size()) : rect.normalized();
    if (target.isEmpty())
        return;
    std::vector<unsigned char> rgba(static_cast<std::size_t>(sr.width()) * static_cast<std::size_t>(sr.height()) * 4);
    unsigned char *out = rgba.data();
    const QImage::Format format = image.format();
    for (int y = sr.top(); y <= sr.bottom(); ++y) {
        const auto *line = reinterpret_cast<const QRgb *>( // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
            image.constScanLine(y));
        for (int x = sr.left(); x <= sr.right(); ++x, out += 4) {
            QRgb p = line[x];
            if (format == QImage::Format_RGB32)
                p |= 0xff000000u;
            else if (format == QImage::Format_ARGB32)
                p = qPremultiply(p);
            toRgba(p, out);
        }
    }
    uploadTexture(m_res->texture, sr.width(), sr.height(), rgba,
                  m_state.renderHints() & QPainter::SmoothPixmapTransform);
    m_f->glUniform1i(m_res->uMode, Image);
    m_f->glUniform1f(m_res->uOpacity, static_cast<GLfloat>(std::clamp(m_state.opacity(), 0.0, 1.0)));
    const auto x0 = static_cast<GLfloat>(target.x()), y0 = static_cast<GLfloat>(target.y());
    const auto x1 = static_cast<GLfloat>(target.x() + target.width()),
               y1 = static_cast<GLfloat>(target.y() + target.height());
    const std::vector<GLfloat> xy = {x0, y0, x1, y0, x1, y1, x0, y0, x1, y1, x0, y1};
    const std::vector<GLfloat> uv = {0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1};
    if (m_clip == Clip::Stencil) {
        m_f->glEnable(GL_STENCIL_TEST);
        m_f->glStencilMask(0);
        m_f->glStencilFunc(GL_EQUAL, 0x80, 0x80);
        m_f->glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
    }
    drawVertices(GL_TRIANGLES, xy, &uv);
    if (m_clip == Clip::Stencil)
        m_f->glDisable(GL_STENCIL_TEST);
}

void QOpenGL2PaintEngine::drawAlphaMask(const QPoint &topLeft, const uchar *mask, int width, int height,
                                        qsizetype stride) {
    if (width <= 0 || height <= 0 || !prepare())
        return;
    std::vector<unsigned char> rgba(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4);
    unsigned char *out = rgba.data();
    for (int y = 0; y < height; ++y) {
        const uchar *line = mask + static_cast<qsizetype>(y) * stride;
        for (int x = 0; x < width; ++x, out += 4)
            out[0] = out[1] = out[2] = out[3] = line[x];
    }
    uploadTexture(m_res->texture, width, height, rgba, false);
    setSolid(m_state.pen().color());
    m_f->glUniform1i(m_res->uMode, Mask);
    const auto x0 = static_cast<GLfloat>(topLeft.x()), y0 = static_cast<GLfloat>(topLeft.y());
    const auto x1 = x0 + static_cast<GLfloat>(width), y1 = y0 + static_cast<GLfloat>(height);
    const std::vector<GLfloat> xy = {x0, y0, x1, y0, x1, y1, x0, y0, x1, y1, x0, y1};
    const std::vector<GLfloat> uv = {0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1};
    if (m_clip == Clip::Stencil) {
        m_f->glEnable(GL_STENCIL_TEST);
        m_f->glStencilMask(0);
        m_f->glStencilFunc(GL_EQUAL, 0x80, 0x80);
        m_f->glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
    }
    drawVertices(GL_TRIANGLES, xy, &uv);
    if (m_clip == Clip::Stencil)
        m_f->glDisable(GL_STENCIL_TEST);
}

void QOpenGL2PaintEngine::drawPath(const QPainterPath &path) {
    if (path.isEmpty() || !prepare())
        return;
    raster::PathGeometry geometry;
    const bool fill = m_state.brush().style() != Qt::NoBrush;
    if (!raster::pathGeometry(path, m_state.transform(), fill, m_state.pen(),
                              m_state.renderHints() & QPainter::Antialiasing, &geometry))
        return;
    if (fill && setBrush(m_state.brush()))
        fillPolygons(geometry.fill, geometry.evenOdd);
    if (!geometry.stroke.points.empty()) {
        setSolid(m_state.pen().brush().color());
        fillPolygons(geometry.stroke, false);
    }
}

void QOpenGL2PaintEngine::drawTexture(QOpenGLContext *context, GLuint texture, const QSize &targetSize, bool blend) {
    Resources *res = resourcesFor(context);
    if (!res || targetSize.isEmpty())
        return;
    QOpenGLFunctions *f = context->functions();
    f->glViewport(0, 0, targetSize.width(), targetSize.height());
    f->glDisable(GL_STENCIL_TEST);
    f->glDisable(GL_SCISSOR_TEST);
    f->glDisable(GL_DEPTH_TEST);
    if (blend) {
        f->glEnable(GL_BLEND);
        f->glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    } else {
        f->glDisable(GL_BLEND);
    }
    f->glUseProgram(res->program);
    if (res->vao)
        res->bindVertexArray(res->vao);
    f->glUniform2f(res->uSize, static_cast<GLfloat>(targetSize.width()), static_cast<GLfloat>(targetSize.height()));
    f->glUniform1f(res->uFlip, 1.0f);
    f->glUniform1i(res->uMode, Image);
    f->glUniform1f(res->uOpacity, 1.0f);
    f->glUniform1i(res->uTex, 0);
    f->glActiveTexture(GL_TEXTURE0);
    f->glBindTexture(GL_TEXTURE_2D, texture);
    const auto w = static_cast<GLfloat>(targetSize.width()), h = static_cast<GLfloat>(targetSize.height());
    const GLfloat xy[] = {0, 0, w, 0, w, h, 0, 0, w, h, 0, h};
    const GLfloat uv[] = {0, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 0};
    f->glBindBuffer(GL_ARRAY_BUFFER, res->vbo);
    f->glBufferData(GL_ARRAY_BUFFER, sizeof xy, xy, GL_STREAM_DRAW);
    f->glEnableVertexAttribArray(0);
    f->glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    f->glBindBuffer(GL_ARRAY_BUFFER, res->uvbo);
    f->glBufferData(GL_ARRAY_BUFFER, sizeof uv, uv, GL_STREAM_DRAW);
    f->glEnableVertexAttribArray(1);
    f->glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    f->glDrawArrays(GL_TRIANGLES, 0, 6);
    f->glDisableVertexAttribArray(0);
    f->glDisableVertexAttribArray(1);
    f->glBindBuffer(GL_ARRAY_BUFFER, 0);
    f->glBindTexture(GL_TEXTURE_2D, 0);
    if (res->vao)
        res->bindVertexArray(0);
    f->glUseProgram(0);
    f->glDisable(GL_BLEND);
}
