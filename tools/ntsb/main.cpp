// MIT License - see LICENSE
//
// ntsb makes the shader files (.nsb) that QShader reads: it compiles GLSL to SPIR-V with glslc (or takes a SPIR-V file)
// and packs the SPIR-V as the standard variant of the shader. It is Nt's replacement of Qt's qsb.
#include <QtCore/qcoreapplication.h>
#include <QtCore/qfile.h>
#include <QtCore/qfileinfo.h>
#include <QtCore/qprocess.h>
#include <QtCore/qtemporarydir.h>
#include <rhi/qshader.h>

#include <cstdio>

namespace {

int usage() {
    std::fputs("usage: ntsb [-s vert|frag|comp] [-o output.nsb] [--glslc path] input.{vert,frag,comp,glsl,spv}\n"
               "  Compiles the GLSL file with glslc to SPIR-V (Vulkan 1.0) and writes a .nsb file for QShader.\n"
               "  The stage comes from -s, or from the extension of the input. A .spv input is packed as it is.\n",
               stderr);
    return 2;
}

bool stageFor(const QString &name, QShader::Stage *stage) {
    if (name == QLatin1String("vert") || name == QLatin1String("vertex")) {
        *stage = QShader::VertexStage;
    } else if (name == QLatin1String("frag") || name == QLatin1String("fragment")) {
        *stage = QShader::FragmentStage;
    } else if (name == QLatin1String("comp") || name == QLatin1String("compute")) {
        *stage = QShader::ComputeStage;
    } else if (name == QLatin1String("geom")) {
        *stage = QShader::GeometryStage;
    } else if (name == QLatin1String("tesc")) {
        *stage = QShader::TessellationControlStage;
    } else if (name == QLatin1String("tese")) {
        *stage = QShader::TessellationEvaluationStage;
    } else {
        return false;
    }
    return true;
}

} // namespace

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    QString input, output, stageName, glslc = QStringLiteral("glslc");
    const QStringList args = app.arguments().mid(1);
    for (qsizetype i = 0; i < args.size(); ++i) {
        const QString &a = args.at(i);
        if ((a == QLatin1String("-o") || a == QLatin1String("-s") || a == QLatin1String("--glslc")) &&
            i + 1 < args.size()) {
            (a == QLatin1String("-o") ? output : a == QLatin1String("-s") ? stageName : glslc) = args.at(++i);
        } else if (!a.startsWith(QLatin1Char('-')) && input.isEmpty()) {
            input = a;
        } else {
            return usage();
        }
    }
    if (input.isEmpty())
        return usage();
    if (output.isEmpty())
        output = input + QStringLiteral(".nsb");

    const QFileInfo info(input);
    QString suffix = info.suffix();
    QShader::Stage stage = QShader::VertexStage;
    QByteArray spirv;
    if (suffix == QLatin1String("spv")) {
        // name.vert.spv: the stage is the extension before .spv
        if (stageName.isEmpty())
            stageName = QFileInfo(info.completeBaseName()).suffix();
        QFile file(input);
        if (!file.open(QIODevice::ReadOnly)) {
            std::fprintf(stderr, "ntsb: cannot read %s\n", qPrintable(input));
            return 1;
        }
        spirv = file.readAll();
    } else {
        if (stageName.isEmpty())
            stageName = suffix;
        QTemporaryDir dir;
        if (!dir.isValid())
            return 1;
        const QString spvPath = dir.filePath(QStringLiteral("out.spv"));
        QProcess process;
        QStringList glslcArgs = {QStringLiteral("--target-env=vulkan1.0"), QStringLiteral("-o"), spvPath};
        if (!stageFor(stageName, &stage)) {
            std::fprintf(stderr, "ntsb: the stage of %s is not known: give -s vert, frag or comp\n", qPrintable(input));
            return 1;
        }
        glslcArgs << (QStringLiteral("-fshader-stage=") + (stage == QShader::VertexStage     ? QStringLiteral("vert")
                                                           : stage == QShader::FragmentStage ? QStringLiteral("frag")
                                                           : stage == QShader::ComputeStage  ? QStringLiteral("comp")
                                                           : stage == QShader::GeometryStage ? QStringLiteral("geom")
                                                           : stage == QShader::TessellationControlStage
                                                               ? QStringLiteral("tesc")
                                                               : QStringLiteral("tese")))
                  << input;
        process.start(glslc, glslcArgs);
        if (!process.waitForFinished(60000) || process.exitCode() != 0) {
            std::fprintf(stderr, "ntsb: glslc failed:\n%s\n", process.readAllStandardError().constData());
            return 1;
        }
        QFile file(spvPath);
        if (!file.open(QIODevice::ReadOnly))
            return 1;
        spirv = file.readAll();
    }
    if (!stageFor(stageName, &stage)) {
        std::fprintf(stderr, "ntsb: the stage of %s is not known: give -s vert, frag or comp\n", qPrintable(input));
        return 1;
    }
    if (spirv.size() < 20 || spirv.size() % 4 != 0) {
        std::fprintf(stderr, "ntsb: %s is not SPIR-V\n", qPrintable(input));
        return 1;
    }
    QShader shader;
    shader.setStage(stage);
    shader.setShader(QShaderKey(QShader::SpirvShader, QShaderVersion(100)), QShaderCode(spirv, "main"));
    QFile out(output);
    if (!out.open(QIODevice::WriteOnly) || out.write(shader.serialized()) < 0) {
        std::fprintf(stderr, "ntsb: cannot write %s\n", qPrintable(output));
        return 1;
    }
    return 0;
}
