// Unmodified Plaits/stmlib classes (commits in lib/plaits/UPSTREAM.md) as they were before the
// v1.3 modsynth speed edits, renamed to namespaces plaits_ref / stmlib_ref. Test-only.
// Copyright Emilie Gillet, MIT License: see lib/plaits/LICENSE and lib/plaits/stmlib/LICENSE.
#pragma once
#define TEST
#include "plaits/dsp/oscillator/oscillator.h"
#include "plaits/dsp/oscillator/sine_oscillator.h"
#include "stmlib/dsp/filter.h"
#include "stmlib/dsp/parameter_interpolator.h"
#include "stmlib/dsp/polyblep.h"

namespace stmlib_ref {
using namespace stmlib;
class Svf {
 public:
  Svf() { }
  ~Svf() { }
  
  void Init() {
    set_f_q<FREQUENCY_DIRTY>(0.01f, 100.0f);
    Reset();
  }
  
  void Reset() {
    state_1_ = state_2_ = 0.0f;
  }
  
  // Copy settings from another filter.
  inline void set(const Svf& f) {
    g_ = f.g();
    r_ = f.r();
    h_ = f.h();
  }

  // Set all parameters from LUT.
  inline void set_g_r_h(float g, float r, float h) {
    g_ = g;
    r_ = r;
    h_ = h;
  }
  
  // Set frequency and resonance coefficients from LUT, adjust remaining
  // parameter.
  inline void set_g_r(float g, float r) {
    g_ = g;
    r_ = r;
    h_ = 1.0f / (1.0f + r_ * g_ + g_ * g_);
  }

  // Set frequency from LUT, resonance in true units, adjust the rest.
  inline void set_g_q(float g, float resonance) {
    g_ = g;
    r_ = 1.0f / resonance;
    h_ = 1.0f / (1.0f + r_ * g_ + g_ * g_);
  }

  // Set frequency and resonance from true units. Various approximations
  // are available to avoid the cost of tanf.
  template<FrequencyApproximation approximation>
  inline void set_f_q(float f, float resonance) {
    g_ = OnePole::tan<approximation>(f);
    r_ = 1.0f / resonance;
    h_ = 1.0f / (1.0f + r_ * g_ + g_ * g_);
  }
  
  template<FilterMode mode>
  inline float Process(float in) {
    float hp, bp, lp;
    hp = (in - r_ * state_1_ - g_ * state_1_ - state_2_) * h_;
    bp = g_ * hp + state_1_;
    state_1_ = g_ * hp + bp;
    lp = g_ * bp + state_2_;
    state_2_ = g_ * bp + lp;
    
    if (mode == FILTER_MODE_LOW_PASS) {
      return lp;
    } else if (mode == FILTER_MODE_BAND_PASS) {
      return bp;
    } else if (mode == FILTER_MODE_BAND_PASS_NORMALIZED) {
      return bp * r_;
    } else if (mode == FILTER_MODE_HIGH_PASS) {
      return hp;
    }
  }
  
  template<FilterMode mode_1, FilterMode mode_2>
  inline void Process(float in, float* out_1, float* out_2) {
    float hp, bp, lp;
    hp = (in - r_ * state_1_ - g_ * state_1_ - state_2_) * h_;
    bp = g_ * hp + state_1_;
    state_1_ = g_ * hp + bp;
    lp = g_ * bp + state_2_;
    state_2_ = g_ * bp + lp;
    
    if (mode_1 == FILTER_MODE_LOW_PASS) {
      *out_1 = lp;
    } else if (mode_1 == FILTER_MODE_BAND_PASS) {
      *out_1 = bp;
    } else if (mode_1 == FILTER_MODE_BAND_PASS_NORMALIZED) {
      *out_1 = bp * r_;
    } else if (mode_1 == FILTER_MODE_HIGH_PASS) {
      *out_1 = hp;
    }

    if (mode_2 == FILTER_MODE_LOW_PASS) {
      *out_2 = lp;
    } else if (mode_2 == FILTER_MODE_BAND_PASS) {
      *out_2 = bp;
    } else if (mode_2 == FILTER_MODE_BAND_PASS_NORMALIZED) {
      *out_2 = bp * r_;
    } else if (mode_2 == FILTER_MODE_HIGH_PASS) {
      *out_2 = hp;
    }
  }
  
  template<FilterMode mode>
  inline void Process(const float* in, float* out, size_t size) {
    float hp, bp, lp;
    float state_1 = state_1_;
    float state_2 = state_2_;
    
    while (size--) {
      hp = (*in - r_ * state_1 - g_ * state_1 - state_2) * h_;
      bp = g_ * hp + state_1;
      state_1 = g_ * hp + bp;
      lp = g_ * bp + state_2;
      state_2 = g_ * bp + lp;
    
      float value;
      if (mode == FILTER_MODE_LOW_PASS) {
        value = lp;
      } else if (mode == FILTER_MODE_BAND_PASS) {
        value = bp;
      } else if (mode == FILTER_MODE_BAND_PASS_NORMALIZED) {
        value = bp * r_;
      } else if (mode == FILTER_MODE_HIGH_PASS) {
        value = hp;
      }
      
      *out = value;
      ++out;
      ++in;
    }
    state_1_ = state_1;
    state_2_ = state_2;
  }
  
  template<FilterMode mode>
  inline void ProcessAdd(const float* in, float* out, size_t size, float gain) {
    float hp, bp, lp;
    float state_1 = state_1_;
    float state_2 = state_2_;
    
    while (size--) {
      hp = (*in - r_ * state_1 - g_ * state_1 - state_2) * h_;
      bp = g_ * hp + state_1;
      state_1 = g_ * hp + bp;
      lp = g_ * bp + state_2;
      state_2 = g_ * bp + lp;
    
      float value;
      if (mode == FILTER_MODE_LOW_PASS) {
        value = lp;
      } else if (mode == FILTER_MODE_BAND_PASS) {
        value = bp;
      } else if (mode == FILTER_MODE_BAND_PASS_NORMALIZED) {
        value = bp * r_;
      } else if (mode == FILTER_MODE_HIGH_PASS) {
        value = hp;
      }
      
      *out += gain * value;
      ++out;
      ++in;
    }
    state_1_ = state_1;
    state_2_ = state_2;
  }
  
  template<FilterMode mode>
  inline void Process(const float* in, float* out, size_t size, size_t stride) {
    float hp, bp, lp;
    float state_1 = state_1_;
    float state_2 = state_2_;
    
    while (size--) {
      hp = (*in - r_ * state_1 - g_ * state_1 - state_2) * h_;
      bp = g_ * hp + state_1;
      state_1 = g_ * hp + bp;
      lp = g_ * bp + state_2;
      state_2 = g_ * bp + lp;
    
      float value;
      if (mode == FILTER_MODE_LOW_PASS) {
        value = lp;
      } else if (mode == FILTER_MODE_BAND_PASS) {
        value = bp;
      } else if (mode == FILTER_MODE_BAND_PASS_NORMALIZED) {
        value = bp * r_;
      } else if (mode == FILTER_MODE_HIGH_PASS) {
        value = hp;
      }
      
      *out = value;
      out += stride;
      in += stride;
    }
    state_1_ = state_1;
    state_2_ = state_2;
  }
  
  inline void ProcessMultimode(
      const float* in,
      float* out,
      size_t size,
      float mode) {
    float hp, bp, lp;
    float state_1 = state_1_;
    float state_2 = state_2_;
    float hp_gain = mode < 0.5f ? -mode * 2.0f : -2.0f + mode * 2.0f;
    float lp_gain = mode < 0.5f ? 1.0f - mode * 2.0f : 0.0f;
    float bp_gain = mode < 0.5f ? 0.0f : mode * 2.0f - 1.0f;
    while (size--) {
      hp = (*in - r_ * state_1 - g_ * state_1 - state_2) * h_;
      bp = g_ * hp + state_1;
      state_1 = g_ * hp + bp;
      lp = g_ * bp + state_2;
      state_2 = g_ * bp + lp;
      *out = hp_gain * hp + bp_gain * bp + lp_gain * lp;
      ++in;
      ++out;
    }
    state_1_ = state_1;
    state_2_ = state_2;
  }
  
  inline void ProcessMultimodeLPtoHP(
      const float* in,
      float* out,
      size_t size,
      float mode) {
    float hp, bp, lp;
    float state_1 = state_1_;
    float state_2 = state_2_;
    float hp_gain = std::min(-mode * 2.0f + 1.0f, 0.0f);
    float bp_gain = 1.0f - 2.0f * fabsf(mode - 0.5f);
    float lp_gain = std::max(1.0f - mode * 2.0f, 0.0f);
    while (size--) {
      hp = (*in - r_ * state_1 - g_ * state_1 - state_2) * h_;
      bp = g_ * hp + state_1;
      state_1 = g_ * hp + bp;
      lp = g_ * bp + state_2;
      state_2 = g_ * bp + lp;
      *out = hp_gain * hp + bp_gain * bp + lp_gain * lp;
      ++in;
      ++out;
    }
    state_1_ = state_1;
    state_2_ = state_2;
  }
  
  template<FilterMode mode>
  inline void Process(
      const float* in, float* out_1, float* out_2, size_t size,
      float gain_1, float gain_2) {
    float hp, bp, lp;
    float state_1 = state_1_;
    float state_2 = state_2_;
    
    while (size--) {
      hp = (*in - r_ * state_1 - g_ * state_1 - state_2) * h_;
      bp = g_ * hp + state_1;
      state_1 = g_ * hp + bp;
      lp = g_ * bp + state_2;
      state_2 = g_ * bp + lp;
    
      float value;
      if (mode == FILTER_MODE_LOW_PASS) {
        value = lp;
      } else if (mode == FILTER_MODE_BAND_PASS) {
        value = bp;
      } else if (mode == FILTER_MODE_BAND_PASS_NORMALIZED) {
        value = bp * r_;
      } else if (mode == FILTER_MODE_HIGH_PASS) {
        value = hp;
      }
      
      *out_1 += value * gain_1;
      *out_2 += value * gain_2;
      ++out_1;
      ++out_2;
      ++in;
    }
    state_1_ = state_1;
    state_2_ = state_2;
  }
  
  inline float g() const { return g_; }
  inline float r() const { return r_; }
  inline float h() const { return h_; }
  
 private:
  float g_;
  float r_;
  float h_;

  float state_1_;
  float state_2_;
  
  DISALLOW_COPY_AND_ASSIGN(Svf);
};
inline float SoftLimit(float x) { return x * (27.0f + x * x) / (27.0f + 9.0f * x * x); }
}  // namespace stmlib_ref

namespace plaits_ref {
using namespace plaits;
template<int batch_size>
class ResonatorSvf {
 public:
  ResonatorSvf() { }
  ~ResonatorSvf() { }
  
  void Init() {
    for (int i = 0; i < batch_size; ++i) {
      state_1_[i] = state_2_[i] = 0.0f;
    }
  }
  
  template<stmlib::FilterMode mode, bool add>
  void Process(
      const float* f,
      const float* q,
      const float* gain,
      const float* in,
      float* out,
      size_t size) {
    float g[batch_size];
    float r[batch_size];
    float r_plus_g[batch_size];
    float h[batch_size];
    float state_1[batch_size];
    float state_2[batch_size];
    float gains[batch_size];
    for (int i = 0; i < batch_size; ++i) {
      g[i] = stmlib::OnePole::tan<stmlib::FREQUENCY_FAST>(f[i]);
      r[i] = 1.0f / q[i];
      h[i] = 1.0f / (1.0f + r[i] * g[i] + g[i] * g[i]);
      r_plus_g[i] = r[i] + g[i];
      state_1[i] = state_1_[i];
      state_2[i] = state_2_[i];
      gains[i] = gain[i];
    }
    
    while (size--) {
      float s_in = *in++;
      float s_out = 0.0f;
      for (int i = 0; i < batch_size; ++i) {
        const float hp = (s_in - r_plus_g[i] * state_1[i] - state_2[i]) * h[i];
        const float bp = g[i] * hp + state_1[i];
        state_1[i] = g[i] * hp + bp;
        const float lp = g[i] * bp + state_2[i];
        state_2[i] = g[i] * bp + lp;
        s_out += gains[i] * ((mode == stmlib::FILTER_MODE_LOW_PASS) ? lp : bp);
      }
      if (add) {
        *out++ += s_out;
      } else {
        *out++ = s_out;
      }
    }
    for (int i = 0; i < batch_size; ++i) {
      state_1_[i] = state_1[i];
      state_2_[i] = state_2[i];
    }
  }
  
 private:
  float state_1_[batch_size];
  float state_2_[batch_size];
  
  DISALLOW_COPY_AND_ASSIGN(ResonatorSvf);
};

class VariableShapeOscillator {
 public:
  VariableShapeOscillator() { }
  ~VariableShapeOscillator() { }

  void Init() {
    master_phase_ = 0.0f;
    slave_phase_ = 0.0f;
    next_sample_ = 0.0f;
    previous_pw_ = 0.5f;
    high_ = false;
  
    master_frequency_ = 0.0f;
    slave_frequency_ = 0.01f;
    pw_ = 0.5f;
    waveshape_ = 0.0f;
    phase_modulation_ = 0.0f;
  }

  void Render(
      float frequency,
      float pw,
      float waveshape,
      float* out,
      size_t size) {
    Render<false, false>(0.0f, frequency, pw, waveshape, 0.0f, out, size);
  }
  
  void Render(
      float master_frequency,
      float frequency,
      float pw,
      float waveshape,
      float* out,
      size_t size) {
    Render<true, false>(
        master_frequency,
        frequency,
        pw,
        waveshape,
        0.0f,
        out, size);
  }
  
  template<bool enable_sync, bool output_phase>
  void Render(
      float master_frequency,
      float frequency,
      float pw,
      float waveshape,
      float phase_modulation_amount,
      float* out,
      size_t size) {
    if (master_frequency >= kMaxFrequency) {
      master_frequency = kMaxFrequency;
    }
    if (frequency >= kMaxFrequency) {
      frequency = kMaxFrequency;
    }
    
    if (frequency >= 0.25f) {
      pw = 0.5f;
    } else {
      CONSTRAIN(pw, frequency * 2.0f, 1.0f - 2.0f * frequency);
    }
    
    stmlib::ParameterInterpolator master_fm(
        &master_frequency_, master_frequency, size);
    stmlib::ParameterInterpolator fm(&slave_frequency_, frequency, size);
    stmlib::ParameterInterpolator pwm(&pw_, pw, size);
    stmlib::ParameterInterpolator waveshape_modulation(
        &waveshape_, waveshape, size);
    stmlib::ParameterInterpolator phase_modulation(
        &phase_modulation_, phase_modulation_amount, size);

    float next_sample = next_sample_;
    
    while (size--) {
      bool reset = false;
      bool transition_during_reset = false;
      float reset_time = 0.0f;

      float this_sample = next_sample;
      next_sample = 0.0f;
    
      const float master_frequency = master_fm.Next();
      const float slave_frequency = fm.Next();
      const float pw = pwm.Next();
      const float waveshape = waveshape_modulation.Next();
      
      const float square_amount = std::max(waveshape - 0.5f, 0.0f) * 2.0f;
      const float triangle_amount = std::max(1.0f - waveshape * 2.0f, 0.0f);
      
      const float slope_up = 1.0f / (pw);
      const float slope_down = 1.0f / (1.0f - pw);

      if (enable_sync) {
        master_phase_ += master_frequency;
        if (master_phase_ >= 1.0f) {
          master_phase_ -= 1.0f;
          reset_time = master_phase_ / master_frequency;
      
          float slave_phase_at_reset = slave_phase_ + \
              (1.0f - reset_time) * slave_frequency;
          reset = true;
          if (slave_phase_at_reset >= 1.0f) {
            slave_phase_at_reset -= 1.0f;
            transition_during_reset = true;
          }
          if (!high_ && slave_phase_at_reset >= pw) {
            transition_during_reset = true;
          }
          float value = ComputeNaiveSample(
              slave_phase_at_reset,
              pw,
              slope_up,
              slope_down,
              triangle_amount,
              square_amount);
          this_sample -= value * stmlib::ThisBlepSample(reset_time);
          next_sample -= value * stmlib::NextBlepSample(reset_time);
        }
      } else if (output_phase) {
        master_phase_ += master_frequency;
        if (master_phase_ >= 1.0f) {
          master_phase_ -= 1.0f;
        }
      }
      
      slave_phase_ += slave_frequency;
      while (transition_during_reset || !reset) {
        if (!high_) {
          if (slave_phase_ < pw) {
            break;
          }
          float t = (slave_phase_ - pw) / (previous_pw_ - pw + slave_frequency);
          float triangle_step = (slope_up + slope_down) * slave_frequency;
          triangle_step *= triangle_amount;
          
          this_sample += square_amount * stmlib::ThisBlepSample(t);
          next_sample += square_amount * stmlib::NextBlepSample(t);
          this_sample -= triangle_step * stmlib::ThisIntegratedBlepSample(t);
          next_sample -= triangle_step * stmlib::NextIntegratedBlepSample(t);
          high_ = true;
        }
      
        if (high_) {
          if (slave_phase_ < 1.0f) {
            break;
          }
          slave_phase_ -= 1.0f;
          float t = slave_phase_ / slave_frequency;
          float triangle_step = (slope_up + slope_down) * slave_frequency;
          triangle_step *= triangle_amount;

          this_sample -= (1.0f - triangle_amount) * stmlib::ThisBlepSample(t);
          next_sample -= (1.0f - triangle_amount) * stmlib::NextBlepSample(t);
          this_sample += triangle_step * stmlib::ThisIntegratedBlepSample(t);
          next_sample += triangle_step * stmlib::NextIntegratedBlepSample(t);
          high_ = false;
        }
      }
    
      if (enable_sync && reset) {
        slave_phase_ = reset_time * slave_frequency;
        high_ = false;
      }
    
      next_sample += ComputeNaiveSample(
          slave_phase_,
          pw,
          slope_up,
          slope_down,
          triangle_amount,
          square_amount);
      previous_pw_ = pw;
      
      if (output_phase) {
        float phasor = master_phase_;
        if (enable_sync) {
          // A trick to prevent discontinuities when the phase wraps around.
          const float w = 4.0f * (1.0f - master_phase_) * master_phase_;
          this_sample *= w * (2.0f - w);
          
          // Apply some asymmetry on the main phasor too.
          const float p2 = phasor * phasor;
          phasor += (p2 * p2 - phasor) * fabsf(pw - 0.5f) * 2.0f;
        }
        *out++ = phasor + phase_modulation.Next() * this_sample;
      } else {
        *out++ = (2.0f * this_sample - 1.0f);
      }
    }
    
    next_sample_ = next_sample;
  }
  
  inline void set_master_phase(float master_phase) {
    master_phase_ = master_phase;
  }
  
 private:
  inline float ComputeNaiveSample(
      float phase,
      float pw,
      float slope_up,
      float slope_down,
      float triangle_amount,
      float square_amount) const {
    float saw = phase;
    float square = phase < pw ? 0.0f : 1.0f;
    float triangle = phase < pw
        ? phase * slope_up
        : 1.0f - (phase - pw) * slope_down;
    saw += (square - saw) * square_amount;
    saw += (triangle - saw) * triangle_amount;
    return saw;
  }

  // Oscillator state.
  float master_phase_;
  float slave_phase_;
  float next_sample_;
  float previous_pw_;
  bool high_;

  // For interpolation of parameters.
  float master_frequency_;
  float slave_frequency_;
  float pw_;
  float waveshape_;
  float phase_modulation_;

  DISALLOW_COPY_AND_ASSIGN(VariableShapeOscillator);
};

class GrainletOscillator {
 public:
  GrainletOscillator() { }
  ~GrainletOscillator() { }

  void Init() {
    carrier_phase_ = 0.0f;
    formant_phase_ = 0.0f;
    next_sample_ = 0.0f;
  
    carrier_frequency_ = 0.0f;
    formant_frequency_ = 0.0f;
    carrier_shape_ = 0.0f;
    carrier_bleed_ = 0.0f;
  }
  
  void Render(
      float carrier_frequency,
      float formant_frequency,
      float carrier_shape,
      float carrier_bleed,
      float* out,
      size_t size) {
    if (carrier_frequency >= kMaxFrequency * 0.5f) {
      carrier_frequency = kMaxFrequency * 0.5f;
    }
    if (formant_frequency >= kMaxFrequency) {
      formant_frequency = kMaxFrequency;
    }
    
    stmlib::ParameterInterpolator carrier_frequency_modulation(
        &carrier_frequency_,
        carrier_frequency,
        size);
    stmlib::ParameterInterpolator formant_frequency_modulation(
        &formant_frequency_,
        formant_frequency,
        size);
    stmlib::ParameterInterpolator carrier_shape_modulation(
        &carrier_shape_,
        carrier_shape,
        size);
    stmlib::ParameterInterpolator carrier_bleed_modulation(
        &carrier_bleed_,
        carrier_bleed,
        size);

    float next_sample = next_sample_;
    
    while (size--) {
      bool reset = false;
      float reset_time = 0.0f;

      float this_sample = next_sample;
      next_sample = 0.0f;
    
      const float f0 = carrier_frequency_modulation.Next();
      const float f1 = formant_frequency_modulation.Next();
    
      carrier_phase_ += f0;
      reset = carrier_phase_ >= 1.0f;
      
      if (reset) {
        carrier_phase_ -= 1.0f;
        reset_time = carrier_phase_ / f0;
        float before = Grainlet(
            1.0f,
            formant_phase_ + (1.0f - reset_time) * f1,
            carrier_shape_modulation.subsample(1.0f - reset_time),
            carrier_bleed_modulation.subsample(1.0f - reset_time));

        float after = Grainlet(
            0.0f,
            0.0f,
            carrier_shape_modulation.subsample(1.0f),
            carrier_bleed_modulation.subsample(1.0f));

        float discontinuity = after - before;
        this_sample += discontinuity * stmlib::ThisBlepSample(reset_time);
        next_sample += discontinuity * stmlib::NextBlepSample(reset_time);
        formant_phase_ = reset_time * f1;
      } else {
        formant_phase_ += f1;
        if (formant_phase_ >= 1.0f) {
          formant_phase_ -= 1.0f;
        }
      }
      
      next_sample += Grainlet(
          carrier_phase_,
          formant_phase_,
          carrier_shape_modulation.Next(),
          carrier_bleed_modulation.Next());
      *out++ = this_sample;
    }
    
    next_sample_ = next_sample;
  }

 private:
  inline float Carrier(float phase, float shape) {
    shape *= 3.0f;
    MAKE_INTEGRAL_FRACTIONAL(shape);
    float t = 1.0f - shape_fractional;
    
    if (shape_integral == 0) {
      phase = phase * (1.0f + t * t * t * 15.0f);
      if (phase >= 1.0f) {
        phase = 1.0f;
      }
      phase += 0.75f;
    } else if (shape_integral == 1) {
      float breakpoint = 0.001f + 0.499f * t * t * t;
      if (phase < breakpoint) {
        phase *= (0.5f / breakpoint);
      } else {
        phase = 0.5f + (phase - breakpoint) * 0.5f / (1.0f - breakpoint);
      }
      phase += 0.75f;
    } else {
      t = 1.0f - t;
      phase = 0.25f + phase * (0.5f + t * t * t * 14.5f);
      if (phase >= 0.75f) phase = 0.75f;
    }
    return (Sine(phase) + 1.0f) * 0.25f;
  }

  inline float Grainlet(
      float carrier_phase,
      float formant_phase,
      float shape,
      float bleed) {
    float carrier = Carrier(carrier_phase, shape);
    float formant = Sine(formant_phase);
    return carrier * (formant + bleed) / (1.0f + bleed);
  }

  // Oscillator state.
  float carrier_phase_;
  float formant_phase_;
  float next_sample_;

  // For interpolation of parameters.
  float carrier_frequency_;
  float formant_frequency_;
  float carrier_shape_;
  float carrier_bleed_;
  
  DISALLOW_COPY_AND_ASSIGN(GrainletOscillator);
};
}  // namespace plaits_ref
