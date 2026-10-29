#pragma once
#include "dsp_util.h"

// DaisySP's LadderFilter (Huovilainen new Moog model, van Hoesel v1.7: 4x linearly interpolated
// oversampling, six modes) with the same coefficients and response, rewritten for the ESP32-S3:
// the per-step tanh division is a recip0.s reciprocal, the stages are inline with the state in
// locals, and the mode mix is chosen once per sample instead of in every step. DaisySP's version
// measured ~960 cycles/sample on the device.
class Ladder {
   public:
    enum Mode { LP24, LP12, BP24, BP12, HP24, HP12 };  // DaisySP's FilterMode order

    void Init(float sampleRate) {
        sr_ = sampleRate;
        srIntRecip_ = 1.f / (sampleRate * kOs);
        for (int i = 0; i < 4; i++) z0_[i] = z1_[i] = 0.f;
        old_ = 0.f;
        mode_ = LP24;
        SetPassbandGain(0.5f);
        SetInputDrive(0.5f);
        SetFreq(5000.f);
        SetRes(0.2f);
    }
    void SetFreq(float hz) {
        hz = hz < 5.f ? 5.f : (hz > sr_ * 0.425f ? sr_ * 0.425f : hz);
        const float wc = hz * 2.f * 3.14159265f * srIntRecip_, wc2 = wc * wc;
        alpha_ = 0.9892f * wc - 0.4324f * wc2 + 0.1381f * wc * wc2 - 0.0202f * wc2 * wc2;
        qAdjust_ = 1.006f + 0.0536f * wc - 0.095f * wc2 - 0.05f * wc2 * wc2;
    }
    void SetRes(float res) { k_ = 4.f * (res < 0.f ? 0.f : (res > 1.8f ? 1.8f : res)); }
    void SetPassbandGain(float pbg) {
        pbg_ = pbg < 0.f ? 0.f : (pbg > 0.5f ? 0.5f : pbg);
        SetInputDrive(drive_);
    }
    void SetInputDrive(float d) {
        drive_ = d > 0.f ? d : 0.f;
        if (drive_ > 1.f) {
            drive_ = drive_ < 4.f ? drive_ : 4.f;
            driveScaled_ = 1.f + (drive_ - 1.f) * (1.f - pbg_);
        } else {
            driveScaled_ = drive_;
        }
    }
    void SetFilterMode(Mode m) { mode_ = m; }

    float Process(float in) {
        switch (mode_) {
            case LP24: return run<LP24>(in);
            case LP12: return run<LP12>(in);
            case BP24: return run<BP24>(in);
            case BP12: return run<BP12>(in);
            case HP24: return run<HP24>(in);
            default: return run<HP12>(in);
        }
    }

   private:
    static constexpr int kOs = 4;

    static float tanhApprox(float x) {
        if (x > 3.f) return 1.f;
        if (x < -3.f) return -1.f;
        const float x2 = x * x;
        return x * (27.f + x2) * recipPos(27.f + 9.f * x2);
    }

    template <int M>
    static float mix(float u, float s1, float s2, float s3, float s4) {
        switch (M) {
            case LP24: return s4;
            case LP12: return s2;
            case BP24: return (s2 + s4) * 4.f - s3 * 8.f;
            case BP12: return (s1 - s2) * 2.f;
            case HP24: return u + s4 - (s1 + s3) * 4.f + s2 * 6.f;
            default: return u + s2 - s1 * 2.f;
        }
    }

    template <int M>
    float run(float in) {
        const float input = in * driveScaled_;
        const float kq = k_ * qAdjust_, a = alpha_, pbg = pbg_;
        float a0 = z0_[0], a1 = z0_[1], a2 = z0_[2], a3 = z0_[3];
        float b0 = z1_[0], b1 = z1_[1], b2 = z1_[2], b3 = z1_[3];
        float total = 0.f, interp = 0.f;
        for (int os = 0; os < kOs; os++) {
            const float x = interp * old_ + (1.f - interp) * input;
            const float u = tanhApprox(x - (b3 - pbg * x) * kq);
            // stage: ft = s * (1 / 1.3) + (0.3 / 1.3) * z0 - z1; ft = ft * alpha + z1
            float f = (u * 0.76923077f + 0.23076923f * a0 - b0) * a + b0;
            a0 = u;
            b0 = f;
            const float s1 = f;
            f = (s1 * 0.76923077f + 0.23076923f * a1 - b1) * a + b1;
            a1 = s1;
            b1 = f;
            const float s2 = f;
            f = (s2 * 0.76923077f + 0.23076923f * a2 - b2) * a + b2;
            a2 = s2;
            b2 = f;
            const float s3 = f;
            f = (s3 * 0.76923077f + 0.23076923f * a3 - b3) * a + b3;
            a3 = s3;
            b3 = f;
            total += mix<M>(u, s1, s2, s3, f) * (1.f / kOs);
            interp += 1.f / kOs;
        }
        z0_[0] = a0, z0_[1] = a1, z0_[2] = a2, z0_[3] = a3;
        z1_[0] = b0, z1_[1] = b1, z1_[2] = b2, z1_[3] = b3;
        old_ = input;
        return total;
    }

    float sr_ = 48000.f, srIntRecip_ = 0.f, alpha_ = 1.f, k_ = 1.f, qAdjust_ = 1.f, pbg_ = 0.5f;
    float drive_ = 0.5f, driveScaled_ = 0.5f, old_ = 0.f;
    float z0_[4] = {}, z1_[4] = {};
    Mode mode_ = LP24;
};
