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

BVHNode::BVHNode(std::vector<Hittable*> &objects, size_t start, size_t end) {  
  int axis = int(3 * random_double());
  auto comparator = (axis == 0) ? box_x_compare : (axis == 1) ? box_y_compare : box_z_compare;
  size_t object_span = end - start;

  if (object_span == 1) {
    left = right = objects[start];    
  } else if (object_span == 2) {
    if (comparator(objects[start], objects[start + 1])) {
      left = objects[start];
      right = objects[start + 1];
    } else {
      left = objects[start + 1];
      right = objects[start];
    }
  } else {
    std::sort(objects.begin() + start, objects.begin() + end, comparator);
    auto mid = start + object_span/2;

    left = new BVHNode(objects, start, mid);
    right = new BVHNode(objects, mid, end);
  }

  AABB box_left, box_right;
  left -> bounding_box(box_left);
  right -> bounding_box(box_right);

  box = surrounding_box(box_left, box_right);
}

bool BVHNode::hit(const Ray &ray, double t_min, double t_max, HitRecord &rec) const {
  if (!box.hit(ray, t_min, t_max)) {
    return false;
  }
  bool hit_left = left -> hit(ray, t_min, t_max, rec);
  bool hit_right = right -> hit(ray, t_min, hit_left ? rec.t : t_max, rec);
  return hit_left || hit_right;
}

bool BVHNode::bounding_box(AABB &output_box) const {
  output_box = box;
  return true;
};
