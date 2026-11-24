// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qhash.h>
#include <QtCore/qlist.h>
#include <QtCore/qstring.h>

#include <memory>

class QShaderVersion {
public:
    enum Flag { GlslEs = 0x01 };
    Q_DECLARE_FLAGS(Flags, Flag)

    QShaderVersion() = default;
    QShaderVersion(int v, Flags f = Flags()) : m_version(v), m_flags(f) {}

    int version() const { return m_version; }
    void setVersion(int v) { m_version = v; }
    Flags flags() const { return m_flags; }
    void setFlags(Flags f) { m_flags = f; }

    friend bool operator==(const QShaderVersion &a, const QShaderVersion &b) noexcept {
        return a.m_version == b.m_version && a.m_flags == b.m_flags;
    }
    friend bool operator!=(const QShaderVersion &a, const QShaderVersion &b) noexcept { return !(a == b); }

private:
    int m_version = 100;
    Flags m_flags;
};
Q_DECLARE_OPERATORS_FOR_FLAGS(QShaderVersion::Flags)

class QShaderCode {
public:
    QShaderCode() = default;
    explicit QShaderCode(const QByteArray &code, const QByteArray &entryPoint = QByteArray("main"))
        : m_shader(code), m_entryPoint(entryPoint) {}

    QByteArray shader() const { return m_shader; }
    void setShader(const QByteArray &code) { m_shader = code; }
    QByteArray entryPoint() const { return m_entryPoint; }
    void setEntryPoint(const QByteArray &entryPoint) { m_entryPoint = entryPoint; }

private:
    QByteArray m_shader;
    QByteArray m_entryPoint = QByteArray("main");
};

class QShaderKey;

class QShader {
public:
    enum Stage {
        VertexStage,
        TessellationControlStage,
        TessellationEvaluationStage,
        GeometryStage,
        FragmentStage,
        ComputeStage
    };
    enum Source { SpirvShader, GlslShader, HlslShader, DxbcShader, MslShader, DxilShader, MetalLibShader, WgslShader };
    enum Variant {
        StandardShader = 0,
        BatchableVertexShader,
        UInt16IndexedVertexAsComputeShader,
        UInt32IndexedVertexAsComputeShader,
        NonIndexedVertexAsComputeShader
    };

    QShader();
    QShader(const QShader &other);
    QShader &operator=(const QShader &other);
    ~QShader();

    bool isValid() const;
    Stage stage() const;
    void setStage(Stage stage);

    QList<QShaderKey> availableShaders() const;
    QShaderCode shader(const QShaderKey &key) const;
    void setShader(const QShaderKey &key, const QShaderCode &shader);
    void removeShader(const QShaderKey &key);

    QByteArray serialized() const;
    static QShader fromSerialized(const QByteArray &data);

    friend bool operator==(const QShader &a, const QShader &b) noexcept;
    friend bool operator!=(const QShader &a, const QShader &b) noexcept { return !(a == b); }

private:
    struct Private;
    void detach();
    std::shared_ptr<Private> d;
};

class QShaderKey {
public:
    QShaderKey() = default;
    QShaderKey(QShader::Source s, const QShaderVersion &v, QShader::Variant var = QShader::StandardShader)
        : m_source(s), m_version(v), m_variant(var) {}

    QShader::Source source() const { return m_source; }
    void setSource(QShader::Source s) { m_source = s; }
    QShaderVersion sourceVersion() const { return m_version; }
    void setSourceVersion(const QShaderVersion &v) { m_version = v; }
    QShader::Variant sourceVariant() const { return m_variant; }
    void setSourceVariant(QShader::Variant v) { m_variant = v; }

    friend bool operator==(const QShaderKey &a, const QShaderKey &b) noexcept {
        return a.m_source == b.m_source && a.m_version == b.m_version && a.m_variant == b.m_variant;
    }
    friend bool operator!=(const QShaderKey &a, const QShaderKey &b) noexcept { return !(a == b); }
    friend bool operator<(const QShaderKey &a, const QShaderKey &b) noexcept {
        if (a.m_source != b.m_source)
            return a.m_source < b.m_source;
        if (a.m_version.version() != b.m_version.version())
            return a.m_version.version() < b.m_version.version();
        if (a.m_version.flags().toInt() != b.m_version.flags().toInt())
            return a.m_version.flags().toInt() < b.m_version.flags().toInt();
        return a.m_variant < b.m_variant;
    }

private:
    QShader::Source m_source = QShader::SpirvShader;
    QShaderVersion m_version;
    QShader::Variant m_variant = QShader::StandardShader;
};
