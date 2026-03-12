#pragma once
#include "../core/hittable.h"

class Plane : public Hittable {
public:
  Vec3 point;
  Vec3 normal;
  Material *mat;
  double side;

  Plane(const Vec3 &p, const Vec3 &n, Material *m, double s = 0.0) : point(p), normal(n), mat(m), side(s) {}
  bool hit(const Ray &ray, double t_min, double t_max, HitRecord &rec) const override;
  bool bounding_box(AABB &output_box) const override;
};

bool Plane::hit(const Ray &ray, double t_min, double t_max, HitRecord &rec) const {
  double denom = normal.dot(ray.direction);
  if (fabs(denom) < 1e-8) return false; // parallel to ray not visible

  double t = (point - ray.origin).dot(normal) / denom;
  if (t < t_min || t > t_max) return false;

  Vec3 hitpoint = ray.at(t);
  if (side > 0) {
    Vec3 local = hitpoint - point;
    Vec3 u_axis = normal.cross(Vec3(0, 1, 0));
    if (u_axis.norm() < 1e-3) u_axis = normal.cross(Vec3(1, 0, 0));
    u_axis = u_axis.normalize();
    Vec3 v_axis = normal.cross(u_axis);

    double u = local.dot(u_axis);
    double v = local.dot(v_axis);

    if (fabs(u) > side/2 || fabs(v) > side/2) return false; // not in plane range
  }

  rec.t = t;
  rec.point = ray.at(t);
  rec.mat = mat;
  rec.set_face_normal(ray, normal);
  return true;
}

bool Plane::bounding_box(AABB &output_box) const {
  double inf = 1e6;
  if (side <= 0) {
    // Infinite plane
    output_box = AABB(Vec3(-inf, point.y-1e-4, -inf), Vec3(inf, point.y+1e-4, inf));
  } else {
    // Finite square
    Vec3 u_axis = normal.cross(Vec3(0,1,0));
    if (u_axis.norm() < 1e-3) u_axis = normal.cross(Vec3(1,0,0));
    u_axis = u_axis.normalize();
    Vec3 v_axis = normal.cross(u_axis);

    Vec3 corners[4] = {
      point + (u_axis*(side/2)) + (v_axis*(side/2)),
      point + (u_axis*(side/2)) - (v_axis*(side/2)),
      point - (u_axis*(side/2)) + (v_axis*(side/2)),
      point - (u_axis*(side/2)) - (v_axis*(side/2))
    };

    Vec3 min = corners[0], max = corners[0];
    for(int i=1;i<4;i++){
      min.x = std::min(min.x, corners[i].x);
      min.y = std::min(min.y, corners[i].y);
      min.z = std::min(min.z, corners[i].z);
      max.x = std::max(max.x, corners[i].x);
      max.y = std::max(max.y, corners[i].y);
      max.z = std::max(max.z, corners[i].z);
    }
    output_box = AABB(min, max);
  }
  return true;
}