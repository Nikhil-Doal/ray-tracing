#pragma once
#include "material.h"
#include "../textures/texture.h"
#include <memory>

double reflectance(double cosine, double ref_idx);

class Dielectric : public Material {
public:
  std::shared_ptr<Texture> tint;
  double ir; // the index of refraction
  Dielectric(double ir, std::shared_ptr<Texture> tint = nullptr);
  virtual bool scatter(const Ray &ray_in, const HitRecord &rec, Vec3 &attenuation, Ray &scattered) const override;
  bool is_transmissive() const override { return true; }
  Vec3 albedo_at(const HitRecord &rec) const override {return tint ? tint->value(rec.u, rec.v, rec.point, rec.t) : Vec3(1,1,1);}
};