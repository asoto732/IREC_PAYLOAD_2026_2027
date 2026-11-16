#pragma once
#include <cmath>

struct VelocityFilter {
  float tauS;     // smoothing time constant in seconds: bigger = smoother, but slower to follow
  float maxGapS;  // a longer gap between samples than this restarts the filter
  bool primed = false;
  float lastAltitudeM = 0.0f;
  float velocityMps = NAN;

  VelocityFilter(float tau, float maxGap) : tauS(tau), maxGapS(maxGap) {}
  void reset() {
    primed = false;
    velocityMps = NAN;
  }
  // Returns the filtered velocity in m/s, or NAN if there is not enough valid data yet.
  float update(float altitudeM, float dtS) {
    if (std::isnan(altitudeM)) {
      reset();
      return NAN;
    }
    if (!primed || !(dtS > 0.0f) || dtS > maxGapS) {
      primed = true;  // start again from this sample
      lastAltitudeM = altitudeM;
      velocityMps = NAN;
      return NAN;
    }
    const float raw = (altitudeM - lastAltitudeM) / dtS;
    lastAltitudeM = altitudeM;
    if (std::isnan(velocityMps)) {
      velocityMps = raw;  // seed with the first raw value
    } else {
      const float alpha = dtS / (tauS + dtS);
      velocityMps += alpha * (raw - velocityMps);
    }
    return velocityMps;
  }
};
