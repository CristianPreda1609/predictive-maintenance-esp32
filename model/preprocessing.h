#ifndef PREPROCESSING_H
#define PREPROCESSING_H

#include <cstdint>
#include <cstddef>

// Parametri pentru normalizarea datelor brute (StandardScaler) calculati in Python
// Ordinea este stricta: ax, ay, az, gx, gy, gz (6 caracteristici cinematice)
constexpr float kInputMean[6] = { 0.163085f, 1.257205f, -10.624784f, -0.082103f, 0.021795f, -0.010438f };
constexpr float kInputScale[6] = { 0.322586f, 0.400106f, 0.261893f, 0.023412f, 0.030094f, 0.005760f };

constexpr size_t kFeatureCount = 6;

#endif // PREPROCESSING_H