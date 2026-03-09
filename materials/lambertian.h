// Working with the lambertian reflectance
#pragma once
#include "material.h"
#include <cstdlib>

Vec3 random_in_unit_sphere() {
  while (true) {
    // monte carlo sampling
    Vec3 p(
      rand()/(double)RAND_MAX*2-1,
      rand()/(double)RAND_MAX*2-1,
      rand()/(double)RAND_MAX*2-1
    );

    if (p.dot(p) >= 1) continue;
    return p;
  }
}

class Lambertian : public Material {
public:
  Vec3 albedo;
  Lambertian (const Vec3 &a) : albedo(a) {}
  bool scatter(const Ray &ray_in, const HitRecord &rec, Vec3 &attenuation, Ray &scattered) const override;
};
bool Lambertian::scatter(const Ray &ray_in, const HitRecord &rec, Vec3 &attenuation, Ray &scattered) const{
  Vec3 scatter_direction = rec.normal + random_in_unit_sphere();

  scattered = Ray(rec.point, scatter_direction);
  attenuation = albedo;

  return true;
}
