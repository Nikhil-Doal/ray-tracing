// Working with the lambertian reflectance
#pragma once
#include "material.h"
#include "../textures/texture.h"
#include "../utils/sampling_utils.h"
#include <cstdlib>
#include <memory>


Vec3 random_in_unit_sphere();
Vec3 random_unit_vector();

class Lambertian : public Material {
public:
  std::shared_ptr<Texture> albedo;
  Lambertian (std::shared_ptr<Texture> a);
  bool scatter(const Ray &ray_in, const HitRecord &rec, Vec3 &attenuation, Ray &scattered) const override;
};
