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

PointLight::PointLight(const Vec3 &pos, const Vec3 &col, double intensity) : position(pos), color(col), intensity(intensity) {}
LightSample PointLight::sample(const Vec3 &from) const {
  Vec3 dir = position - from;
  double dist2 = dir.dot(dir);
  LightSample ls;
  ls.point = position;
  ls.normal = (from - position).normalize();
  ls.emission = color * intensity / dist2; // inv. square falloff
  ls.pdf = 1.0; // delta dist, i.e only one sample pt
  return ls;
}
Vec3 PointLight::emission(const Vec3 &from, const Vec3 &dir) const {return Vec3(0,0,0);}
double PointLight::pdf(const Vec3 &from, const Vec3 &dir) const {return 0.0;}

