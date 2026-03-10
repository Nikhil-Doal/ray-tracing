#include "../core/hittable.h"
#include <iostream>

bool box_compare(const Hittable *a, const Hittable *b, int axis) {
  AABB box_a; 
  AABB box_b;
  if (!a -> bounding_box(box_a) || !b -> bounding_box(box_b)) {
    std::cerr << "No bounding box\n";
  }
  return box_a.minimum[axis] < box_b.minimum[axis];
}

bool box_x_compare(const Hittable *a, const Hittable *b) {
  return box_compare(a, b, 0);
}

bool box_y_compare(const Hittable *a, const Hittable *b) {
  return box_compare(a, b, 1);
}

bool box_z_compare(const Hittable *a, const Hittable *b) {
  return box_compare(a, b, 2);
}