#pragma once
#include "material.h"

double reflectance(double cosine, double ref_idx);

class Texture;

class Dielectric : public Material {
public:
  Texture *tint;
  double ir; // the index of refraction
  Dielectric(double ir, Texture *tint = nullptr);
  virtual bool scatter(const Ray &ray_in, const HitRecord &rec, Vec3 &attenuation, Ray &scattered) const override;
};