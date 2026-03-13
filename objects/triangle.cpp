#include "triangle.h"

// triangle.cpp
Triangle::Triangle(Vec3 a, Vec3 b, Vec3 c, std::shared_ptr<Material> mat, Vec3 uv0, Vec3 uv1, Vec3 uv2) : v0(a), v1(b), v2(c), mat(mat), uv0(uv0), uv1(uv1), uv2(uv2) {}
// using Moller-Trumbore algorithm
bool Triangle::hit(const Ray &ray, double t_min, double t_max, HitRecord &rec) const {
  const double EPSILON = 1e-8;

  Vec3 edge1 = v1 - v0;
  Vec3 edge2 = v2 - v0;
  Vec3 h = ray.direction.cross(edge2);
  double a = edge1.dot(h);
  if (fabs(a) < EPSILON) return false;

  double f = 1.0 / a;
  Vec3 s = ray.origin - v0;
  double bary_u = f * s.dot(h);
  if (bary_u < 0.0 || bary_u > 1.0) return false;

  Vec3 q = s.cross(edge1);
  double bary_v = f * ray.direction.dot(q);
  if (bary_v < 0.0 || bary_u + bary_v > 1.0) return false;

  double t = f * edge2.dot(q);
  if (t < t_min || t > t_max) return false;

  double bary_w = 1.0 - bary_u - bary_v;
  rec.t = t;
  rec.point = ray.at(t);
  rec.u = bary_w * uv0.x + bary_u * uv1.x + bary_v * uv2.x;
  rec.v = bary_w * uv0.y + bary_u * uv1.y + bary_v * uv2.y;

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