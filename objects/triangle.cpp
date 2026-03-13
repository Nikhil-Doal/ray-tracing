#include "triangle.h"

// using Moller-Trumbore algorithm
bool Triangle::hit(const Ray &ray, double t_min, double t_max, HitRecord &rec) const {
  const double EPSILON = 1e-8;

  Vec3 edge1 = v1 - v0;
  Vec3 edge2 = v2 - v0;
  Vec3 h = ray.direction.cross(edge2);
  double a = edge1.dot(h);

  if (fabs(a) < EPSILON) return false;

  double f = 1.0/a;
  Vec3 s = ray.origin - v0;
  double u = f * s.dot(h);
  if (u < 0.0 || u > 1.0) return false;

  Vec3 q = s.cross(edge1);
  double v = f * ray.direction.dot(q);

  if (v < 0.0 || u + v > 1.0) return false;

  double t = f * edge2.dot(q);
  if (t < t_min || t > t_max) return false;

  rec.t = t;
  rec.point = ray.at(t);

  Vec3 normal = edge1.cross(edge2).normalize();
  rec.set_face_normal(ray, normal);
  rec.mat = mat;

  return true;
}

bool Triangle::bounding_box(AABB &output_box) const {
  Vec3 big(fmax(v0.x ,fmax(v1.x, v2.x)), fmax(v0.y ,fmax(v1.y, v2.y)), fmax(v0.z ,fmax(v1.z, v2.z)));
  Vec3 small(fmin(v0.x ,fmin(v1.x, v2.x)), fmin(v0.y ,fmin(v1.y, v2.y)), fmin(v0.z ,fmin(v1.z, v2.z)));
  output_box = AABB(small, big);
  return true;
}