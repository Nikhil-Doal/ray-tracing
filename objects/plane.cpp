#include "plane.h"

Plane::Plane(const Vec3 &p, const Vec3 &n, std::shared_ptr<Material> m, double s) : point(p), normal(n), mat(m), side(s) {}

static void compute_tangent_frame(const Vec3 &normal, Vec3 &u_axis, Vec3 &v_axis) {
  // Choose a vector NOT parallel to normal
  Vec3 up = (fabs(normal.y) < 0.999) ? Vec3(0, 1, 0) : Vec3(1, 0, 0);
  u_axis = normal.cross(up).normalize();
  v_axis = normal.cross(u_axis).normalize();
}

bool Plane::hit(const Ray &ray, double t_min, double t_max, HitRecord &rec) const {
  double denom = normal.dot(ray.direction);
  if (fabs(denom) < 1e-8) return false; // parallel to ray not visible

  double t = (point - ray.origin).dot(normal) / denom;
  if (t < t_min || t > t_max) return false;

  Vec3 hitpoint = ray.at(t);

  Vec3 u_axis, v_axis;
  compute_tangent_frame(normal, u_axis, v_axis);

  Vec3 local = hitpoint - point;
  double local_u = local.dot(u_axis);
  double local_v = local.dot(v_axis);
  
  if (side > 0) {
    if (fabs(local_u) > side/2 || fabs(local_v) > side/2) return false; // not in plane range
  }
  
  rec.u = fmod(fabs(local.dot(u_axis) * 0.1), 1.0);  // 0.1 = tiling scale
  rec.v = fmod(fabs(local.dot(v_axis) * 0.1), 1.0);

  rec.t = t;
  rec.point = ray.at(t);
  rec.mat = mat;
  rec.set_face_normal(ray, normal);

  // tbn for normal and bump mapping
  rec.tangent = u_axis;
  rec.bitangent = v_axis;
  rec.has_tbn = true;

  return true;
}

bool Plane::bounding_box(AABB &output_box) const {
  Vec3 u_axis, v_axis;
  compute_tangent_frame(normal, u_axis, v_axis);

  if (side <= 0) {
    // Infinite plane: use large but finite bounds along tangent directions and thin slab along normal
    double inf = 1e6;
    Vec3 corners[4] = {point + u_axis * inf + v_axis * inf, point + u_axis * inf - v_axis * inf, point - u_axis * inf + v_axis * inf, point - u_axis * inf - v_axis * inf};
    Vec3 mn = corners[0], mx = corners[0];
    for (int i = 1; i < 4; ++i) {
      mn.x = std::min(mn.x, corners[i].x);
      mn.y = std::min(mn.y, corners[i].y);
      mn.z = std::min(mn.z, corners[i].z);
      mx.x = std::max(mx.x, corners[i].x);
      mx.y = std::max(mx.y, corners[i].y);
      mx.z = std::max(mx.z, corners[i].z);
    }
    // Pad along normal for thin slab
    Vec3 pad = Vec3(fabs(normal.x), fabs(normal.y), fabs(normal.z)) * 1e-4;
    output_box = AABB(mn - pad, mx + pad);
  } else {
    // Finite square
    double hs = side / 2;
    Vec3 corners[4] = {
      point + u_axis * hs + v_axis * hs,
      point + u_axis * hs - v_axis * hs,
      point - u_axis * hs + v_axis * hs,
      point - u_axis * hs - v_axis * hs
    };
    Vec3 mn = corners[0], mx = corners[0];
    for (int i = 1; i < 4; ++i) {
      mn.x = std::min(mn.x, corners[i].x);
      mn.y = std::min(mn.y, corners[i].y);
      mn.z = std::min(mn.z, corners[i].z);
      mx.x = std::max(mx.x, corners[i].x);
      mx.y = std::max(mx.y, corners[i].y);
      mx.z = std::max(mx.z, corners[i].z);
    }
    // Pad along normal
    Vec3 pad = Vec3(fabs(normal.x), fabs(normal.y), fabs(normal.z)) * 1e-4;
    output_box = AABB(mn - pad, mx + pad);
  }
  return true;
}