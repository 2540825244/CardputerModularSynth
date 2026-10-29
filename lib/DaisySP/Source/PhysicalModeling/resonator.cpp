#include "resonator.h"
#include <math.h>

using namespace daisysp;

void Resonator::Init(float position, int resolution, float sample_rate)
{
    sample_rate_ = sample_rate;
    dirty_       = true; // modsynth
    frequency_ = structure_ = brightness_ = damping_ = -1.f;

    SetFreq(440.f);
    SetStructure(.5f);
    SetBrightness(.5f);
    SetDamping(.5f);

    resolution_ = fmin(resolution, kMaxNumModes);

    for(int i = 0; i < resolution; ++i)
    {
        mode_amplitude_[i] = cos(position * TWOPI_F) * 0.25f;
    }

    for(int i = 0; i < kMaxNumModes / kModeBatchSize; ++i)
    {
        mode_filters_[i].Init();
    }
}

inline float NthHarmonicCompensation(int n, float stiffness)
{
    float stretch_factor = 1.0f;
    for(int i = 0; i < n - 1; ++i)
    {
        stretch_factor += stiffness;
        if(stiffness < 0.0f)
        {
            stiffness *= 0.93f;
        }
        else
        {
            stiffness *= 0.98f;
        }
    }
    return 1.0f / stretch_factor;
}

// modsynth: the coefficient half of the old Process(), same expressions in the same order;
// the coefficients of full batches go to g_/r_plus_g_/h_/a_.
void Resonator::UpdateCoefs()
{
    dirty_ = false;

    float stiffness  = CalcStiff(structure_);
    float f0         = frequency_ * NthHarmonicCompensation(3, stiffness);
    float brightness = brightness_;

    float harmonic       = f0;
    float stretch_factor = 1.0f;

    float input  = damping_ * 79.7f;
    float q_sqrt = powf(2.f, input * ratiofrac_);

    float q = 500.0f * q_sqrt * q_sqrt;
    brightness *= 1.0f - structure_ * 0.3f;
    brightness *= 1.0f - damping_ * 0.3f;
    float q_loss = brightness * (2.0f - brightness) * 0.85f + 0.15f;

    float mode_q[kModeBatchSize];
    float mode_f[kModeBatchSize];
    int   batch_counter = 0;
    batches_            = 0;

    for(int i = 0; i < resolution_; ++i)
    {
        float mode_frequency = harmonic * stretch_factor;
        if(mode_frequency >= 0.499f)
        {
            mode_frequency = 0.499f;
        }
        const float mode_attenuation = 1.0f - mode_frequency * 2.0f;

        mode_f[batch_counter] = mode_frequency;
        mode_q[batch_counter] = 1.0f + mode_frequency * q;
        a_[batches_ * kModeBatchSize + batch_counter]
            = mode_amplitude_[i] * mode_attenuation;
        ++batch_counter;

        if(batch_counter == kModeBatchSize)
        {
            batch_counter  = 0;
            const int base = batches_ * kModeBatchSize;
            ResonatorSvf<kModeBatchSize>::Coefs(
                mode_f, mode_q, &g_[base], &r_plus_g_[base], &h_[base]);
            ++batches_;
        }

        stretch_factor += stiffness;
        if(stiffness < 0.0f)
        {
            // Make sure that the partials do not fold back into negative frequencies.
            stiffness *= 0.93f;
        }
        else
        {
            // This helps adding a few extra partials in the highest frequencies.
            stiffness *= 0.98f;
        }
        harmonic += f0;
        q *= q_loss;
    }
}

float Resonator::Process(const float in)
{
    if(dirty_) // modsynth
        UpdateCoefs();
    float out = 0.f;
    for(int b = 0; b < batches_; ++b)
    {
        const int base = b * kModeBatchSize;
        mode_filters_[b]
            .ProcessCoefs<ResonatorSvf<kModeBatchSize>::BAND_PASS, true>(
                &g_[base], &r_plus_g_[base], &h_[base], &a_[base], in, &out);
    }
    return out;
}

// modsynth: the setters mark the coefficients dirty only on a change.
void Resonator::SetFreq(float freq)
{
    const float v = freq / sample_rate_;
    dirty_        = dirty_ || v != frequency_;
    frequency_    = v;
}

void Resonator::SetStructure(float structure)
{
    const float v = fmax(fmin(structure, 1.f), 0.f);
    dirty_        = dirty_ || v != structure_;
    structure_    = v;
}

void Resonator::SetBrightness(float brightness)
{
    const float v = fmax(fmin(brightness, 1.f), 0.f);
    dirty_        = dirty_ || v != brightness_;
    brightness_   = v;
}

void Resonator::SetDamping(float damping)
{
    const float v = fmax(fmin(damping, 1.f), 0.f);
    dirty_        = dirty_ || v != damping_;
    damping_      = v;
}

float Resonator::CalcStiff(float sig)
{
    if(sig < .25f)
    {
        sig = .25 - sig;
        sig = -sig * .25;
    }
    else if(sig < .3f)
    {
        sig = 0.f;
    }
    else if(sig < .9f)
    {
        sig -= .3f;
        sig *= stiff_frac_2;
    }
    else
    {
        sig -= .9f;
        sig *= 10; // div by .1
        sig *= sig;
        sig = 1.5 - cos(sig * PI_F) * .5f;
    }
    return sig;
}
