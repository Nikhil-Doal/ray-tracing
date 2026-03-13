#pragma once
#include "../core/ray.h"
#include <utility>

// bvh implementation
// aabb -> axis aligned bounding box 
class AABB {
public:
  Vec3 minimum;
  Vec3 maximum;
  AABB() {}
  AABB(const Vec3 &a, const Vec3 &b) : minimum(a), maximum(b) {}
  bool hit(const Ray &ray, double t_min, double t_max) const;
};

inline bool AABB::hit(const Ray &ray, double t_min, double t_max) const {
  for (int i = 0; i < 3; ++i) {
    double invD = 1.0 / ray.direction[i];
    double t0 = (minimum[i] - ray.origin[i]) * invD;
    double t1 = (maximum[i] - ray.origin[i]) * invD;

    if (invD < 0.0) std::swap(t0, t1);

    t_min = t0 > t_min ? t0 : t_min;
    t_max = t_max > t1 ? t1 : t_max;

    if (t_max <= t_min) return false;
  }

  return true;
}


// helpers
inline AABB surrounding_box(AABB box0, AABB box1) {
  Vec3 small(fmin(box0.minimum.x, box1.minimum.x), fmin(box0.minimum.y, box1.minimum.y), fmin(box0.minimum.z, box1.minimum.z));
  Vec3 big(fmax(box0.maximum.x, box1.maximum.x), fmax(box0.maximum.y, box1.maximum.y), fmax(box0.maximum.z, box1.maximum.z));
  return AABB(small, big);
}