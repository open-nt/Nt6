// MIT License - see LICENSE
#include <QtCore/qfloat16.h>

void qFloatToFloat16(qfloat16 *out, const float *in, qsizetype length) noexcept {
    for (qsizetype i = 0; i < length; ++i)
        out[i] = qfloat16(in[i]);
}

void qFloatFromFloat16(float *out, const qfloat16 *in, qsizetype length) noexcept {
    for (qsizetype i = 0; i < length; ++i)
        out[i] = static_cast<float>(in[i]);
}
