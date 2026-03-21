#pragma once
#include "../core/ray.h"
#include "../core/hittable.h"
#include "../textures/texture.h"
#include <memory>

struct HitRecord;

class Material {
public:
  std::shared_ptr<Texture> normal_map;  // optional normal map
  std::shared_ptr<Texture> bump_map;        // grayscale heightmap
  double normal_map_strength = 1.0;         // scale for normal map X/Y
  double bump_strength = 1.0;               // scale for bump map displacement

  virtual bool scatter(const Ray &ray_in, const HitRecord &rec, Vec3 &attenuation, Ray &scattered) const = 0;
  virtual double scattering_pdf(const Ray &ray_in, const HitRecord &rec, const Ray &scattered) const { return 0.0; }
  virtual Vec3 emit(double u, double v, const Vec3 &point) const { return Vec3(0,0,0); }
  virtual bool is_transmissive() const { return false; }
  virtual bool is_emissive() const { return false; }
  virtual Vec3 albedo_at(const HitRecord &rec) const { return Vec3(1,1,1); }

  void set_normal_map(std::shared_ptr<Texture> nm, double strength = 1.0) {
    normal_map = nm;
    normal_map_strength = strength;
  }
 
  void set_bump_map(std::shared_ptr<Texture> bm, double strength = 1.0) {
    bump_map = bm;
    bump_strength = strength;
  }
  virtual ~Material() = default;
};