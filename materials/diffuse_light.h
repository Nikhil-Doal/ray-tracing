#pragma once
#include "material.h"
#include "../textures/texture.h"
#include "../textures/solid_color.h"
#include <memory>

class DiffuseLight : public Material {
public:
  std::shared_ptr<Texture> emit_tex;

  DiffuseLight(std::shared_ptr<Texture> t) : emit_tex(t) {}
  DiffuseLight(const Vec3 &color) : emit_tex(std::make_shared<SolidColor>(color)) {}

  bool scatter(const Ray &ray_in, const HitRecord &rec, Vec3 &attenuation, Ray &scattered) const override {return false;} // lights don't scatter
  Vec3 emit(double u, double v, const Vec3 &point) const override {return emit_tex->value(u, v, point);}
  bool is_emissive() const override { return true; }
};