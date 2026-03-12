// Working with the lambertian reflectance
#pragma once
#include "material.h"
#include "../textures/texture.h"
#include <cstdlib>

Vec3 random_in_unit_sphere() {
  while (true) {
    // monte carlo sampling
    Vec3 p( 2 * random_double() - 1, 2 * random_double() - 1, 2 * random_double() - 1);

    if (p.dot(p) >= 1) continue;
    return p;
  }
}

Vec3 random_unit_vector() {
  return random_in_unit_sphere().normalize();
}

class Lambertian : public Material {
public:
  Texture* albedo;
  Lambertian (Texture* a) : albedo(a) {}
  bool scatter(const Ray &ray_in, const HitRecord &rec, Vec3 &attenuation, Ray &scattered) const override;
};
bool Lambertian::scatter(const Ray &ray_in, const HitRecord &rec, Vec3 &attenuation, Ray &scattered) const{
  Vec3 scatter_direction = rec.normal + random_unit_vector();

  scattered = Ray(rec.point, scatter_direction);
  attenuation = albedo -> value(rec.u, rec.v, rec.point);

  return true;
}
