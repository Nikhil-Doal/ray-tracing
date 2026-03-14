#pragma once
#include "light.h"
#include "../utils/math_utils.h"
#include "../utils/sampling_utils.h"

class SphereAreaLight : public Light {
public:
  Vec3 center;
  double radius;
  Vec3 color;
  double intensity;

  SphereAreaLight(const Vec3 &center, double radius, const Vec3 &color, double intensity)
    : center(center), radius(radius), color(color), intensity(intensity) {}

  LightSample sample(const Vec3 &from) const override {
    // sample a random point on the sphere surface
    Vec3 rand_dir = random_in_unit_sphere().normalize();
    Vec3 point_on_light = center + rand_dir * radius;

    Vec3 to_light = point_on_light - from;
    double dist2 = to_light.dot(to_light);
    double dist = sqrt(dist2);
    Vec3 to_light_dir = to_light / dist;

    // pdf in solid angle = dist² / (cos_theta_light * area)
    double cos_theta_light = fabs(rand_dir.dot(to_light_dir * -1));
    double area = 4.0 * PI * radius * radius;
    double pdf_val = (cos_theta_light < 1e-6) ? 0.0 : dist2 / (cos_theta_light * area);

    LightSample ls;
    ls.point = point_on_light;
    ls.normal = rand_dir;
    ls.emission = color * intensity;
    ls.pdf = pdf_val;
    return ls;
  }

  Vec3 emission(const Vec3 &from, const Vec3 &dir) const override {
    return color * intensity;
  }

  double pdf(const Vec3 &from, const Vec3 &dir) const override {
    // check if ray from 'from' in direction 'dir' hits the sphere
    Vec3 oc = from - center;
    double a = dir.dot(dir);
    double b = oc.dot(dir);
    double c = oc.dot(oc) - radius * radius;
    double discriminant = b*b - a*c;
    if (discriminant < 0) return 0.0;

    // solid angle pdf
    double val = 1.0 - radius*radius / oc.dot(oc);
    if (val <= 0) return 0.0; // inside or on sphere surface
    double cos_theta_max = sqrt(val);
    double solid_angle = 2.0 * PI * (1.0 - cos_theta_max);
    return 1.0 / solid_angle;
  }
};