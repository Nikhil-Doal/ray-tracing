#pragma once
#include <vector>
#include "../core/hittable.h"

class HittableList : public Hittable {
public:
  std::vector<Hittable*> objects;
  void add(Hittable *obj);
  bool hit(const Ray &ray, double t_min, double t_max, HitRecord &rec) const override;
  virtual bool bounding_box(AABB &output_box) const override;
};