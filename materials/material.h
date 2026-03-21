#pragma once
#include "../core/ray.h"
#include "../core/hittable.h"
#include "../textures/texture.h"
#include <memory>

class HitRecord;

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

  void apply_normal_maps(HitRecord &rec) const {
    if (!rec.has_tbn) return;
 
    Vec3 shading_normal = rec.normal;
    // geometric normal: before any perturbation, front-face corrected
    Vec3 geometric_normal = rec.front_face ? shading_normal : shading_normal * -1;
 
    // --- Bump map (height-field finite differences) ---
    if (bump_map) {
      double du = 1.0 / 1024.0, dv = 1.0 / 1024.0;
      auto sample_h = [&](double u, double v) {
        Vec3 c = bump_map->value(u, v, rec.point, rec.t);
        return (c.x + c.y + c.z) / 3.0;
      };
      double h_c = sample_h(rec.u, rec.v);
      double dh_du = (sample_h(rec.u + du, rec.v) - h_c) / du * bump_strength;
      double dh_dv = (sample_h(rec.u, rec.v + dv) - h_c) / dv * bump_strength;
 
      Vec3 mapped = (shading_normal - rec.tangent * dh_du - rec.bitangent * dh_dv).normalize();
      if (mapped.dot(geometric_normal) < 0.01) mapped = (mapped + geometric_normal * 0.1).normalize();
      shading_normal = mapped;
    }
 
    // --- Normal map (tangent-space RGB) ---
    if (normal_map) {
      Vec3 s = normal_map->value(rec.u, rec.v, rec.point, rec.t);
      double tn_x = (s.x * 2.0 - 1.0) * normal_map_strength;
      double tn_y = (s.y * 2.0 - 1.0) * normal_map_strength;
      double tn_z = (s.z * 2.0 - 1.0);
      Vec3 tn = Vec3(tn_x, tn_y, tn_z).normalize();
 
      Vec3 mapped = (rec.tangent * tn.x + rec.bitangent * tn.y + shading_normal * tn.z).normalize();
      if (mapped.dot(geometric_normal) < 0.01) mapped = (mapped + geometric_normal * 0.1).normalize();
      shading_normal = mapped;
    }
    rec.normal = rec.front_face ? shading_normal : shading_normal * -1;
  }
};