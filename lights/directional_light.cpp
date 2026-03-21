#include "directional_light.h"

DirectionalLight::DirectionalLight(const Vec3 &dir, const Vec3 &col, double intensity) : direction(dir.normalize()), color(col), intensity(intensity) {}

LightSample DirectionalLight::sample(const Vec3 &from) const{
  LightSample ls;
  ls.point = from + direction * 1e6;
  ls.normal = direction * -1;
  ls.emission = color * intensity;
  ls.pdf = 1.0;
  return ls;
}

Vec3 DirectionalLight::emission(const Vec3 &from, const Vec3 &dir) const{
  return Vec3(0,0,0);
}

double DirectionalLight::pdf(const Vec3 &from, const Vec3 &dir) const{
  return 0.0;
}