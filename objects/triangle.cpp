#include "triangle.h"
#include "../materials/material.h"
#include <iostream>

// triangle.cpp
Triangle::Triangle(Vec3 a, Vec3 b, Vec3 c, std::shared_ptr<Material> mat, Vec3 uv0, Vec3 uv1, Vec3 uv2) : v0(a), v1(b), v2(c), mat(mat), uv0(uv0), uv1(uv1), uv2(uv2), has_vertex_normals(false) {}

Triangle::Triangle(Vec3 a, Vec3 b, Vec3 c, std::shared_ptr<Material> mat, Vec3 uv0, Vec3 uv1, Vec3 uv2, Vec3 n0, Vec3 n1, Vec3 n2) : v0(a), v1(b), v2(c), mat(mat), uv0(uv0), uv1(uv1), uv2(uv2), n0(n0), n1(n1), n2(n2), has_vertex_normals(true) {}


// using Moller-Trumbore algorithm with TBN
bool Triangle::hit(const Ray &ray, double t_min, double t_max, HitRecord &rec) const {
  const double EPSILON = 1e-8;

  Vec3 edge1 = v1 - v0;
  Vec3 edge2 = v2 - v0;
  Vec3 h = ray.direction.cross(edge2);
  double a = edge1.dot(h);
  if (fabs(a) < EPSILON) return false;

  double f = 1.0 / a;
  Vec3 s = ray.origin - v0;
  double bary_u = f * s.dot(h);
  if (bary_u < 0.0 || bary_u > 1.0) return false;

  Vec3 q = s.cross(edge1);
  double bary_v = f * ray.direction.dot(q);
  if (bary_v < 0.0 || bary_u + bary_v > 1.0) return false;

  double t = f * edge2.dot(q);
  if (t < t_min || t > t_max) return false;

  double bary_w = 1.0 - bary_u - bary_v;
  rec.t = t;
  rec.point = ray.at(t);
  rec.u = bary_w * uv0.x + bary_u * uv1.x + bary_v * uv2.x;
  rec.v = bary_w * uv0.y + bary_u * uv1.y + bary_v * uv2.y;

  // shading normal 
  Vec3 shading_normal;
  if (has_vertex_normals) shading_normal = (n0 * bary_w + n1 * bary_u + n2 * bary_v).normalize();
  else shading_normal = edge1.cross(edge2).normalize();
  // uv gradients for normal mapping
  Vec3 duv1 = uv1 - uv0;
  Vec3 duv2 = uv2 - uv0;
  double det = duv1.x * duv2.y - duv1.y * duv2.x;

  rec.has_tbn = false;
  if (fabs(det) > EPSILON) {
    double inv_det = 1.0 / det;
    Vec3 raw_tangent   = (edge1 * duv2.y - edge2 * duv1.y) * inv_det;
    Vec3 raw_bitangent = (edge2 * duv1.x - edge1 * duv2.x) * inv_det;
 
    // Gram-Schmidt orthogonalize tangent against the shading normal
    Vec3 tangent = (raw_tangent - shading_normal * shading_normal.dot(raw_tangent)).normalize();
    Vec3 bitangent = shading_normal.cross(tangent);
 
    // ensure right-handedness: flip bitangent if it disagrees with raw
    if (bitangent.dot(raw_bitangent) < 0.0) bitangent = bitangent * -1;

 
    rec.tangent = tangent;
    rec.bitangent = bitangent;
    rec.has_tbn = true;
  }
 
  // apply normal map if the material has one and we have a valid TBN
  if (rec.has_tbn && mat && mat->normal_map) {
    Vec3 map_sample = mat->normal_map->value(rec.u, rec.v, rec.point, rec.t);
    // convert from [0,1] texture space to [-1,1] tangent space
    Vec3 tangent_normal( map_sample.x * 2.0 - 1.0, map_sample.y * 2.0 - 1.0, map_sample.z * 2.0 - 1.0);
    // transform tangent-space normal to world space
    shading_normal = (rec.tangent * tangent_normal.x + rec.bitangent * tangent_normal.y + shading_normal * tangent_normal.z).normalize();
  }

  // DEBUG: add this right before rec.set_face_normal
  if (mat && mat->normal_map) {
    Vec3 test = mat->normal_map->value(rec.u, rec.v, rec.point, rec.t);
    std::cerr << "NORMAL MAP HIT: uv=(" << rec.u << "," << rec.v 
              << ") sample=(" << test.x << "," << test.y << "," << test.z 
              << ") has_tbn=" << rec.has_tbn << "\n";
    // only print once then exit
    static int count = 0;
    if (++count > 5) exit(0);
  }

  rec.set_face_normal(ray, shading_normal);
  rec.mat = mat;
  return true;
}

bool Triangle::bounding_box(AABB &output_box) const {
  Vec3 big(fmax(v0.x ,fmax(v1.x, v2.x)), fmax(v0.y ,fmax(v1.y, v2.y)), fmax(v0.z ,fmax(v1.z, v2.z)));
  Vec3 small(fmin(v0.x ,fmin(v1.x, v2.x)), fmin(v0.y ,fmin(v1.y, v2.y)), fmin(v0.z ,fmin(v1.z, v2.z)));

  // padding for 0 h prevention
  double pad = 1e-4;
  if (big.x - small.x < pad) { small.x -= pad; big.x += pad; }
  if (big.y - small.y < pad) { small.y -= pad; big.y += pad; }
  if (big.z - small.z < pad) { small.z -= pad; big.z += pad; }

  output_box = AABB(small, big);
  return true;
}