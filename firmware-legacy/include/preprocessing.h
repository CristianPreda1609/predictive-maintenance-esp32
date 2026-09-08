#ifndef PREPROCESSING_H
#define PREPROCESSING_H

#include <cstdint>
#include <cstddef>

// Parametri pentru normalizarea datelor brute (StandardScaler) calculati in Python
// Ordinea este stricta: ax, ay, az, gx, gy, gz (6 caracteristici cinematice)
constexpr float kInputMean[6] = { 0.163085, 1.257205, -10.624784, -0.082103, 0.021795, -0.010438 };
constexpr float kInputScale[6] = { 0.322586, 0.400106, 0.261893, 0.023412, 0.030094, 0.005760 };

constexpr size_t kFeatureCount = 6;

#endif // PREPROCESSING_H