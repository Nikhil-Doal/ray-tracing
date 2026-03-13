#include "lambertian.h"

Lambertian::Lambertian (std::shared_ptr<Texture> a) : albedo(a) {}

bool Lambertian::scatter(const Ray &ray_in, const HitRecord &rec, Vec3 &attenuation, Ray &scattered) const{
  ONB onb;
  onb.build_from_w(rec.normal);

  Vec3 scatter_direction = onb.local(random_cosine_direction());
  
  scattered = Ray(rec.point, scatter_direction);
  attenuation = albedo -> value(rec.u, rec.v, rec.point, rec.t);
  
  return true;
}