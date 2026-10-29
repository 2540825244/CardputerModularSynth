#include <math.h>
#include "svf.h"
#include "dsp.h"
#include "modsynth_fast.h"
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

using namespace daisysp;

void Svf::Init(float sample_rate)
{
    sr_        = sample_rate;
    fc_        = 200.0f;
    res_       = 0.5f;
    drive_     = 0.5f;
    pre_drive_ = 0.5f;
    freq_      = 0.25f;
    damp_      = 0.0f;
    notch_     = 0.0f;
    low_       = 0.0f;
    high_      = 0.0f;
    band_      = 0.0f;
    peak_      = 0.0f;
    input_     = 0.0f;
    out_notch_ = 0.0f;
    out_low_   = 0.0f;
    out_high_  = 0.0f;
    out_peak_  = 0.0f;
    out_band_  = 0.0f;
    fc_max_    = sr_ / 3.f;
    last_f_in_ = last_r_in_ = __builtin_nanf(""); // modsynth
    inv_2sr_                = 1.f / (sr_ * 2.0f); // modsynth
}

void Svf::Process(float in)
{
    input_ = in;
    // first pass
    notch_ = input_ - damp_ * band_;
    low_   = low_ + freq_ * band_;
    high_  = notch_ - low_;
    band_  = freq_ * high_ + band_ - drive_ * band_ * band_ * band_;
    // take first sample of output
    out_low_   = 0.5f * low_;
    out_high_  = 0.5f * high_;
    out_band_  = 0.5f * band_;
    out_peak_  = 0.5f * (low_ - high_);
    out_notch_ = 0.5f * notch_;
    // second pass
    notch_ = input_ - damp_ * band_;
    low_   = low_ + freq_ * band_;
    high_  = notch_ - low_;
    band_  = freq_ * high_ + band_ - drive_ * band_ * band_ * band_;
    // average second pass outputs
    out_low_ += 0.5f * low_;
    out_high_ += 0.5f * high_;
    out_band_ += 0.5f * band_;
    out_peak_ += 0.5f * (low_ - high_);
    out_notch_ += 0.5f * notch_;
}

void Svf::SetFreq(float f)
{
    if(f == last_f_in_) // modsynth: same input, same result
        return;
    last_f_in_ = f;
    fc_ = fclamp(f, 1.0e-6, fc_max_);
    // Set Internal Frequency for fc_
    freq_ = 2.0f
            * sinf(PI_F
                   * MIN(0.25f,
                         fc_ / (sr_ * 2.0f))); // fs*2 because double sampled
    // recalculate damp
    damp_ = MIN(2.0f * (1.0f - powf(res_, 0.25f)),
                MIN(2.0f, 2.0f / freq_ - freq_ * 0.5f));
}

void Svf::SetRes(float r)
{
    if(r == last_r_in_) // modsynth: same input, same result
        return;
    last_r_in_ = r;
    float res = fclamp(r, 0.f, 1.f);
    res_      = res;
    // recalculate damp
    damp_  = MIN(2.0f * (1.0f - powf(res_, 0.25f)),
                MIN(2.0f, 2.0f / freq_ - freq_ * 0.5f));
    drive_ = pre_drive_ * res_;
}

void Svf::SetDrive(float d)
{
    float drv  = fclamp(d * 0.1f, 0.f, 1.f);
    pre_drive_ = drv;
    drive_     = pre_drive_ * res_;
}

// modsynth: SetFreq(f); SetRes(r) without libm. sin(pi x) for x <= 0.25 by polynomial,
// powf(res, 0.25) by two rsqrt steps, 2 / freq_ by recip0.s.
void Svf::SetFreqResFast(float f, float r)
{
    last_f_in_ = last_r_in_ = __builtin_nanf(""); // the next exact call recomputes
    fc_   = fclamp(f, 1.0e-6, fc_max_);
    freq_ = 2.0f * FastSinHalfPi(PI_F * MIN(0.25f, fc_ * inv_2sr_));
    res_  = fclamp(r, 0.f, 1.f);
    damp_ = MIN(2.0f * (1.0f - FastPowQuarter(res_)),
                MIN(2.0f, 2.0f * FastRecip(freq_) - freq_ * 0.5f));
    drive_ = pre_drive_ * res_;
}
