#pragma once
#include "../core/vec3.h"
#include "math_utils.h"

// ONB helper (orthonormal basis)
struct ONB {
  Vec3 u, v, w;

  void build_from_w(const Vec3 &n) {
    w = n.normalize();
    Vec3 a = (fabs(w.x) > 0.9) ? Vec3(0, 1, 0) : Vec3(1, 0, 0);
    v = w.cross(a).normalize();
    u = w.cross(v);
  }

  Vec3 local(const Vec3 &a) const {
    return u * a.x + v * a.y + w * a.z;
  }
};

inline Vec3 random_in_unit_sphere() {
  while (true) {
    // monte carlo sampling
    Vec3 p( 2 * random_double() - 1, 2 * random_double() - 1, 2 * random_double() - 1);

    if (p.dot(p) >= 1) continue;
    return p;
  }
}

// malley's method for cosine sampling
inline Vec3 random_cosine_direction() {
  double r1 = random_double();
  double r2 = random_double();

  double phi = 2 * PI * r1;
  double x = cos(phi) * sqrt(r2);
  double y = sin(phi) * sqrt(r2);
  double z = sqrt(1 - r2);

  return Vec3(x, y, z);
}