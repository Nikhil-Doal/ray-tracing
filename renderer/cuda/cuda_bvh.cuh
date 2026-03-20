#pragma once
#include "cuda_scene.cuh"
#include <vector>
#include <unordered_map>
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
#include "../../materials/glossy.h"
#include "../../textures/texture.h"
#include "../../textures/solid_color.h"
#include "../../textures/image_texture.h"

// result of flattening the cpu scene - holds host_side arrays to upload to gpu if availible
struct FlatScene{
  std::vector<GpuPrimitive> primitives;
  std::vector<GpuBVHNode> bvh_nodes;
  std::vector<GpuMaterial> materials;
  std::vector<GpuTexture> textures;
  std::vector<unsigned char> tex_data;
  int root = 0;
};

// ---- Rotation transform helper ----
// Stores the sin/cos values from a Rotate node so we can transform vertices/normals
struct RotateTransform {
  double sin_x, cos_x, sin_y, cos_y, sin_z, cos_z;
  bool active = false;

  RotateTransform() : sin_x(0), cos_x(1), sin_y(0), cos_y(1), sin_z(0), cos_z(1), active(false) {}

  static RotateTransform from_rotate(const Rotate *rot) {
    RotateTransform t;
    t.sin_x = rot->sin_x; t.cos_x = rot->cos_x;
    t.sin_y = rot->sin_y; t.cos_y = rot->cos_y;
    t.sin_z = rot->sin_z; t.cos_z = rot->cos_z;
    t.active = true;
    return t;
  }

  Vec3 apply(const Vec3 &v) const {
    if (!active) return v;
    // X rotation
    double y1 = cos_x * v.y - sin_x * v.z;
    double z1 = sin_x * v.y + cos_x * v.z;
    // Y rotation
    double x2 = cos_y * v.x + sin_y * z1;
    double z2 = -sin_y * v.x + cos_y * z1;
    // Z rotation
    double x3 = cos_z * x2 - sin_z * y1;
    double y3 = sin_z * x2 + cos_z * y1;
    return Vec3(x3, y3, z2);
  }

  // Compose: apply this transform THEN other
  RotateTransform compose(const RotateTransform &other) const {
    // For simplicity with multiple stacked rotations, we don't analytically
    // compose — instead we'll apply transforms in sequence during vertex
    // collection. This struct is only used for a single Rotate node.
    (void)other;
    return *this; // caller should apply in sequence
  }
};

// texture helpers
inline int register_texture(Texture *tex, std::unordered_map<Texture*, int> &tex_map, std::vector<GpuTexture> &textures, std::vector<unsigned char> &tex_data) {
  if (!tex) return -1;

  // Only ImageTexture gets real texture data, keep SolidColor stays as solid color.
  auto *img = dynamic_cast<ImageTexture*>(tex);
  if (!img || !img->data || img->width <= 0 || img->height <= 0) return -1;

  auto it = tex_map.find(tex);
  if (it != tex_map.end()) return it->second;

  int id = (int)textures.size();
  tex_map[tex] = id;

  GpuTexture gt;
  gt.offset = (int)(tex_data.size() / 3);  // pixel offset
  gt.width  = img->width;
  gt.height = img->height;
  textures.push_back(gt);

  // Append raw RGB bytes.  stbi loads 3 channels (we request 3 in ImageTexture).
  int num_pixels = img->width * img->height;
  size_t byte_count = num_pixels * 3;
  size_t old_size = tex_data.size();
  tex_data.resize(old_size + byte_count);
  std::memcpy(tex_data.data() + old_size, img->data, byte_count);

  return id;
}

// convert cpu Material * to a GpuMaterial
inline GpuMaterial convert_material(Material *mat, std::unordered_map<Texture*, int> &tex_map, std::vector<GpuTexture> &textures, std::vector<unsigned char> &tex_data) {
  GpuMaterial m{};
  m.is_emissive = mat->is_emissive();
  m.is_transmissive = mat->is_transmissive();
  m.albedo_tex_id   = -1;
  m.emit_tex_id     = -1;
    
  m.normal_tex_id   = -1;
  m.bump_tex_id     = -1;
  m.normal_map_strength = (float)mat->normal_map_strength;
  m.bump_strength       = (float)mat->bump_strength;
  m.roughness = 0.3f;
  m.specular_strength = 0.0f;

  // Register normal map if present
  if (mat->normal_map) {
    m.normal_tex_id = register_texture(mat->normal_map.get(), tex_map, textures, tex_data);
  }
  // Register bump map if present
  if (mat->bump_map) {
    m.bump_tex_id = register_texture(mat->bump_map.get(), tex_map, textures, tex_data);
  }

  if (auto *lamb = dynamic_cast<Lambertian*>(mat)) {
    m.type = GpuMatType::LAMBERTIAN;
    //try to register texture
    m.albedo_tex_id = register_texture(lamb->albedo.get(), tex_map, textures, tex_data);
    // using dummy HitRecord as fallback
    HitRecord dummy{};
    Vec3 a = lamb -> albedo_at(dummy);
    m.albedo = {(float)a.x, (float)a.y, (float)a.z};
  }

  else if (auto *glossy = dynamic_cast<Glossy*>(mat)) {
    m.type = GpuMatType::GLOSSY;
    m.albedo_tex_id = register_texture(glossy->albedo.get(), tex_map, textures, tex_data);
    HitRecord dummy{};
    Vec3 a = glossy->albedo_at(dummy);
    m.albedo = {(float)a.x, (float)a.y, (float)a.z};
    m.roughness = (float)glossy->roughness;
    m.specular_strength = (float)glossy->specular_strength;
  }
 
  else if (auto *met = dynamic_cast<Metal*>(mat)) {
    m.type = GpuMatType::METAL;
    m.albedo_tex_id = register_texture(met->albedo.get(), tex_map, textures, tex_data);
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

    if (auto *dl = dynamic_cast<DiffuseLight*>(mat)) {
      m.emit_tex_id = register_texture(dl->emit_tex.get(), tex_map, textures, tex_data);
      // Solid fallback
      Vec3 e = dl->emit_tex ? dl->emit_tex->value(0, 0, Vec3(0,0,0)) : Vec3(0,0,0);
      m.emit_color = {(float)e.x, (float)e.y, (float)e.z};
    } else {
      Vec3 e = mat->emit(0, 0, Vec3(0,0,0));
      m.emit_color = {(float)e.x, (float)e.y, (float)e.z};
    }
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
inline int get_or_add_material(Material *mat, std::unordered_map<Material*, int> &mat_map, std::vector<GpuMaterial> &mats, std::unordered_map<Texture*, int> &tex_map, std::vector<GpuTexture> &textures, std::vector<unsigned char> &tex_data) {
  auto it = mat_map.find(mat);
  if (it != mat_map.end()) return it -> second;
  int id = (int) mats.size();
  mats.push_back(convert_material(mat, tex_map, textures, tex_data));
  mat_map[mat] = id;
  return id;
}

// Recursively collect all primitives frm CPU hittable tree into flat array handling BVHNode, HittableList, Sphere, Triangle
inline void collect_primitives(const Hittable *node, std::vector<GpuPrimitive> &primitives, std::unordered_map<Material*, int> &mat_map, std::vector<GpuMaterial> &mats, std::unordered_map<Texture*, int>  &tex_map, std::vector<GpuTexture> &textures, std::vector<unsigned char> &tex_data, const RotateTransform &xform = RotateTransform()) {
  if (!node) return;
  if (auto *bvh = dynamic_cast<const BVHNode*>(node)) {
    collect_primitives(bvh->left.get(), primitives, mat_map, mats, tex_map, textures, tex_data, xform);
    if (bvh->right.get() != bvh->left.get())
      collect_primitives(bvh->right.get(), primitives, mat_map, mats, tex_map, textures, tex_data, xform);
  }
  else if (auto *list = dynamic_cast<const HittableList*>(node)) {
    for (auto &obj : list -> objects) collect_primitives(obj.get(), primitives, mat_map, mats, tex_map, textures, tex_data, xform);
  }
  else if (auto *rot = dynamic_cast<const Rotate*>(node)){
    // not applying rotation right now, *just rotate all obj or item vertices for future
    RotateTransform child_xform = RotateTransform::from_rotate(rot);
    collect_primitives(rot->object.get(), primitives, mat_map, mats, tex_map, textures, tex_data, xform);
  }
  else if (auto *sph = dynamic_cast<const Sphere*>(node)) {
    GpuPrimitive p{};
    p.type = GpuGeomType::SPHERE;
    Vec3 center = xform.apply(sph->center);
    p.sphere.center = {(float)sph->center.x, (float)sph->center.y, (float)sph->center.z};
    p.sphere.radius = (float)sph->radius;
    p.mat_id = get_or_add_material(sph->mat.get(), mat_map, mats, tex_map, textures, tex_data);
    primitives.push_back(p);
  } 
  else if (auto *tri = dynamic_cast<const Triangle*>(node)) {
    GpuPrimitive p{};
    p.type = GpuGeomType::TRIANGLE;
    Vec3 tv0 = xform.apply(tri->v0);
    Vec3 tv1 = xform.apply(tri->v1);
    Vec3 tv2 = xform.apply(tri->v2);
    p.triangle.v0  = {(float)tri->v0.x,  (float)tri->v0.y,  (float)tri->v0.z};
    p.triangle.v1  = {(float)tri->v1.x,  (float)tri->v1.y,  (float)tri->v1.z};
    p.triangle.v2  = {(float)tri->v2.x,  (float)tri->v2.y,  (float)tri->v2.z};
    p.triangle.uv0 = {(float)tri->uv0.x, (float)tri->uv0.y, 0};
    p.triangle.uv1 = {(float)tri->uv1.x, (float)tri->uv1.y, 0};
    p.triangle.uv2 = {(float)tri->uv2.x, (float)tri->uv2.y, 0};
        // Export per-vertex normals
    p.triangle.has_vertex_normals = tri->has_vertex_normals;
    if (tri->has_vertex_normals) {
      Vec3 rn0 = xform.apply(tri->n0).normalize();
      Vec3 rn1 = xform.apply(tri->n1).normalize();
      Vec3 rn2 = xform.apply(tri->n2).normalize();
      p.triangle.n0 = {(float)tri->n0.x, (float)tri->n0.y, (float)tri->n0.z};
      p.triangle.n1 = {(float)tri->n1.x, (float)tri->n1.y, (float)tri->n1.z};
      p.triangle.n2 = {(float)tri->n2.x, (float)tri->n2.y, (float)tri->n2.z};
    }

    p.mat_id = get_or_add_material(tri->mat.get(), mat_map, mats, tex_map, textures, tex_data);
    primitives.push_back(p);
  }
  // skipping plane as difficult to handle infinite distance : use 2 triangles instead
}

inline int build_flat_bvh_recursive(std::vector<GpuPrimitive> &primitives, std::vector<GpuBVHNode> &nodes, int start, int end) {
  int index = (int)nodes.size();
  nodes.push_back({}); // nothing but reserving a space

  GpuBVHNode &node = nodes[index];

  // compute the AABB over primitives from start to end
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
  std::unordered_map<Texture*,  int> tex_map;

  // collect all prims and mats
  collect_primitives(&cpu_root, flat.primitives, mat_map, flat.materials, tex_map, flat.textures, flat.tex_data);
  if (flat.primitives.empty()) {
    std::cerr << "no gpu prims collected";
    return flat;
  }

  std::cout << "GPU scene: " << flat.primitives.size() << " primitives, " << flat.materials.size() << " materials, " << flat.textures.size() << " textures (" << flat.tex_data.size() / 1024 << " KB)\n";

  // build flat bvh over collected prims
  flat.bvh_nodes.reserve(flat.primitives.size() * 2);
  flat.root = build_flat_bvh_recursive(flat.primitives, flat.bvh_nodes, 0, (int)flat.primitives.size());
  std::cout << "GPU BVH nodes: " << flat.bvh_nodes.size() << "\n";
  
  return flat;
}