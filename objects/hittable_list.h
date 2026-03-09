#pragma once
#include <vector>
#include "../core/hittable.h"

class HittableList : public Hittable {
public:
  std::vector<Hittable*> objects;
  void add(Hittable *obj) {objects.push_back(obj);}
  bool hit(const Ray &ray, double t_min, double t_max, HitRecord &rec) const override;
};
bool HittableList::hit(const Ray &ray, double t_min, double t_max, HitRecord &rec) const {
  HitRecord temp_record;
  bool hit_anything = false;
  double closest_yet = t_max;

  for (const auto &obj : objects) {
    if (obj -> hit(ray, t_min, closest_yet, temp_record)) {
      hit_anything = true;
      closest_yet = temp_record.t;
      rec = temp_record;
    }
  }

  return hit_anything;
}
