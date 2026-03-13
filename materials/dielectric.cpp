#include "dielectric.h"
#include "../textures/texture.h"

Dielectric::Dielectric(double ir, std::shared_ptr<Texture> tint) : ir(ir), tint(tint) {}

bool Dielectric::scatter(const Ray &ray_in, const HitRecord &rec, Vec3 &attenuation, Ray &scattered) const{
  attenuation = tint ? tint->value(rec.u, rec.v, rec.point, rec.t) : Vec3(1.0, 1.0, 1.0);
  double refract_ratio = rec.front_face ? (1.0/ir) : ir;
  Vec3 unit_dir = ray_in.direction.normalize();

  double cos_theta = fmin(rec.normal.dot(unit_dir * -1), 1);
  double sin_theta = sqrt(1 - cos_theta*cos_theta);

  bool cannot_refract = refract_ratio * sin_theta > 1.0;

  Vec3 direction;

  if (cannot_refract || reflectance(cos_theta, refract_ratio) > random_double()) direction = reflect(unit_dir, rec.normal);
  else direction = refract(unit_dir, rec.normal, refract_ratio);
  
  scattered = Ray(rec.point, direction);
  return true;
}

// helpers
// using schlick's approximation to approximate the fresnel equations
double reflectance(double cosine, double ref_idx) {
  double r0 = (1 - ref_idx) / (1 + ref_idx);
  r0 = r0 * r0;
  return r0+ (1 - r0) * pow((1 - cosine), 5);
}