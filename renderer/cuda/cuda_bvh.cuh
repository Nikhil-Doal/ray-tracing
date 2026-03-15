#pragma once
#include "cuda_scene.cuh"
#include <vector>
#include <unordered_map>
#include <functional>
#include <algorithm>
#include "../../core/hittable.h"
#include "../../objects/bvh_node.h"
#include "../../objects/hittable_list.h"
#include "../../objects/sphere.h"
#include "../../objects/triangle.h"
#include "../../objects/rotate.h"
#include "../../materials/material.h"
#include "../../materials/lambertian.h"
#include "../../materials/metal.h"
#include "../../materials/dielectric.h"
#include "../../materials/diffuse_light.h"

// result of flattening the cpu scene - holds host_side arrays to upload to gpu if availible
struct FlatScene{
  std::vector<GpuPrimitive> primitives;
  std::vector<GpuBVHNode> bvh_nodes;
  std::vector<GpuMaterial> materials;
  int root = 0;
};


// convert cpu Material * to a GpuMaterial
inline GpuMaterial convert_material(Material *mat) {
  GpuMaterial m{};
  m.is_emissive = mat->is_emissive();
  m.is_transmissive = mat->is_transmissive();

  if (auto *lamb = dynamic_cast<Lambertian*>(mat)) {
    m.type = GpuMatType::LAMBERTIAN;
    // using dummy HitRecord
    HitRecord dummy{};
    Vec3 a = lamb -> albedo_at(dummy);
    m.albedo = {(float)a.x, (float)a.y, (float)a.z};
  } 
  
  else if (auto *met = dynamic_cast<Metal*>(mat)) {
    m.type = GpuMatType::METAL;
    HitRecord dummy{};
    Vec3 a = met->albedo_at(dummy);
    m.albedo = {(float)a.x, (float)a.y, (float)a.z};
    m.fuzz   = (float)met->fuzz;
  }

  else if (auto *die = dynamic_cast<Dielectric*>(mat)) {
    m.type = GpuMatType::DIELECTRIC;
    m.ir   = (float)die->ir;
    m.is_transmissive = true;
  } 

  else if (mat->is_emissive()) {
    m.type = GpuMatType::DIFFUSE_LIGHT;
    Vec3 e = mat->emit(0, 0, Vec3(0,0,0));
    m.emit_color  = {(float)e.x, (float)e.y, (float)e.z};
    m.is_emissive = true;
  }

  else {
    // fallback random lambertian
    m.type   = GpuMatType::LAMBERTIAN;
    m.albedo = {0.5f, 0.5f, 0.5f};
  }

  return m;
}

// get or add a material and return its index
inline int get_or_add_material(Material *mat, std::unordered_map<Material*, int> &mat_map, std::vector<GpuMaterial> &mats) {
  auto it = mat_map.find(mat);
  if (it != mat_map.end()) return it -> second;
  int id = (int) mats.size();
  mats.push_back(convert_material(mat));
  mat_map[mat] = id;
  return id;
}

// Recursively collect all primitives frm CPU hittable tree into flat array handling BVHNode, HittableList, Sphere, Triangle
inline void collect_primitives(const Hittable *node, std::vector<GpuPrimitive> &primitives, std::unordered_map<Material*, int> &mat_map, std::vector<GpuMaterial> &mats) {
  if (!node) return;
  if (auto *bvh = dynamic_cast<const BVHNode*>(node)) {
    collect_primitives(bvh->left.get(), primitives, mat_map, mats);
    if (bvh->right.get() != bvh->left.get())
      collect_primitives(bvh->right.get(), primitives, mat_map, mats);
  }
  else if (auto *list = dynamic_cast<const HittableList*>(node)) {
    for (auto &obj : list -> objects) collect_primitives(obj.get(), primitives, mat_map, mats);
  }
  else if (auto *rot = dynamic_cast<const Rotate*>(node)){
    // not applying rotation right now, *just rotate all obj or item vertices for future
    collect_primitives(rot->object.get(), primitives, mat_map, mats);
  }
  else if (auto *sph = dynamic_cast<const Sphere*>(node)) {
    GpuPrimitive p{};
    p.type = GpuGeomType::SPHERE;
    p.sphere.center = {(float)sph->center.x, (float)sph->center.y, (float)sph->center.z};
    p.sphere.radius = (float)sph->radius;
    p.mat_id = get_or_add_material(sph->mat.get(), mat_map, mats);
    primitives.push_back(p);
  } 
  else if (auto *tri = dynamic_cast<const Triangle*>(node)) {
    GpuPrimitive p{};
    p.type = GpuGeomType::TRIANGLE;
    p.triangle.v0  = {(float)tri->v0.x,  (float)tri->v0.y,  (float)tri->v0.z};
    p.triangle.v1  = {(float)tri->v1.x,  (float)tri->v1.y,  (float)tri->v1.z};
    p.triangle.v2  = {(float)tri->v2.x,  (float)tri->v2.y,  (float)tri->v2.z};
    p.triangle.uv0 = {(float)tri->uv0.x, (float)tri->uv0.y, 0};
    p.triangle.uv1 = {(float)tri->uv1.x, (float)tri->uv1.y, 0};
    p.triangle.uv2 = {(float)tri->uv2.x, (float)tri->uv2.y, 0};
    p.mat_id = get_or_add_material(tri->mat.get(), mat_map, mats);
    primitives.push_back(p);
  }
  // skipping plane as difficult to handle infinite distance
}

inline int build_flat_bvh_recursive(std::vector<GpuPrimitive> &primitives, std::vector<GpuBVHNode> &nodes, int start, int end) {
  int index = (int)nodes.size();
  nodes.push_back({}); // nothing but reserving a space

  GpuBVHNode &node = nodes[index];

  // comput the AABB over primitives from start to end
  node.aabb_min = {1e30f, 1e30f, 1e30f};
  node.aabb_max = {-1e30f, -1e30f, -1e30f};

  for (int i = start; i < end; ++i) {
    const GpuPrimitive &p = primitives[i];
    if (p.type == GpuGeomType::SPHERE) {
      float r = p.sphere.radius;
      node.aabb_min.x = fminf(node.aabb_min.x, p.sphere.center.x - r);
      node.aabb_min.y = fminf(node.aabb_min.y, p.sphere.center.y - r);
      node.aabb_min.z = fminf(node.aabb_min.z, p.sphere.center.z - r);
      node.aabb_max.x = fmaxf(node.aabb_max.x, p.sphere.center.x + r);
      node.aabb_max.y = fmaxf(node.aabb_max.y, p.sphere.center.y + r);
      node.aabb_max.z = fmaxf(node.aabb_max.z, p.sphere.center.z + r);
    } else {
      const GpuTriangle &t = p.triangle;
      node.aabb_min.x = fminf(fminf(node.aabb_min.x, t.v0.x), fminf(t.v1.x, t.v2.x));
      node.aabb_min.y = fminf(fminf(node.aabb_min.y, t.v0.y), fminf(t.v1.y, t.v2.y));
      node.aabb_min.z = fminf(fminf(node.aabb_min.z, t.v0.z), fminf(t.v1.z, t.v2.z));
      node.aabb_max.x = fmaxf(fmaxf(node.aabb_max.x, t.v0.x), fmaxf(t.v1.x, t.v2.x));
      node.aabb_max.y = fmaxf(fmaxf(node.aabb_max.y, t.v0.y), fmaxf(t.v1.y, t.v2.y));
      node.aabb_max.z = fmaxf(fmaxf(node.aabb_max.z, t.v0.z), fmaxf(t.v1.z, t.v2.z));
    }
  }

  const float PAD = 1e-3f;
  if (node.aabb_max.x - node.aabb_min.x < PAD) { node.aabb_min.x -= PAD; node.aabb_max.x += PAD; }
  if (node.aabb_max.y - node.aabb_min.y < PAD) { node.aabb_min.y -= PAD; node.aabb_max.y += PAD; }
  if (node.aabb_max.z - node.aabb_min.z < PAD) { node.aabb_min.z -= PAD; node.aabb_max.z += PAD; }

  if (end - start == 1) {
    // these are the tree leafs
    node.left = node.right = -1;
    node.prim_start = start;
    node.prim_count = 1;
  } else {
    node.prim_count = 0;
    // split along the longest axis 
    GpuVec3 extent = node.aabb_max - node.aabb_min;
    int axis = 0;
    if (extent.y > extent.x) axis = 1;
    if (extent.z > (axis==0 ? extent.x : extent.y)) axis = 2;

    // sort along chosen axis created by centeroid
    std::sort(primitives.begin() + start, primitives.begin() + end, [axis](const GpuPrimitive &a, const GpuPrimitive &b) {
      float ca = (a.type == GpuGeomType::SPHERE) ? a.sphere.center[axis] : (a.triangle.v0[axis] + a.triangle.v1[axis] + a.triangle.v2[axis]) / 3.0f;
      float cb = (b.type == GpuGeomType::SPHERE) ? b.sphere.center[axis] : (b.triangle.v0[axis] + b.triangle.v1[axis] + b.triangle.v2[axis]) / 3.0f;
      return ca < cb;
    });

    int mid = (start + end) / 2;

    //building childern, since vector can reallocate, we use indexes instead of addresses (pass by ref) after recursive call
    int left_index = build_flat_bvh_recursive(primitives, nodes, start, mid);
    int right_index = build_flat_bvh_recursive(primitives, nodes, mid, end);

    nodes[index].left = left_index;
    nodes[index].right = right_index;
  }
  return index;
}

// Main entry point for cpu to convert full cpu scene into a flat gpu array
inline FlatScene build_flat_scene(const Hittable &cpu_root) {
  FlatScene flat;
  std::unordered_map<Material*, int> mat_map;

  // collect all prims and mats
  collect_primitives(&cpu_root, flat.primitives, mat_map, flat.materials);
  if (flat.primitives.empty()) {
    std::cerr << "no gpu prims collected";
    return flat;
  }

  std::cout << "GPU scene: " << flat.primitives.size() << " primitives, " << flat.materials.size()  << " materials\n";

  // build flat bvh over collected prims
  flat.bvh_nodes.reserve(flat.primitives.size() * 2);
  flat.root = build_flat_bvh_recursive(flat.primitives, flat.bvh_nodes, 0, (int)flat.primitives.size());
  std::cout << "GPU BVH nodes: " << flat.bvh_nodes.size() << "\n";
  
  return flat;
}