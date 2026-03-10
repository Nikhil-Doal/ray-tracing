#pragma once
#include "hittable_list.h"
#include "triangle.h"

class Mesh : public HittableList {
public:
  Mesh() {}
  bool bounding_box(AABB &output_box) const override;
};
bool Mesh::bounding_box(AABB &output_box) const {
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