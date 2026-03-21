#pragma once 
#include "light.h"
#include "../utils/math_utils.h"

class PointLight : public Light {
public:
  Vec3 position;
  Vec3 color;
  double intensity;

  PointLight(const Vec3 &pos, const Vec3 &col, double intensity);
  LightSample sample(const Vec3 &from) const override;
  Vec3 emission(const Vec3 &from, const Vec3 &dir) const override;
  double pdf(const Vec3 &from, const Vec3 &dir) const override;
};

