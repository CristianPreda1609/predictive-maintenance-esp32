#include "config.h"

// =====================================================================
//  config.cpp  ::  singura definitie a tabelelor declarate in config.h
// =====================================================================

const char* const class_names[NUM_CLASSES] =
    {"Repaus", "Normal", "Dezechilibru", "Frecare", "Slabit"};

const char* const feature_names[NUM_FEATURES] =
    {"ax", "ay", "az", "gx", "gy", "gz"};

const int kAllowedPwm[]  = {0, 160, 180, 200, 220, 240, 255};
const int kNumAllowedPwm = sizeof(kAllowedPwm) / sizeof(kAllowedPwm[0]);

// Verificare la COMPILARE: nu prinde un swap de ordine, dar prinde
// o clasa/feature lipsa sau in plus.
static_assert(sizeof(class_names) / sizeof(class_names[0]) == NUM_CLASSES,
              "class_names nu are exact NUM_CLASSES elemente");
static_assert(sizeof(feature_names) / sizeof(feature_names[0]) == NUM_FEATURES,
              "feature_names nu are exact NUM_FEATURES elemente");