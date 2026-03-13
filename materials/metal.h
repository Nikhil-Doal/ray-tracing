#pragma once
#include "material.h"
#include "../core/vec3.h"
#include "../core/ray.h"
#include "../core/hittable.h"

class Texture;

class Metal : public Material {
public:
  Texture* albedo;
  double fuzz;
  Metal(Texture* a, double f);

  virtual bool scatter(const Ray &ray_in, const HitRecord &rec, Vec3 &attenuation, Ray &scattered) const override;
};