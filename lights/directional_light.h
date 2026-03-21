#pragma once
#include "light.h"

class DirectionalLight : public Light {
public:
  Vec3 direction;
  Vec3 color;
  double intensity;

  DirectionalLight(const Vec3 &dir, const Vec3 &col, double intensity);
  LightSample sample(const Vec3 &from) const override;
  Vec3 emission(const Vec3 &from, const Vec3 &dir) const override;
  double pdf(const Vec3 &from, const Vec3 &dir) const override;
};