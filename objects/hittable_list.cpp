#include "hittable_list.h"

void HittableList::add(Hittable *obj) {objects.push_back(obj);}

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

bool HittableList::bounding_box(AABB &output_box) const {
  if(objects.empty()) return false;
  AABB tmp_box;
  bool first_box = true;

  for(const auto &object : objects) {
    if (!object -> bounding_box(tmp_box)) return false;
    output_box = first_box ? tmp_box : surrounding_box(output_box, tmp_box);
    first_box = false;
  }
  return true;
}