#pragma once
#include "../core/ray.h"
#include "../core/hittable.h"
#include "../textures/texture.h"
#include <memory>

class HitRecord;

class Material {
public:
  std::shared_ptr<Texture> normal_map;  // optional normal map

  virtual bool scatter(const Ray &ray_in, const HitRecord &rec, Vec3 &attenuation, Ray &scattered) const = 0;
  virtual double scattering_pdf(const Ray &ray_in, const HitRecord &rec, const Ray &scattered) const { return 0.0; }
  virtual Vec3 emit(double u, double v, const Vec3 &point) const { return Vec3(0,0,0); }
  virtual bool is_transmissive() const { return false; }
  virtual bool is_emissive() const { return false; }
  virtual Vec3 albedo_at(const HitRecord &rec) const { return Vec3(1,1,1); }

  void set_normal_map(std::shared_ptr<Texture> nm) { normal_map = nm; }
};