#pragma once
#include "../core/hittable.h"
#include <memory>
#include <cmath>

class Transform : public Hittable {
public:
  std::shared_ptr<Hittable> object;
  double sin_y, cos_y; // rotation around Y axis

  Transform(std::shared_ptr<Hittable> obj, double degrees) : object(obj) {
    double radians = degrees * PI / 180.0;
    sin_y = sin(radians);
    cos_y = cos(radians);
  }

  bool hit(const Ray &ray, double t_min, double t_max, HitRecord &rec) const override {
    Vec3 ro = ray.origin;
    Vec3 rd = ray.direction;

    ro.x =  cos_y * ray.origin.x + sin_y * ray.origin.z;
    ro.z = -sin_y * ray.origin.x + cos_y * ray.origin.z;
    rd.x =  cos_y * ray.direction.x + sin_y * ray.direction.z;
    rd.z = -sin_y * ray.direction.x + cos_y * ray.direction.z;

    Ray rotated_ray(ro, rd);
    if (!object->hit(rotated_ray, t_min, t_max, rec)) return false;

    Vec3 p = rec.point;
    Vec3 n = rec.normal;

    p.x = cos_y * rec.point.x - sin_y * rec.point.z;
    p.z = sin_y * rec.point.x + cos_y * rec.point.z;

    n.x = cos_y * rec.normal.x - sin_y * rec.normal.z;
    n.z = sin_y * rec.normal.x + cos_y * rec.normal.z;

    rec.point = p;
    Vec3 world_rd = ray.direction;
    rec.set_face_normal(Ray(rec.point, world_rd), n.normalize());
    return true;
  }

  bool bounding_box(AABB &output_box) const override {
    AABB box;
    if (!object->bounding_box(box)) return false;

    Vec3 mn(1e9,1e9,1e9), mx(-1e9,-1e9,-1e9);
    for (int i = 0; i < 2; ++i)
    for (int j = 0; j < 2; ++j)
    for (int k = 0; k < 2; ++k) {
      double x = i ? box.maximum.x : box.minimum.x;
      double y = j ? box.maximum.y : box.minimum.y;
      double z = k ? box.maximum.z : box.minimum.z;

      double rx =  cos_y * x + sin_y * z;
      double rz = -sin_y * x + cos_y * z;

      mn.x = fmin(mn.x, rx); mn.y = fmin(mn.y, y); mn.z = fmin(mn.z, rz);
      mx.x = fmax(mx.x, rx); mx.y = fmax(mx.y, y); mx.z = fmax(mx.z, rz);
    }
    output_box = AABB(mn, mx);
    return true;
  }
};
