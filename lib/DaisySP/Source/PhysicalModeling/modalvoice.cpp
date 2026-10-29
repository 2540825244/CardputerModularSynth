#include "modalvoice.h"
#include <algorithm>

using namespace daisysp;

void ModalVoice::Init(float sample_rate)
{
    sample_rate_ = sample_rate;
    aux_         = 0.f;
    dirty_       = true; // modsynth

    excitation_filter_.Init();
    resonator_.Init(0.015f, 24, sample_rate_);
    excitation_filter_.Init();
    dust_.Init();

    SetSustain(false);
    SetFreq(440.f);
    SetAccent(.3f);
    SetStructure(.6f);
    SetBrightness(.8f);
    SetDamping(.6f);
}

void ModalVoice::SetSustain(bool sustain)
{
    dirty_   = true; // modsynth
    sustain_ = sustain;
}

void ModalVoice::Trig()
{
    trig_ = true;
}

void ModalVoice::SetFreq(float freq)
{
    resonator_.SetFreq(freq);
    const float v = fclamp(freq / sample_rate_, 0.f, .25f);
    dirty_        = dirty_ || v != f0_; // modsynth
    f0_           = v;
}

void ModalVoice::SetAccent(float accent)
{
    const float v = fclamp(accent, 0.f, 1.f);
    dirty_        = dirty_ || v != accent_; // modsynth
    accent_       = v;
}

void ModalVoice::SetStructure(float structure)
{
    resonator_.SetStructure(structure);
}

void ModalVoice::SetBrightness(float brightness)
{
    const float v = fclamp(brightness, 0.f, 1.f);
    dirty_        = dirty_ || v != brightness_; // modsynth
    brightness_   = v;
    density_      = brightness_ * brightness_;
}

void ModalVoice::SetDamping(float damping)
{
    damping_ = fclamp(damping, 0.f, 1.f);
}

float ModalVoice::GetAux()
{
    return aux_;
}

float ModalVoice::Process(bool trigger)
{
    float brightness = brightness_ + 0.25f * accent_ * (1.0f - brightness_);
    float damping    = damping_ + 0.25f * accent_ * (1.0f - damping_);

    // modsynth: cutoff (a powf) and the excitation filter coefficients only after a change
    if(dirty_)
    {
        dirty_            = false;
        const float range = sustain_ ? 36.0f : 60.0f;
        const float f     = sustain_ ? 4.0f * f0_ : 2.0f * f0_;
        cutoff_           = fmin(
            f
                * powf(2.f,
                       kOneTwelfth
                           * ((brightness * (2.0f - brightness) - 0.5f) * range)),
            0.499f);
        q_ = sustain_ ? 0.7f : 1.5f;
        ResonatorSvf<1>::Coefs(&cutoff_, &q_, &ex_g_, &ex_r_plus_g_, &ex_h_);
    }
    const float cutoff = cutoff_;

    float temp = 0.f;
    // Synthesize excitation signal.
    if(sustain_)
    {
        const float dust_f = 0.00005f + 0.99995f * density_ * density_;
        dust_.SetDensity(dust_f);
        temp = dust_.Process() * (4.0f - dust_f * 3.0f) * accent_;
    }
    else if(trigger || trig_)
    {
        const float attenuation = 1.0f - damping * 0.5f;
        const float amplitude   = (0.12f + 0.08f * accent_) * attenuation;
        temp = amplitude * powf(2.f, kOneTwelfth * (cutoff * cutoff * 24.0f))
               / cutoff;
        trig_ = false;
    }

    const float one = 1.0f;
    excitation_filter_.ProcessCoefs<ResonatorSvf<1>::LOW_PASS, false>(
        &ex_g_, &ex_r_plus_g_, &ex_h_, &one, temp, &temp);

    aux_ = temp;

    resonator_.SetBrightness(brightness);
    resonator_.SetDamping(damping);

    return resonator_.Process(temp);
}