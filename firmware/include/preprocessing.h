#ifndef PREPROCESSING_H
#define PREPROCESSING_H

#include <cstdint>
#include <cstddef>

// Parametri pentru normalizarea datelor brute (StandardScaler) calculati in Python
// Ordinea este stricta: ax, ay, az, gx, gy, gz (6 caracteristici cinematice)
constexpr float kInputMean[6] = { 0.002488, 0.489056, -10.773833, -0.081192, 0.022033, -0.012787 };
constexpr float kInputScale[6] = { 0.518999, 0.466470, 0.318713, 0.036910, 0.066986, 0.008984 };

constexpr size_t kFeatureCount = 6;

#endif // PREPROCESSING_H