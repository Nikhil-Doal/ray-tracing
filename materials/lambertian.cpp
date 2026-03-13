#include "lambertian.h"

Lambertian::Lambertian (std::shared_ptr<Texture> a) : albedo(a) {}

bool Lambertian::scatter(const Ray &ray_in, const HitRecord &rec, Vec3 &attenuation, Ray &scattered) const{
  Vec3 scatter_direction = rec.normal + random_unit_vector();
  
  scattered = Ray(rec.point, scatter_direction);
  attenuation = albedo -> value(rec.u, rec.v, rec.point, rec.t);
  
  return true;
}