// Copyright 2013 Olivier Gillet.
//
// Author: Olivier Gillet (ol.gillet@gmail.com)
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
// 
// See http://creativecommons.org/licenses/MIT/ for more information.
//
// -----------------------------------------------------------------------------
//
// LFO.

#include "lfo.h"

#include <cstdio>

#include "stmlib/utils/dsp.h"
#include "stmlib/utils/random.h"

#include "resources.h"

namespace batumi {

using namespace stmlib;

void Lfo::Init() {
  phase_ = 0;
  divided_phase_ = 0;
  initial_phase_ = 0;
  phase_increment_ = UINT32_MAX >> 8;
  divider_ = 1;
  divider_count_ = 0;
  level_ = UINT16_MAX;
}

void Lfo::Step() {
  phase_ += phase_increment_;
  if (phase_ < phase_increment_)
    divider_count_ = (divider_count_ + 1) % divider_;
  divided_phase_ = phase_ / divider_ +
    UINT32_MAX / divider_ * divider_count_;
}

uint32_t Lfo::ComputePhaseIncrement(int16_t pitch) {
  pitch_ = pitch;
  int16_t num_shifts = 0;
  while (pitch < 0) {
    pitch += kOctave;
    --num_shifts;
  }
  while (pitch >= kOctave) {
    pitch -= kOctave;
    ++num_shifts;
  }
  // Lookup phase increment
  uint32_t a = lut_increments[pitch >> 4];
  uint32_t b = lut_increments[(pitch >> 4) + 1];
  uint32_t phase_increment = a + ((b - a) * (pitch & 0xf) >> 4);
  return num_shifts >= 0
      ? phase_increment << num_shifts
      : phase_increment >> -num_shifts;
}

int16_t Lfo::ComputeSampleShape(LfoShape s) {
  s = SHAPE_SAW;				// TODO temp
  switch (s) {
  case SHAPE_TRIANGLE:
    return ComputeSampleTriangle();
  case SHAPE_SAW:
    return ComputeSampleSaw();
  case SHAPE_RAMP:
    return ComputeSampleRamp();
  case SHAPE_TRAPEZOID:
    return ComputeSampleTrapezoid();
  }
  return 0;
}

int16_t Lfo::ComputeSampleSine() {
  uint32_t phase = initial_phase_ + divided_phase_;
  int16_t sine = Interpolate1022(wav_sine, phase);
  return sine * level_ >> 16;
}

int16_t Lfo::ComputeSampleTriangle() {
  uint32_t phase = initial_phase_ + divided_phase_;
  int16_t tri = phase < 1UL << 31
      ? -32768 + (phase >> 15)
      :  32767 - (phase >> 15);
  int16_t pitch = divided_pitch_;
  int16_t x = 0;
  if (pitch > kPitch100Hz) {
    x = Interpolate1022(wav_tri100, phase);
  } else if (pitch > kPitch10Hz) {
    uint16_t balance = static_cast<int32_t>(pitch - kPitch10Hz) *
      65535L / (kPitch100Hz - kPitch10Hz);
    x = Crossfade1022(wav_tri10, wav_tri100, phase, balance);
  } else if (pitch > kPitch1Hz) {
    uint16_t balance = (pitch - kPitch1Hz) * 65535L / (kPitch10Hz - kPitch1Hz);
    int32_t a = tri;
    int32_t b = Interpolate1022(wav_tri10, phase);
    x = a + ((b - a) * static_cast<int32_t>(balance) >> 16);
  } else {
    x = tri;
  }
  return x * level_ >> 16;
}

int16_t Lfo::ComputeSampleSaw() {
  return -ComputeSampleRamp();
}

int16_t Lfo::ComputeSampleRamp() {
  uint32_t phase = initial_phase_ + divided_phase_;
  int16_t ramp = -32678 + (phase >> 16);
  int16_t pitch = divided_pitch_;
  int16_t x = 0;
  if (pitch > kPitch100Hz) {
    x = Interpolate1022(wav_saw100, phase);
  } else if (pitch > kPitch10Hz) {
    uint16_t balance = static_cast<int32_t>(pitch - kPitch10Hz) *
      65535L / (kPitch100Hz - kPitch10Hz);
    x = Crossfade1022(wav_saw10, wav_saw100, phase, balance);
  } else if (pitch > kPitch1Hz) {
    uint16_t balance = (pitch - kPitch1Hz) * 65535L / (kPitch10Hz - kPitch1Hz);
    int32_t a = ramp;
    int32_t b = Interpolate1022(wav_saw10, phase);
    x = a + ((b - a) * static_cast<int32_t>(balance) >> 16);
  } else {
    x = ramp;
  }
  return x * level_ >> 16;
}

int16_t Lfo::ComputeSampleTrapezoid() {
  uint32_t phase = initial_phase_ + divided_phase_;
  int16_t tri = phase < 1UL << 31 ? -32768 + (phase >> 15) :  32767 - (phase >> 15);
  int32_t trap = tri * 2;
  CONSTRAIN(trap, INT16_MIN, INT16_MAX);
  int16_t pitch = divided_pitch_;
  int16_t x = 0;
  if (pitch > kPitch100Hz) {
    x = Interpolate1022(wav_trap100, phase);
  } else if (pitch > kPitch10Hz) {
    uint16_t balance = static_cast<int32_t>(pitch - kPitch10Hz) *
      65535L / (kPitch100Hz - kPitch10Hz);
    x = Crossfade1022(wav_trap10, wav_trap100, phase, balance);
  } else if (pitch > kPitch1Hz) {
    uint16_t balance = (pitch - kPitch1Hz) * 65535L / (kPitch10Hz - kPitch1Hz);
    int32_t a = trap;
    int32_t b = Interpolate1022(wav_trap10, phase);
    x = a + ((b - a) * static_cast<int32_t>(balance) >> 16);
  } else {
    x = trap;
  }
  return x * level_ >> 16;
}

}  // namespace batumi