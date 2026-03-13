#pragma once
#include "../core/vec3.h"
#include "math_utils.h"

inline Vec3 random_in_unit_sphere() {
  while (true) {
    // monte carlo sampling
    Vec3 p( 2 * random_double() - 1, 2 * random_double() - 1, 2 * random_double() - 1);

    if (p.dot(p) >= 1) continue;
    return p;
  }
}

inline Vec3 random_unit_vector() {
  return random_in_unit_sphere().normalize();
}