#include "dsp.h"
#include "analogsnaredrum.h"
#include "modsynth_fast.h"
#include <math.h>
#include <stdlib.h>

using namespace daisysp;

static const int kNumModes = 5;

void AnalogSnareDrum::Init(float sample_rate)
{
    sample_rate_ = sample_rate;

    trig_ = false;

    pulse_remaining_samples_ = 0;
    pulse_                   = 0.0f;
    pulse_height_            = 0.0f;
    pulse_lp_                = 0.0f;
    noise_envelope_          = 0.0f;
    sustain_gain_            = 0.0f;

    // modsynth: the per-sample constants, once. Setters mark the coefficients dirty on change.
    trigger_pulse_duration_ = 1.0e-3 * sample_rate_;
    pulse_decay_            = 1.0f - 1.0f / (float)(0.1e-3 * sample_rate_);
    dirty_                  = true;
    f0_ = tone_ = snappy_ = decay_ = -1.f;

    SetSustain(false);
    SetAccent(.6f);
    SetFreq(200.f);
    SetDecay(.3f);
    SetSnappy(.7f);
    SetTone(.5f);

    for(int i = 0; i < kNumModes; ++i)
    {
        resonator_[i].Init(sample_rate_);
        phase_[i] = 0.f;
    }
    noise_filter_.Init(sample_rate_);
}

/** Trigger the drum */
void AnalogSnareDrum::Trig()
{
    trig_ = true;
}

void AnalogSnareDrum::SetSustain(bool sustain)
{
    sustain_ = sustain;
}

void AnalogSnareDrum::SetAccent(float accent)
{
    accent_ = fclamp(accent, 0.f, 1.f);
}

// modsynth: the setters below mark the coefficients dirty only when the value changes.
void AnalogSnareDrum::SetFreq(float f0)
{
    f0             = f0 / sample_rate_;
    const float v  = fclamp(f0, 0.f, .4f);
    dirty_         = dirty_ || v != f0_;
    f0_            = v;
}

void AnalogSnareDrum::SetTone(float tone)
{
    const float v = fclamp(tone, 0.f, 1.f) * 2.f;
    dirty_        = dirty_ || v != tone_;
    tone_         = v;
}

void AnalogSnareDrum::SetDecay(float decay)
{
    dirty_ = dirty_ || decay != decay_;
    decay_ = decay;
}

void AnalogSnareDrum::SetSnappy(float snappy)
{
    const float v = fclamp(snappy, 0.f, 1.f);
    dirty_        = dirty_ || v != snappy_;
    snappy_       = v;
}

// modsynth: the coefficient half of the old Process(), same expressions in the same order.
void AnalogSnareDrum::UpdateCoefs()
{
    dirty_                = false;
    const float decay_xt  = decay_ * (1.0f + decay_ * (decay_ - 1.0f));
    const float q         = 2000.0f * powf(2.f, kOneTwelfth * decay_xt * 84.0f);
    noise_envelope_decay_ = 1.0f
                            - 0.0017f
                                  * powf(2.f,
                                         kOneTwelfth * (-decay_ * (50.0f + snappy_ * 10.0f)));
    exciter_leak_ = snappy_ * (2.0f - snappy_) * 0.1f;

    float snappy = snappy_ * 1.1f - 0.05f;
    snappy_gain_ = fclamp(snappy, 0.0f, 1.0f);

    float tone = tone_;

    static const float kModeFrequencies[kNumModes]
        = {1.00f, 2.00f, 3.18f, 4.16f, 5.62f};

    for(int i = 0; i < kNumModes; ++i)
    {
        mode_f_[i] = fmin(f0_ * kModeFrequencies[i], 0.499f);
        resonator_[i].SetFreq(mode_f_[i] * sample_rate_);
        resonator_[i].SetRes((mode_f_[i] * (i == 0 ? q : q * 0.25f)) * .2);
    }

    if(tone < 0.666667f)
    {
        tone *= 1.5f;
        mode_gain_[0] = 1.5f + (1.0f - tone) * (1.0f - tone) * 4.5f;
        mode_gain_[1] = 2.0f * tone + 0.15f;
        for(int i = 2; i < kNumModes; i++)
        {
            mode_gain_[i] = 0.f;
        }
    }
    else
    {
        tone          = (tone - 0.666667f) * 3.0f;
        mode_gain_[0] = 1.5f - tone * 0.5f;
        mode_gain_[1] = 2.15f - tone * 0.7f;
        for(int i = 2; i < kNumModes; ++i)
        {
            mode_gain_[i] = tone;
            tone *= tone;
        }
    }

    float f_noise = f0_ * 16.0f;
    noise_filter_.SetFreq(f_noise * sample_rate_);
    noise_filter_.SetRes(f_noise * 1.5f);
}

float AnalogSnareDrum::Process(bool trigger)
{
    // modsynth: coefficients come from UpdateCoefs(); per sample only the audio path runs.
    if(dirty_)
        UpdateCoefs();

    if(trigger || trig_)
    {
        trig_                    = false;
        pulse_remaining_samples_ = trigger_pulse_duration_;
        pulse_height_            = 3.0f + 7.0f * accent_;
        noise_envelope_          = 2.0f;
    }

    // Q45 / Q46
    float pulse = 0.0f;
    if(pulse_remaining_samples_)
    {
        --pulse_remaining_samples_;
        pulse = pulse_remaining_samples_ ? pulse_height_ : pulse_height_ - 1.0f;
        pulse_ = pulse;
    }
    else
    {
        pulse_ *= pulse_decay_;
        pulse = pulse_;
    }

    float sustain_gain_value = sustain_gain_ = accent_ * decay_;

    // R189 / C57 / R190 + C58 / C59 / R197 / R196 / IC14
    pulse_lp_ = fclamp(pulse_lp_, pulse, 0.75f);

    float shell = 0.0f;
#pragma GCC unroll 5 // modsynth: the five resonators' chains interleave
    for(int i = 0; i < kNumModes; ++i)
    {
        float excitation
            = i == 0 ? (pulse - pulse_lp_) + 0.006f * pulse : 0.026f * pulse;

        phase_[i] += mode_f_[i];
        phase_[i] = phase_[i] >= 1.f ? phase_[i] - 1.f : phase_[i];

        resonator_[i].ProcessBand(excitation); // modsynth: only Band() is read

        shell += mode_gain_[i]
                 * (sustain_
                        ? sin(phase_[i] * TWOPI_F) * sustain_gain_value * 0.25f
                        : resonator_[i].Band() + excitation * exciter_leak_);
    }
    shell = SoftClip(shell);

    // C56 / R194 / Q48 / C54 / R188 / D54
    float noise = 2.0f * FastRand() /* modsynth */ * kRandFrac - 1.0f;
    if(noise < 0.0f)
        noise = 0.0f;
    noise_envelope_ *= noise_envelope_decay_;
    noise *= (sustain_ ? sustain_gain_value : noise_envelope_) * snappy_gain_ * 2.0f;

    // C66 / R201 / C67 / R202 / R203 / Q49
    noise_filter_.ProcessBand(noise); // modsynth: only Band() is read
    noise = noise_filter_.Band();

    // IC13
    return noise + shell * (1.0f - snappy_gain_);
}

inline float AnalogSnareDrum::SoftLimit(float x)
{
    return x * (27.0f + x * x) * FastRecip(27.0f + 9.0f * x * x); // modsynth: no divide
}

inline float AnalogSnareDrum::SoftClip(float x)
{
    if(x < -3.0f)
    {
        return -1.0f;
    }
    else if(x > 3.0f)
    {
        return 1.0f;
    }
    else
    {
        return SoftLimit(x);
    }
}