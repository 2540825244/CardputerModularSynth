#pragma once
#include <cmath>
#include <complex>

// RBJ cookbook biquads (EQ3 and its picture): design, run, and the magnitude at a frequency.
struct Biquad {
    float b0 = 1.f, b1 = 0.f, b2 = 0.f, a1 = 0.f, a2 = 0.f;  // normalised by a0
    float z1 = 0.f, z2 = 0.f;
    enum Kind { LowShelf, Peak, HighShelf };
    void design(Kind k, float hz, float q, float gainDb, float fs) {
        const float A = powf(10.f, gainDb / 40.f), w = 6.2831853f * hz / fs, cw = cosf(w), sw = sinf(w);
        const float al = sw / (2.f * q);
        float B0, B1, B2, A0, A1, A2;
        if (k == Peak) {
            B0 = 1.f + al * A; B1 = -2.f * cw; B2 = 1.f - al * A;
            A0 = 1.f + al / A; A1 = -2.f * cw; A2 = 1.f - al / A;
        } else {
            const float sA = 2.f * sqrtf(A) * al, s = k == LowShelf ? 1.f : -1.f;
            B0 = A * ((A + 1.f) - s * (A - 1.f) * cw + sA);
            B1 = s * 2.f * A * ((A - 1.f) - s * (A + 1.f) * cw);
            B2 = A * ((A + 1.f) - s * (A - 1.f) * cw - sA);
            A0 = (A + 1.f) + s * (A - 1.f) * cw + sA;
            A1 = -s * 2.f * ((A - 1.f) + s * (A + 1.f) * cw);
            A2 = (A + 1.f) + s * (A - 1.f) * cw - sA;
        }
        b0 = B0 / A0; b1 = B1 / A0; b2 = B2 / A0; a1 = A1 / A0; a2 = A2 / A0;
    }
    float process(float x) {  // transposed direct form II
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    float magDb(float hz, float fs) const {
        const std::complex<float> z = std::polar(1.f, -6.2831853f * hz / fs), z2c = z * z;
        const std::complex<float> n = b0 + b1 * z + b2 * z2c, d = 1.f + a1 * z + a2 * z2c;
        return 20.f * log10f(std::abs(n / d) + 1e-9f);
    }
};
