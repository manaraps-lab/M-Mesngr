// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 MANARA Messenger contributors.
//
// Measures the live speed of a transfer from progress samples and estimates
// the time left. The speed is an exponentially smoothed instantaneous rate
// (time constant 2 s), so it follows real changes in the network quickly
// without jumping on every sample. Pure arithmetic: no Qt dependency, no clock
// of its own (the caller passes timestamps), which keeps it unit-testable.
#pragma once

#include <cmath>
#include <cstdint>

class TransferMeter {
public:
  // Samples closer together than this are folded into the next one.
  static constexpr int64_t kMinIntervalMs = 100;
  static constexpr double kTimeConstantMs = 2000.0;

  void reset() {
    lastMs = -1;
    lastPos = 0;
    smoothed = 0.0;
    haveSpeed = false;
  }

  // Feeds one sample: `position` bytes transferred at time `nowMs`.
  void update(int64_t position, int64_t nowMs) {
    if(lastMs < 0) {
      lastMs = nowMs;
      lastPos = position;
      return;
    }
    const int64_t dt = nowMs - lastMs;
    if(dt < kMinIntervalMs)
      return;
    if(position < lastPos) {  // restarted or rewound: forget the history
      smoothed = 0.0;
      haveSpeed = false;
      lastMs = nowMs;
      lastPos = position;
      return;
    }
    const double instant = static_cast<double>(position - lastPos) * 1000.0 / static_cast<double>(dt);
    if(!haveSpeed) {
      smoothed = instant;
      haveSpeed = true;
    } else {
      const double alpha = 1.0 - std::exp(-static_cast<double>(dt) / kTimeConstantMs);
      smoothed += alpha * (instant - smoothed);
    }
    lastMs = nowMs;
    lastPos = position;
  }

  // Current smoothed speed in bytes per second (0 until two samples exist).
  int64_t bytesPerSecond() const { return static_cast<int64_t>(smoothed); }

  // Whole seconds left for `remainingBytes`; -1 when the speed is unknown.
  int64_t secondsLeft(int64_t remainingBytes) const {
    if(!haveSpeed || smoothed < 1.0 || remainingBytes < 0)
      return -1;
    return static_cast<int64_t>(std::ceil(static_cast<double>(remainingBytes) / smoothed));
  }

private:
  int64_t lastMs = -1;
  int64_t lastPos = 0;
  double smoothed = 0.0;
  bool haveSpeed = false;
};
