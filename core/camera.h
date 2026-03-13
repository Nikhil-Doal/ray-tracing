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
