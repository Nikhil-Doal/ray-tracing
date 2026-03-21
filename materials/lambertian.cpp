#include "lambertian.h"

Lambertian::Lambertian (std::shared_ptr<Texture> a) : albedo(a) {}

bool Lambertian::scatter(const Ray &ray_in, const HitRecord &rec_in, Vec3 &attenuation, Ray &scattered) const{
  HitRecord rec = rec_in;
  apply_normal_maps(rec);
  
  ONB onb;
  onb.build_from_w(rec.normal);
  Vec3 scatter_direction = onb.local(random_cosine_direction());
  
  scattered = Ray(rec.point, scatter_direction);
  attenuation = albedo -> value(rec.u, rec.v, rec.point, rec.t);
  
  return true;
}