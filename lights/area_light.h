#pragma once
#include "light.h"
#include "../utils/math_utils.h"
#include "../utils/sampling_utils.h"

class SphereAreaLight : public Light {
public:
  Vec3 center;
  double radius;
  Vec3 color;
  double intensity;

  SphereAreaLight(const Vec3 &center, double radius, const Vec3 &color, double intensity);
  LightSample sample(const Vec3 &from) const override;
  Vec3 emission(const Vec3 &from, const Vec3 &dir) const override;
  double pdf(const Vec3 &from, const Vec3 &dir) const override;
};