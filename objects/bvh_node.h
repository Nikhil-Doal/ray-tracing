#pragma once
#include "../core/hittable.h"
#include "aabb.h"
#include "../utils/bvh_helpers.h"
#include <algorithm>
#include <vector>


class BVHNode : public Hittable {
public:
  Hittable *left;
  Hittable *right;
  AABB box;
  
  BVHNode(std::vector<Hittable*> &objects, size_t start, size_t end);
  bool hit(const Ray &ray, double t_min, double t_max, HitRecord &rec) const override;
  bool bounding_box(AABB &output_box) const override;
};
