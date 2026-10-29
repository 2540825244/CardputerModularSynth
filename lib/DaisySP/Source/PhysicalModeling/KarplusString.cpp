#include "modsynth_fast.h" // modsynth
#include <cmath>
#include "dsp.h"
#include "KarplusString.h"
#include <stdlib.h>

using namespace daisysp;

void String::Init(float sample_rate)
{
    sample_rate_ = sample_rate;
    dirty_       = true; // modsynth

    SetFreq(440.f);
    non_linearity_amount_ = .5f;
    brightness_           = .5f;
    damping_              = .5f;

    string_.Init();
    stretch_.Init();
    Reset();

    SetFreq(440.f);
    SetDamping(.8f);
    SetNonLinearity(.1f);
    SetBrightness(.5f);

    crossfade_.Init();
}

void String::Reset()
{
    string_.Reset();
    stretch_.Reset();
    iir_damping_filter_.Init();

    dc_blocker_.Init(sample_rate_);

    dispersion_noise_ = 0.0f;
    curved_bridge_    = 0.0f;
    out_sample_[0] = out_sample_[1] = 0.0f;
    src_phase_                      = 0.0f;
}

float String::Process(const float in)
{
    if(dirty_) // modsynth
        UpdateCoefs();
    if(non_linearity_amount_ <= 0.0f)
    {
        non_linearity_amount_ *= -1;
        float ret = ProcessInternal<NON_LINEARITY_CURVED_BRIDGE>(in);
        non_linearity_amount_ *= -1;
        return ret;
    }
    else
    {
        return ProcessInternal<NON_LINEARITY_DISPERSION>(in);
    }
}

// modsynth: the setters mark the coefficients dirty only on a change.
void String::SetFreq(float freq)
{
    freq /= sample_rate_;
    const float v = fclamp(freq, 0.f, .25f);
    dirty_        = dirty_ || v != frequency_;
    frequency_    = v;
}

void String::SetNonLinearity(float non_linearity_amount)
{
    const float v         = fclamp(non_linearity_amount, 0.f, 1.f);
    dirty_                = dirty_ || v != non_linearity_amount_;
    non_linearity_amount_ = v;
}

void String::SetBrightness(float brightness)
{
    const float v = fclamp(brightness, 0.f, 1.f);
    dirty_        = dirty_ || v != brightness_;
    brightness_   = v;
}

void String::SetDamping(float damping)
{
    const float v = fclamp(damping, 0.f, 1.f);
    dirty_        = dirty_ || v != damping_;
    damping_      = v;
}

// modsynth: the coefficient half of the old ProcessInternal(), same expressions in the same
// order, for the non-linearity amount ProcessInternal() sees (Process() flips its sign).
void String::UpdateCoefs()
{
    dirty_ = false;
    const float nl = non_linearity_amount_ <= 0.0f ? -non_linearity_amount_
                                                   : non_linearity_amount_;
    float brightness = brightness_;

    float delay = 1.0f / frequency_;
    delay       = fclamp(delay, 4.f, kDelayLineSize - 4.0f);
    delay_      = delay;

    float src_ratio = delay * frequency_;
    src_full_       = src_ratio >= 0.9999f;
    src_ratio_      = src_full_ ? 1.0f : src_ratio;

    float damping_cutoff
        = fmin(12.0f + damping_ * damping_ * 60.0f + brightness * 24.0f, 84.0f);
    float damping_f
        = fmin(frequency_ * powf(2.f, damping_cutoff * kOneTwelfth), 0.499f);

    // Crossfade to infinite decay.
    if(damping_ >= 0.95f)
    {
        float to_infinite = 20.0f * (damping_ - 0.95f);
        brightness += to_infinite * (1.0f - brightness);
        damping_f += to_infinite * (0.4999f - damping_f);
        damping_cutoff += to_infinite * (128.0f - damping_cutoff);
    }

    float temp_f = damping_f;
    iir_damping_filter_.SetFrequency(temp_f);

    float ratio           = powf(2.f, damping_cutoff * kOneTwelfth);
    damping_compensation_ = 1.f - 2.f * atanf(1.f / ratio) / (TWOPI_F);

    stretch_point_ = nl * (2.0f - nl) * 0.225f;
    float stretch_correction = (160.0f / sample_rate_) * delay;
    stretch_correction_      = fclamp(stretch_correction, 1.f, 2.1f);

    float noise_amount_sqrt = nl > 0.75f ? 4.0f * (nl - 0.75f) : 0.0f;
    noise_amount_           = noise_amount_sqrt * noise_amount_sqrt * 0.1f;
    noise_filter_           = 0.06f + 0.94f * brightness * brightness;

    float bridge_curving_sqrt = nl;
    bridge_curving_ = bridge_curving_sqrt * bridge_curving_sqrt * 0.01f;

    ap_gain_ = -0.618f * nl / (0.15f + fabsf(nl));
}

template <String::StringNonLinearity non_linearity>
float String::ProcessInternal(const float in)
{
    // modsynth: coefficients from UpdateCoefs()
    float delay     = delay_;
    float src_ratio = src_ratio_;
    if(src_full_)
    {
        src_phase_ = 1.0f;
    }
    const float damping_compensation = damping_compensation_;
    const float stretch_point        = stretch_point_;
    const float stretch_correction   = stretch_correction_;
    const float noise_amount         = noise_amount_;
    const float noise_filter         = noise_filter_;
    const float bridge_curving       = bridge_curving_;
    const float ap_gain              = ap_gain_;

    src_phase_ += src_ratio;
    if(src_phase_ > 1.0f)
    {
        src_phase_ -= 1.0f;

        delay   = delay * damping_compensation;
        float s = 0.0f;

        if(non_linearity == NON_LINEARITY_DISPERSION)
        {
            float noise = FastRand() /* modsynth */ * kRandFrac - 0.5f;
            fonepole(dispersion_noise_, noise, noise_filter);
            delay *= 1.0f + dispersion_noise_ * noise_amount;
        }
        else
        {
            delay *= 1.0f - curved_bridge_ * bridge_curving;
        }

        if(non_linearity == NON_LINEARITY_DISPERSION)
        {
            float ap_delay   = delay * stretch_point;
            float main_delay = delay
                               - ap_delay * (0.408f - stretch_point * 0.308f)
                                     * stretch_correction;
            if(ap_delay >= 4.0f && main_delay >= 4.0f)
            {
                s = string_.Read(main_delay);
                s = stretch_.Allpass(s, ap_delay, ap_gain);
            }
            else
            {
                s = string_.ReadHermite(delay);
            }
        }
        else
        {
            s = string_.ReadHermite(delay);
        }

        if(non_linearity == NON_LINEARITY_CURVED_BRIDGE)
        {
            float value    = fabsf(s) - 0.025f;
            float sign     = s > 0.0f ? 1.0f : -1.5f;
            curved_bridge_ = (fabsf(value) + value) * sign;
        }

        s += in;
        s = fclamp(s, -20.f, +20.f);

        s = dc_blocker_.Process(s);

        s = iir_damping_filter_.Process(s);
        string_.Write(s);

        out_sample_[1] = out_sample_[0];
        out_sample_[0] = s;
    }

    crossfade_.SetPos(src_phase_);
    return crossfade_.Process(out_sample_[1], out_sample_[0]);
}
