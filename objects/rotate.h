#pragma once
#include "../core/hittable.h"
#include <memory>
#include <cmath>

class Rotate : public Hittable {
public:
  std::shared_ptr<Hittable> object;
  double sin_x, cos_x;
  double sin_y, cos_y;
  double sin_z, cos_z;

  Rotate(std::shared_ptr<Hittable> obj, double deg_x, double deg_y, double deg_z) : object(obj) {
    auto toRad = [](double d) { return d * PI / 180.0; };
    sin_x = sin(toRad(deg_x)); cos_x = cos(toRad(deg_x));
    sin_y = sin(toRad(deg_y)); cos_y = cos(toRad(deg_y));
    sin_z = sin(toRad(deg_z)); cos_z = cos(toRad(deg_z));
  }

  Vec3 rotate(const Vec3 &v) const {
    // Apply X then Y then Z rotation
    // X axis rotation
    double y1 = cos_x * v.y - sin_x * v.z;
    double z1 = sin_x * v.y + cos_x * v.z;
    // Y axis rotation
    double x2 =  cos_y * v.x + sin_y * z1;
    double z2 = -sin_y * v.x + cos_y * z1;
    // Z axis rotation
    double x3 = cos_z * x2 - sin_z * y1;
    double y3 = sin_z * x2 + cos_z * y1;
    return Vec3(x3, y3, z2);
  }

  Vec3 rotate_inverse(const Vec3 &v) const {
    // Inverse = reverse order, negate angles
    // Inverse Z
    double x1 =  cos_z * v.x + sin_z * v.y;
    double y1 = -sin_z * v.x + cos_z * v.y;
    // Inverse Y
    double x2 = cos_y * x1 - sin_y * v.z;
    double z2 = sin_y * x1 + cos_y * v.z;
    // Inverse X
    double y3 =  cos_x * y1 + sin_x * z2;
    double z3 = -sin_x * y1 + cos_x * z2;
    return Vec3(x2, y3, z3);
  }

  bool hit(const Ray &ray, double t_min, double t_max, HitRecord &rec) const override {
    // rotate ray into object space using inverse rotation
    Vec3 ro = rotate_inverse(ray.origin);
    Vec3 rd = rotate_inverse(ray.direction);

    Ray rotated_ray(ro, rd);
    if (!object->hit(rotated_ray, t_min, t_max, rec)) return false;

    // rotate hit results back to world space
    rec.point = rotate(rec.point);
    Vec3 n = rotate(rec.normal).normalize();
    rec.set_face_normal(Ray(rec.point, ray.direction), n);
    return true;
  }

  bool bounding_box(AABB &output_box) const override {
    AABB box;
    if (!object->bounding_box(box)) return false;

    Vec3 mn(1e9,1e9,1e9), mx(-1e9,-1e9,-1e9);
    for (int i = 0; i < 2; ++i)
    for (int j = 0; j < 2; ++j)
    for (int k = 0; k < 2; ++k) {
      Vec3 corner(
        i ? box.maximum.x : box.minimum.x,
        j ? box.maximum.y : box.minimum.y,
        k ? box.maximum.z : box.minimum.z
      );
      Vec3 rotated = rotate(corner);
      mn.x = fmin(mn.x, rotated.x); mn.y = fmin(mn.y, rotated.y); mn.z = fmin(mn.z, rotated.z);
      mx.x = fmax(mx.x, rotated.x); mx.y = fmax(mx.y, rotated.y); mx.z = fmax(mx.z, rotated.z);
    }
    output_box = AABB(mn, mx);
    return true;
  }
};