#include "metal.h"
#include "../textures/texture.h"
#include "lambertian.h"

Metal::Metal(std::shared_ptr<Texture> a, double f) : albedo(a), fuzz(f < 1 ? f : 1) {}

bool Metal::scatter(const Ray &ray_in, const HitRecord &rec, Vec3 &attenuation, Ray &scattered) const {
  Vec3 reflected = reflect(ray_in.direction.normalize(), rec.normal);

  scattered = Ray(rec.point, reflected + random_in_unit_sphere() * fuzz);
  attenuation = albedo->value(rec.u, rec.v, rec.point);

  return scattered.direction.dot(rec.normal) > 0;
}