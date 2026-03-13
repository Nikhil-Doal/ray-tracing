#pragma once
#include <vector>
#include "../core/hittable.h"
#include <memory>

class HittableList : public Hittable {
public:
  std::vector<std::shared_ptr<Hittable>> objects;
  void add(std::shared_ptr<Hittable> obj);
  bool hit(const Ray &ray, double t_min, double t_max, HitRecord &rec) const override;
  virtual bool bounding_box(AABB &output_box) const override;
};