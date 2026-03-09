#pragma once
#include "vec3.h"
#include "ray.h"
#include <cmath>
#include "../utils/math_utils.h"

class Camera {
public:
  Vec3 origin;
  Vec3 lower_left_corner;
  Vec3 horizontal;
  Vec3 vertical;
  Vec3 u, v, w;
  double lens_radius;
  Camera(Vec3 lookfrom, Vec3 lookat, Vec3 vup, double vfov, double aspect_ratio, double aperture, double focus_dist);  
  Ray get_ray(double s, double t) const;
};
Camera::Camera(Vec3 lookfrom, Vec3 lookat, Vec3 vup, double vfov, double aspect_ratio, double aperture, double focus_dist) {
  double theta = degrees_to_radians(vfov);
  double h = tan(theta/2);

  double viewport_height = 2.0 * h;
  double viewport_width = aspect_ratio * viewport_height;
  w = (lookfrom - lookat).normalize();
  u = (vup.cross(w)).normalize();
  v = w.cross(u);

  origin = lookfrom;
  horizontal = u * focus_dist * viewport_width;
  vertical = v * focus_dist * viewport_height;

  lower_left_corner = origin - horizontal/2 - vertical/2 - w * focus_dist;

  lens_radius = aperture / 2;
}

Ray Camera::get_ray(double s, double t) const {
  Vec3 rd = random_in_unit_disk() * lens_radius;
  Vec3 offset = u*rd.x + v*rd.y;

  return Ray(origin + offset, lower_left_corner + horizontal*s + vertical*t - origin - offset);
}