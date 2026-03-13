#pragma once
#include "../core/hittable.h"
#include "aabb.h"
#include "../utils/bvh_helpers.h"
#include <algorithm>
#include <vector>
#include <memory>

class BVHNode : public Hittable {
public:
  std::shared_ptr<Hittable> left;
  std::shared_ptr<Hittable> right;
  AABB box;
  
  BVHNode(std::vector<std::shared_ptr<Hittable>> &objects, size_t start, size_t end);
  bool hit(const Ray &ray, double t_min, double t_max, HitRecord &rec) const override;
  bool bounding_box(AABB &output_box) const override;
};
