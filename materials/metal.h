#pragma once
#include "material.h"
#include "../core/vec3.h"
#include "../core/ray.h"
#include "../core/hittable.h"
#include <memory>

class Texture;

class Metal : public Material {
public:
  std::shared_ptr<Texture> albedo;
  double fuzz;
  Metal(std::shared_ptr<Texture> a, double f);

  virtual bool scatter(const Ray &ray_in, const HitRecord &rec, Vec3 &attenuation, Ray &scattered) const override;
};