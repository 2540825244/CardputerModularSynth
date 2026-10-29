/*
Copyright (c) 2020 Electrosmith, Corp

Use of this source code is governed by an MIT-style
license that can be found in the LICENSE file or at
https://opensource.org/licenses/MIT.
*/

#pragma once
#ifndef DSY_SVF_H
#define DSY_SVF_H

namespace daisysp
{
/**      Double Sampled, Stable State Variable Filter

Credit to Andrew Simper from musicdsp.org

This is his "State Variable Filter (Double Sampled, Stable)"

Additional thanks to Laurent de Soras for stability limit, and 
Stefan Diedrichsen for the correct notch output

Ported by: Stephen Hensley
*/
class Svf
{
  public:
    Svf() {}
    ~Svf() {}
    /** Initializes the filter
        float sample_rate - sample rate of the audio engine being run, and the frequency that the Process function will be called.
    */
    void Init(float sample_rate);


    /** 
        Process the input signal, updating all of the outputs.
    */
    void Process(float in);

    /** modsynth: Process() computing only the band-pass output (Band()); the other outputs
        keep stale values. Band() is bit-identical to Process(). Inline, so a caller running
        several filters can interleave them.
    */
    inline void ProcessBand(float in)
    {
        input_    = in;
        notch_    = input_ - damp_ * band_;
        low_      = low_ + freq_ * band_;
        high_     = notch_ - low_;
        band_     = freq_ * high_ + band_ - drive_ * band_ * band_ * band_;
        out_band_ = 0.5f * band_;
        notch_    = input_ - damp_ * band_;
        low_      = low_ + freq_ * band_;
        high_     = notch_ - low_;
        band_     = freq_ * high_ + band_ - drive_ * band_ * band_ * band_;
        out_band_ += 0.5f * band_;
    }


    /** sets the frequency of the cutoff frequency. 
        f must be between 0.0 and sample_rate / 3
    */
    void SetFreq(float f);

    /** sets the resonance of the filter.
        Must be between 0.0 and 1.0 to ensure stability.
    */
    void SetRes(float r);

    /** sets the drive of the filter 
        affects the response of the resonance of the filter
    */
    void SetDrive(float d);

    /** modsynth: SetFreq(f) then SetRes(r) with fast math (polynomial sin, rsqrt, recip
        instead of sinf/powf/divide): for a cutoff that moves every sample. A few ulps off the
        exact pair.
    */
    void SetFreqResFast(float f, float r);
    /** lowpass output
        \return low pass output of the filter
    */
    inline float Low() { return out_low_; }
    /** highpass output
        \return high pass output of the filter
    */
    inline float High() { return out_high_; }
    /** bandpass output
        \return band pass output of the filter
    */
    inline float Band() { return out_band_; }
    /** notchpass output
        \return notch pass output of the filter
    */
    inline float Notch() { return out_notch_; }
    /** peak output
        \return peak output of the filter
    */
    inline float Peak() { return out_peak_; }

  private:
    float sr_, fc_, res_, drive_, freq_, damp_;
    float notch_, low_, high_, band_, peak_;
    float input_;
    float out_low_, out_high_, out_band_, out_peak_, out_notch_;
    float pre_drive_, fc_max_;
    // modsynth: the last SetFreq/SetRes arguments. A repeated value returns early: the result
    // only depends on (f, r), so skipping is exact (NaN until the first call).
    float last_f_in_, last_r_in_, inv_2sr_;
};
} // namespace daisysp

#endif
