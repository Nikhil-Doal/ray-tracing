#include "camera.h"

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