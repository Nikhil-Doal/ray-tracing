#include "glossy.h"

Glossy::Glossy(std::shared_ptr<Texture> a, double roughness, double spec) : albedo(a), roughness(roughness), specular_strength(spec) {}

bool Glossy::scatter(const Ray &ray_in, const HitRecord &rec, Vec3 &attenuation, Ray &scattered) const {
  // Probabilistically choose diffuse or specular based on specular_strength
  bool do_specular = (random_double() < specular_strength);

  if (do_specular) {
    // GGX importance sampling around the reflected direction
    Vec3 unit_in = ray_in.direction.normalize();
    Vec3 reflected = reflect(unit_in, rec.normal);

    // Sample GGX lobe: perturb the reflected direction by roughness
    ONB onb;
    onb.build_from_w(reflected);
    
    double r1 = random_double();
    double r2 = random_double();
    double a2 = roughness * roughness;
    // GGX theta sampling: theta = atan(a * sqrt(r1) / sqrt(1-r1))
    double cos_theta = sqrt((1.0 - r1) / (1.0 + (a2 * a2 - 1.0) * r1));
    double sin_theta = sqrt(1.0 - cos_theta * cos_theta);
    double phi = 2.0 * PI * r2;
    
    Vec3 half_vec = onb.local(Vec3(
      sin_theta * cos(phi),
      sin_theta * sin(phi),
      cos_theta
    )).normalize();

    // Reflect incoming around the half vector
    Vec3 spec_dir = reflect(unit_in, half_vec);
    
    if (spec_dir.dot(rec.normal) <= 0) {
      // Below surface, fall back to diffuse
      ONB diffuse_onb;
      diffuse_onb.build_from_w(rec.normal);
      spec_dir = diffuse_onb.local(random_cosine_direction());
    }

    scattered = Ray(rec.point, spec_dir);
    // Fresnel-modulated specular color (simple Schlick with F0=0.04 for dielectric)
    double cos_i = fmax(0.0, rec.normal.dot(spec_dir.normalize()));
    double f0 = 0.04;
    double fresnel = f0 + (1.0 - f0) * pow(1.0 - cos_i, 5.0);
    attenuation = Vec3(fresnel, fresnel, fresnel);
  } else {
    // Diffuse (Lambertian)
    ONB onb;
    onb.build_from_w(rec.normal);
    Vec3 scatter_direction = onb.local(random_cosine_direction());
    scattered = Ray(rec.point, scatter_direction);
    attenuation = albedo->value(rec.u, rec.v, rec.point, rec.t);
  }
  return true;
}

double Glossy::scattering_pdf(const Ray &ray_in, const HitRecord &rec, const Ray &scattered) const {
  double cos_theta = rec.normal.dot(scattered.direction.normalize());
  double diffuse_pdf = cos_theta < 0 ? 0.0 : cos_theta / PI;

  // When the specular branch fires, we don't have a clean analytic pdf for
  // the GGX-reflected direction in the cosine hemisphere measure.
  // Return 0 for pure specular paths (handled as specular in the integrator)
  // and the diffuse pdf weighted by the diffuse probability otherwise.
  //
  // Old code had `+ 0.001` floor which caused fireflies from huge weight ratios.
  if (diffuse_pdf <= 0.0) return 0.0;
  return diffuse_pdf * (1.0 - specular_strength);
}

Vec3 Glossy::albedo_at(const HitRecord &rec) const {
  return albedo->value(rec.u, rec.v, rec.point, rec.t);
}