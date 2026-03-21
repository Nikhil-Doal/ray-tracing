#pragma once
#include "material.h"
#include "../utils/sampling_utils.h"
#include "../textures/texture.h"
#include <memory>

class Metal : public Material {
public:
  std::shared_ptr<Texture> albedo;
  double fuzz;
  Metal(std::shared_ptr<Texture> a, double f);

  virtual bool scatter(const Ray &ray_in, const HitRecord &rec_in, Vec3 &attenuation, Ray &scattered) const override;
  Vec3 albedo_at(const HitRecord &rec) const override {return albedo->value(rec.u, rec.v, rec.point, rec.t);}
};