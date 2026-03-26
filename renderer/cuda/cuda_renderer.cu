#include "cuda_renderer.cuh"
#include "cuda_scene.cuh"
#include <curand_kernel.h>
#include <cuda_runtime.h>
#include <cstdio>
#include <cmath>
#include "../../utils/image_writer.h"


// macro for error check
#define CUDA_CHECK(x) do { \
  cudaError_t e = (x); \
  if (e != cudaSuccess) { \
    printf("CUDA error: %s at %s:%d\n", cudaGetErrorString(e), __FILE__, __LINE__); \
    exit(1); \
  } \
} while (0)

#define GPU_PI 3.14159265f
#define GPU_MAX_THROUGHPUT 15.0f

// random number generator helpers
// inline to reduce overhead 
__device__ inline float rand_f(curandState *s) { return curand_uniform(s); }
__device__ inline GpuVec3 rand_in_unit_sphere(curandState *s) {
  float u = curand_uniform(s);
  GpuVec3 p(rand_f(s)*2 - 1, rand_f(s)*2 -1, rand_f(s)*2 -1);
  return p.normalize() * cbrtf(u);
}
__device__ inline GpuVec3 rand_cosine_direction(curandState *s) {
  float r1 = rand_f(s);
  float r2 = rand_f(s);
  float phi = 2.0f * GPU_PI * r1;
  float x = cosf(phi) * sqrtf(r2);
  float y = sinf(phi) * sqrtf(r2);
  float z = sqrtf(1.0f - r2);
  return {x, y, z};
}

__device__ inline GpuVec3 clamp_vec(const GpuVec3 &v, float max_val) {
  return GpuVec3(
    isfinite(v.x) ? fminf(fmaxf(v.x, 0.0f), max_val) : 0.0f,
    isfinite(v.y) ? fminf(fmaxf(v.y, 0.0f), max_val) : 0.0f,
    isfinite(v.z) ? fminf(fmaxf(v.z, 0.0f), max_val) : 0.0f
  );
}
// bilinear texture sampling
__device__ GpuVec3 sample_texture(const GpuScene &scene, int tex_id, float u, float v, const GpuVec3 &solid_color) {
  if (tex_id < 0 || tex_id >= scene.num_textures || !scene.tex_data) return solid_color;

  const GpuTexture &tex = scene.textures[tex_id];
  if (tex.width <= 0 || tex.height <= 0) return solid_color;

  // Clamp UV
  u = u - floorf(u);
  v = v - floorf(v);
  v = 1.0f - v;

  float fi = u * (tex.width  - 1);
  float fj = v * (tex.height - 1);
  int i0 = (int)fi;
  int j0 = (int)fj;
  int i1 = i0 + 1 < tex.width  ? i0 + 1 : i0;
  int j1 = j0 + 1 < tex.height ? j0 + 1 : j0;
  float tx = fi - i0;
  float ty = fj - j0;

  const unsigned char *base = scene.tex_data + tex.offset * 3;

  auto get = [&](int ii, int jj) -> GpuVec3 {
    const unsigned char *p = base + (jj * tex.width + ii) * 3;
    // sRGB → linear (approx gamma 2.2)
    // Fast sRGB→linear: x² is a close approximation to x^2.2 and 10-50× faster
    float r = (p[0] / 255.0f); r = r * r;
    float g = (p[1] / 255.0f); g = g * g;
    float b = (p[2] / 255.0f); b = b * b;
    return {r, g, b};
  };

  GpuVec3 c00 = get(i0, j0), c10 = get(i1, j0);
  GpuVec3 c01 = get(i0, j1), c11 = get(i1, j1);
  GpuVec3 c0 = c00*(1-tx) + c10*tx;
  GpuVec3 c1 = c01*(1-tx) + c11*tx;
  return c0*(1-ty) + c1*ty;
}

// Linear texture (normal maps, bump maps) - NO gamma
__device__ GpuVec3 sample_texture_linear(const GpuScene &scene, int tex_id, float u, float v) {
  if (tex_id < 0 || tex_id >= scene.num_textures || !scene.tex_data)
    return GpuVec3(0.5f, 0.5f, 1.0f);

  const GpuTexture &tex = scene.textures[tex_id];
  if (tex.width <= 0 || tex.height <= 0) return GpuVec3(0.5f, 0.5f, 1.0f);

  u = u - floorf(u);
  v = v - floorf(v);
  v = 1.0f - v;

  float fi = u * (tex.width  - 1);
  float fj = v * (tex.height - 1);
  int i0 = (int)fi, j0 = (int)fj;
  int i1 = i0 + 1 < tex.width  ? i0 + 1 : i0;
  int j1 = j0 + 1 < tex.height ? j0 + 1 : j0;
  float tx = fi - i0, ty = fj - j0;

  const unsigned char *base = scene.tex_data + tex.offset * 3;
  auto get = [&](int ii, int jj) -> GpuVec3 {
    const unsigned char *p = base + (jj * tex.width + ii) * 3;
    return {p[0] / 255.0f, p[1] / 255.0f, p[2] / 255.0f};
  };

  GpuVec3 c00 = get(i0, j0), c10 = get(i1, j0);
  GpuVec3 c01 = get(i0, j1), c11 = get(i1, j1);

  GpuVec3 c0 = c00*(1-tx) + c10*tx;
  GpuVec3 c1 = c01*(1-tx) + c11*tx;
  return c0*(1-ty) + c1*ty;
}

// Sample grayscale height from linear texture (average RGB)
__device__ float sample_height(const GpuScene &scene, int tex_id, float u, float v) {
  GpuVec3 c = sample_texture_linear(scene, tex_id, u, v);
  return (c.x + c.y + c.z) / 3.0f;
}

// HDR sky sampling (equirectangular)
__device__ GpuVec3 sample_sky(const GpuScene &scene, const GpuVec3 &dir) {
  if (!scene.sky.enabled || !scene.sky.data) {
    // fallback gradient
    GpuVec3 unit = dir.normalize();
    float t = 0.5f * (unit.y + 1.0f);
    return GpuVec3{1,1,1} * (1-t) + GpuVec3{0.5f, 0.7f, 1.0f} * t;
  }

  GpuVec3 unit = dir.normalize();
  float theta = acosf(fmaxf(-1.0f, fminf(1.0f, unit.y)));
  float phi = atan2f(unit.z, unit.x) + GPU_PI;

  float u = phi / (2.0f * GPU_PI);
  float v = theta / GPU_PI;

  float fi = u * (scene.sky.width - 1);
  float fj = v * (scene.sky.height - 1);
  int i0 = (int)fi, j0 = (int)fj;
  int i1 = i0 + 1 < scene.sky.width  ? i0 + 1 : i0;
  int j1 = j0 + 1 < scene.sky.height ? j0 + 1 : j0;
  float tx = fi - i0, ty = fj - j0;

  auto get = [&](int ii, int jj) -> GpuVec3 {
    int idx = (jj * scene.sky.width + ii) * 3;
    return {scene.sky.data[idx], scene.sky.data[idx+1], scene.sky.data[idx+2]};
  };

  GpuVec3 c00 = get(i0, j0), c10 = get(i1, j0);
  GpuVec3 c01 = get(i0, j1), c11 = get(i1, j1);
  GpuVec3 c0 = c00*(1-tx) + c10*tx;
  GpuVec3 c1 = c01*(1-tx) + c11*tx;
  return (c0*(1-ty) + c1*ty) * scene.sky.intensity;
}

// AABB hit test
__device__ bool aabb_hit(const GpuBVHNode &node, const GpuRay &ray, float t_min, float t_max) {
  for (int i = 0; i < 3; ++i) {
    float inv = 1.0f / ray.direction[i];
    float t0 = (node.aabb_min[i] - ray.origin[i]) * inv;
    float t1 = (node.aabb_max[i] - ray.origin[i]) * inv;
    if (inv < 0.0f) { float tmp = t0; t0 = t1; t1 = tmp; }
    t_min = t0 > t_min ? t0 : t_min;
    t_max = t1 < t_max ? t1 : t_max;
    if (t_max <= t_min) return false;
  }
  return true;
}

// hit sphere — with TBN for normal mapping
__device__ bool sphere_hit(const GpuSphere &s, const GpuRay &ray, float t_min, float t_max, GpuHitRecord &rec) {
  GpuVec3 oc = ray.origin - s.center;
  float a = ray.direction.dot(ray.direction);
  float b = oc.dot(ray.direction);
  float c = oc.dot(oc) - s.radius * s.radius;
  float disc = b*b - a*c;
  if (disc < 0) return false;

  float t = (-b - sqrtf(disc)) / a;
  if (t <= t_min || t >= t_max) {
    t = (-b + sqrtf(disc)) / a;  // try far root
    if (t <= t_min || t >= t_max) return false;
  }

  rec.t = t;
  rec.point = ray.at(t);
  GpuVec3 outward = (rec.point - s.center) * (1.0f / s.radius);
  rec.front_face = ray.direction.dot(outward) < 0;
  rec.normal = rec.front_face ? outward : GpuVec3{-outward.x, -outward.y, -outward.z};
  rec.geometric_normal = outward;

  // spherical uv coords
  float theta = acosf(fmaxf(-1.0f, fminf(1.0f, -outward.y)));
  float phi = atan2f(-outward.z, outward.x) + GPU_PI;
  rec.u = phi / (2.0f * GPU_PI);
  rec.v = theta / GPU_PI;

  // ---- Compute TBN from spherical parameterization ----
  float sin_phi   = sinf(rec.u * 2.0f * GPU_PI - GPU_PI);
  float cos_phi   = cosf(rec.u * 2.0f * GPU_PI - GPU_PI);
  float sin_theta = sinf(rec.v * GPU_PI);

  GpuVec3 tangent(-sin_phi * sin_theta, 0.0f, cos_phi * sin_theta);
  float tangent_len = tangent.norm();

  if (tangent_len > 1e-6f) {
    tangent = tangent * (1.0f / tangent_len);
  } else {
    // Pole degenerate — pick arbitrary perpendicular
    GpuVec3 arb = (fabsf(outward.x) > 0.9f) ? GpuVec3{0,1,0} : GpuVec3{1,0,0};
    tangent = outward.cross(arb).normalize();
  }

  GpuVec3 shading_n = rec.front_face ? outward : outward * -1.0f;
  GpuVec3 bitangent = shading_n.cross(tangent).normalize();

  rec.tangent = tangent;
  rec.bitangent = bitangent;
  rec.shading_normal = shading_n;
  rec.has_tbn = true;

  return true;
}

// hit triangle -> moller trumbore
__device__ bool triangle_hit(const GpuTriangle &tri, const GpuRay &ray, float t_min, float t_max, GpuHitRecord &rec) {
  const float EPS = 1e-8f;
  GpuVec3 e1 = tri.v1 - tri.v0;
  GpuVec3 e2 = tri.v2 - tri.v0;
  GpuVec3 h  = ray.direction.cross(e2);
  float   a  = e1.dot(h);
  if (fabsf(a) < EPS) return false;

  float   f  = 1.0f / a;
  GpuVec3 s  = ray.origin - tri.v0;
  float   bu = f * s.dot(h);
  if (bu < 0.0f || bu > 1.0f) return false;

  GpuVec3 q  = s.cross(e1);
  float   bv = f * ray.direction.dot(q);
  if (bv < 0.0f || bu + bv > 1.0f) return false;

  float t = f * e2.dot(q);
  if (t < t_min || t > t_max) return false;

  float bw = 1.0f - bu - bv;
  rec.t     = t;
  rec.point = ray.at(t);
  rec.u     = bw*tri.uv0.x + bu*tri.uv1.x + bv*tri.uv2.x;
  rec.v     = bw*tri.uv0.y + bu*tri.uv1.y + bv*tri.uv2.y;

  // Geometric normal
  GpuVec3 geo_n = e1.cross(e2).normalize();
  rec.front_face = ray.direction.dot(geo_n) < 0;
  rec.geometric_normal = geo_n;

  // Shading normal (smooth or flat)
  GpuVec3 shading_n;
  if (tri.has_vertex_normals) {
    shading_n = (tri.n0 * bw + tri.n1 * bu + tri.n2 * bv).normalize();
  } else {
    shading_n = geo_n;
  }
  if (shading_n.dot(geo_n) < 0) shading_n = shading_n * -1.0f;

  // Compute TBN from UV gradients
  GpuVec3 duv1(tri.uv1.x - tri.uv0.x, tri.uv1.y - tri.uv0.y, 0);
  GpuVec3 duv2(tri.uv2.x - tri.uv0.x, tri.uv2.y - tri.uv0.y, 0);
  float det = duv1.x * duv2.y - duv1.y * duv2.x;

  rec.has_tbn = false;
  if (fabsf(det) > EPS) {
    float inv_det = 1.0f / det;
    GpuVec3 raw_tangent   = (e1 * duv2.y - e2 * duv1.y) * inv_det;
    GpuVec3 raw_bitangent = (e2 * duv1.x - e1 * duv2.x) * inv_det;

    // Gram-Schmidt orthogonalize
    GpuVec3 tangent = raw_tangent - shading_n * shading_n.dot(raw_tangent);
    if (tangent.norm() < 0.001f) {
      GpuVec3 arb = (fabsf(shading_n.x) > 0.9f) ? GpuVec3{0,1,0} : GpuVec3{1,0,0};
      tangent = shading_n.cross(arb);
    }
    tangent = tangent.normalize();

    GpuVec3 bitangent = shading_n.cross(tangent).normalize();
    if (bitangent.dot(raw_bitangent) < 0.0f) bitangent = bitangent * -1.0f;

    rec.tangent = tangent;
    rec.bitangent = bitangent;
    rec.shading_normal = shading_n;
    rec.has_tbn = true;
  }

  rec.normal = rec.front_face ? shading_n : shading_n * -1.0f;
  return true;
}

// Apply bump map (grayscale heightmap -> perturbed normal via finite differences)
__device__ void apply_bump_map(const GpuScene &scene, GpuHitRecord &rec) {
  if (!rec.has_tbn) return;
  const GpuMaterial &mat = scene.materials[rec.mat_id];
  if (mat.bump_tex_id < 0) return;

  float du = 1.0f / 1024.0f;
  float dv = 1.0f / 1024.0f;

  float h_center = sample_height(scene, mat.bump_tex_id, rec.u,      rec.v);
  float h_right  = sample_height(scene, mat.bump_tex_id, rec.u + du, rec.v);
  float h_up     = sample_height(scene, mat.bump_tex_id, rec.u,      rec.v + dv);

  float dh_du = (h_right - h_center) / du * mat.bump_strength;
  float dh_dv = (h_up    - h_center) / dv * mat.bump_strength;

  GpuVec3 mapped = (rec.shading_normal - rec.tangent * dh_du - rec.bitangent * dh_dv).normalize();

  if (mapped.dot(rec.geometric_normal) < 0.01f) {
    mapped = (mapped + rec.geometric_normal * 0.1f).normalize();
  }

  // Update shading normal for subsequent normal map application
  rec.shading_normal = mapped;
  rec.normal = rec.front_face ? mapped : mapped * -1.0f;
}

// Apply normal map (tangent-space RGB -> perturbed normal)
__device__ void apply_normal_map(const GpuScene &scene, GpuHitRecord &rec) {
  if (!rec.has_tbn) return;
  const GpuMaterial &mat = scene.materials[rec.mat_id];
  if (mat.normal_tex_id < 0) return;

  GpuVec3 map_sample = sample_texture_linear(scene, mat.normal_tex_id, rec.u, rec.v);

  float tn_x = (map_sample.x * 2.0f - 1.0f) * mat.normal_map_strength;
  float tn_y = (map_sample.y * 2.0f - 1.0f) * mat.normal_map_strength;
  float tn_z = map_sample.z * 2.0f - 1.0f;

  GpuVec3 tn = GpuVec3(tn_x, tn_y, tn_z).normalize();

  GpuVec3 mapped = (rec.tangent * tn.x + rec.bitangent * tn.y + rec.shading_normal * tn.z).normalize();

  if (mapped.dot(rec.geometric_normal) < 0.01f) {
    mapped = (mapped + rec.geometric_normal * 0.1f).normalize();
  }

  rec.normal = rec.front_face ? mapped : mapped * -1.0f;
}

// BVH traversal -> stack based to prevent recursion limit on gpu
__device__ bool bvh_hit(const GpuScene &scene, const GpuRay &ray, float t_min, float t_max, GpuHitRecord &rec) {
  int stack[128]; // hardcoded should be fine for most cases
  int top = 0;
  if (scene.bvh_root < 0 || scene.bvh_root >= scene.num_bvh_nodes) return false;
  if (!aabb_hit(scene.bvh_nodes[scene.bvh_root], ray, t_min, t_max)) return false;
  stack[top++] = scene.bvh_root;

  bool hit_anything = false;
  float closest = t_max;

  while (top > 0) {
    int idx = stack[--top];
    const GpuBVHNode &node = scene.bvh_nodes[idx];
    if (node.prim_count > 0) {
      // Leaf — intersect primitives
      for (int i = node.prim_start; i < node.prim_start + node.prim_count; ++i) {
        GpuHitRecord tmp{};
        bool hit = false;

        if (scene.primitives[i].type == GpuGeomType::SPHERE)
          hit = sphere_hit(scene.primitives[i].sphere, ray, t_min, closest, tmp);
        else
          hit = triangle_hit(scene.primitives[i].triangle, ray, t_min, closest, tmp);

        if (hit) {
          hit_anything = true;
          closest = tmp.t;
          rec = tmp;
          rec.mat_id = scene.primitives[i].mat_id;
        }
      }
    } else {
        int left  = node.left;
        int right = node.right;

        bool hit_left  = false;
        bool hit_right = false;

        if (left >= 0)
          hit_left = aabb_hit(scene.bvh_nodes[left], ray, t_min, closest);
        if (right >= 0)
          hit_right = aabb_hit(scene.bvh_nodes[right], ray, t_min, closest);

        if (hit_left && hit_right) {
          // Push far child first, near child last (popped first)
          // Use centroid midpoint of each AABB along ray to approximate entry
          const GpuBVHNode &ln = scene.bvh_nodes[left];
          const GpuBVHNode &rn = scene.bvh_nodes[right];
          float tenter_l = fmaxf(fmaxf(
            fminf((ln.aabb_min.x - ray.origin.x) / ray.direction.x,
                  (ln.aabb_max.x - ray.origin.x) / ray.direction.x),
            fminf((ln.aabb_min.y - ray.origin.y) / ray.direction.y,
                  (ln.aabb_max.y - ray.origin.y) / ray.direction.y)),
            fminf((ln.aabb_min.z - ray.origin.z) / ray.direction.z,
                  (ln.aabb_max.z - ray.origin.z) / ray.direction.z));
          float tenter_r = fmaxf(fmaxf(
            fminf((rn.aabb_min.x - ray.origin.x) / ray.direction.x,
                  (rn.aabb_max.x - ray.origin.x) / ray.direction.x),
            fminf((rn.aabb_min.y - ray.origin.y) / ray.direction.y,
                  (rn.aabb_max.y - ray.origin.y) / ray.direction.y)),
            fminf((rn.aabb_min.z - ray.origin.z) / ray.direction.z,
                  (rn.aabb_max.z - ray.origin.z) / ray.direction.z));
          if (tenter_l < tenter_r) {
            if (top < 126) { stack[top++] = right; stack[top++] = left; }
          } else {
            if (top < 126) { stack[top++] = left; stack[top++] = right; }
          }
        }
        else if (hit_left)  { if (top < 127) stack[top++] = left; }
        else if (hit_right) { if (top < 127) stack[top++] = right; }
    }
  }
  
  // Apply bump map first, then normal map (same order as CPU)
  if (hit_anything) {
    apply_bump_map(scene, rec);
    apply_normal_map(scene, rec);

    // Offset along GEOMETRIC normal to prevent self-intersection
    GpuVec3 geo_off = rec.front_face ? rec.geometric_normal : rec.geometric_normal * -1.0f;
    rec.point = rec.point + geo_off * 1e-4f;
  }

  return hit_anything;
}

// check for scatter based on material
__device__ bool mat_scatter(const GpuScene &scene, const GpuMaterial &mat, const GpuRay &ray_in, const GpuHitRecord &rec, GpuVec3 &attenuation, GpuRay &scattered, curandState *rng) {
  switch(mat.type) {
    case GpuMatType::LAMBERTIAN: {
      // build ONB around normal
      GpuVec3 w = rec.normal;
      GpuVec3 a = (fabsf(w.x) > 0.9f) ? GpuVec3{0,1,0} : GpuVec3{1,0,0};
      GpuVec3 v = w.cross(a).normalize();
      GpuVec3 u = w.cross(v);
      // cosine weighted direction
      GpuVec3 d = rand_cosine_direction(rng);
      GpuVec3 dir = u*d.x + v*d.y + w*d.z;
      scattered = {rec.point, dir};
      attenuation = sample_texture(scene, mat.albedo_tex_id, rec.u, rec.v, mat.albedo);
      return true;
    } 
    case GpuMatType::GLOSSY: {
      bool do_specular = (rand_f(rng) < mat.specular_strength);
      if (do_specular) {
        GpuVec3 unit_in = ray_in.direction.normalize();
        float d = unit_in.dot(rec.normal);
        GpuVec3 reflected = unit_in - rec.normal * (2.0f * d);
        // GGX-like lobe around reflected direction
        GpuVec3 w = unit_in - rec.normal * (2.0f * unit_in.dot(rec.normal));
        GpuVec3 a_vec = (fabsf(w.x) > 0.9f) ? GpuVec3{0,1,0} : GpuVec3{1,0,0};
        GpuVec3 v = w.cross(a_vec).normalize();
        GpuVec3 u = w.cross(v);
  
        float r1 = rand_f(rng);
        float r2 = rand_f(rng);
        float a2 = mat.roughness * mat.roughness;
        float cos_theta = sqrtf((1.0f - r1) / (1.0f + (a2 * a2 - 1.0f) * r1));
        float sin_theta = sqrtf(1.0f - cos_theta * cos_theta);
        float phi = 2.0f * GPU_PI * r2;
  
        GpuVec3 half_vec = (u * (sin_theta * cosf(phi)) + v * (sin_theta * sinf(phi)) + w * cos_theta).normalize();
        GpuVec3 spec_dir = unit_in - half_vec * (2.0f * unit_in.dot(half_vec));
  
        if (spec_dir.dot(rec.normal) <= 0) {
          // Below surface, fall back to diffuse
          GpuVec3 dw = rec.normal;
          GpuVec3 da = (fabsf(dw.x) > 0.9f) ? GpuVec3{0,1,0} : GpuVec3{1,0,0};
          GpuVec3 dv = dw.cross(da).normalize();
          GpuVec3 du = dw.cross(dv);
          GpuVec3 dd = rand_cosine_direction(rng);
          spec_dir = du*dd.x + dv*dd.y + dw*dd.z;
        }
  
        scattered = {rec.point, spec_dir};
        float cos_i = fmaxf(0.0f, half_vec.dot(spec_dir.normalize()));
        float f0 = 0.04f;
        float fresnel = f0 + (1.0f - f0) * powf(1.0f - cos_i, 5.0f);
  
        attenuation = GpuVec3{fresnel, fresnel, fresnel};
      } else {
        GpuVec3 w = rec.normal;
        GpuVec3 a = (fabsf(w.x) > 0.9f) ? GpuVec3{0,1,0} : GpuVec3{1,0,0};
        GpuVec3 v = w.cross(a).normalize();
        GpuVec3 u = w.cross(v);
        GpuVec3 d = rand_cosine_direction(rng);
        GpuVec3 dir = u*d.x + v*d.y + w*d.z;
        scattered = {rec.point, dir};
        attenuation = sample_texture(scene, mat.albedo_tex_id, rec.u, rec.v, mat.albedo);
      }
      return true;
    }
  
    case GpuMatType::METAL: {
      GpuVec3 unit = ray_in.direction.normalize();
      float d = unit.dot(rec.normal);
      GpuVec3 refl = unit - rec.normal * (2.0f * d);
      GpuVec3 fuzz = rand_in_unit_sphere(rng) * mat.fuzz;
      scattered = {rec.point + rec.normal * 1e-4f, refl + fuzz};
      attenuation = sample_texture(scene, mat.albedo_tex_id, rec.u, rec.v, mat.albedo);
      return scattered.direction.dot(rec.normal) > 0;
    } 
    
    case GpuMatType::DIELECTRIC: {
      attenuation = {1,1,1};
      float ratio = rec.front_face ? (1.0f/mat.ir) : mat.ir;
      GpuVec3 unit = ray_in.direction.normalize();
      float cos_t = fminf(-unit.dot(rec.normal), 1.0f);
      float sin_t = sqrtf(1.0f - cos_t*cos_t);
      bool cannot = ratio * sin_t > 1.0f;
      // Schlick approximation
      float r0 = (1-ratio)/(1+ratio); r0 = r0*r0;
      float refl = r0 + (1-r0)*powf(1-cos_t, 5);
      GpuVec3 dir;
      if (cannot || refl > rand_f(rng)) dir = unit - rec.normal * (2.0f * unit.dot(rec.normal));
      else {
        GpuVec3 perp = (unit + rec.normal * cos_t) * ratio;
        GpuVec3 para = rec.normal * (-sqrtf(fabsf(1.0f - perp.dot(perp))));
        dir = perp + para;
      }
  
      GpuVec3 offset_normal = rec.front_face ? rec.normal * -1e-4f : rec.normal * 1e-4f;
      scattered = {rec.point + offset_normal, dir};
      return true;
    }
  
    case GpuMatType::DIFFUSE_LIGHT: {
      return false;
    }

    default:
      return false;
  }
}

// MIS helpers
// scattering pdf for lambertian and glossy
__device__ float scattering_pdf(const GpuMaterial &mat, const GpuHitRecord &rec, const GpuRay &scattered) {
  if (mat.type == GpuMatType::LAMBERTIAN) {
    float cos_t = rec.normal.dot(scattered.direction.normalize());
    return cos_t < 0 ? 0.0f : cos_t / GPU_PI;
  }
  if (mat.type == GpuMatType::GLOSSY) {
    float cos_t = rec.normal.dot(scattered.direction.normalize());
    float diffuse_pdf = cos_t < 0 ? 0.0f : cos_t / GPU_PI;
    // No artificial floor — return 0 for pure specular paths
    if (diffuse_pdf <= 0.0f) return 0.0f;
    return diffuse_pdf * (1.0f - mat.specular_strength);
  }
  return 0.0f;
}

// power heuristic
__device__ float power_heuristic(float pdf_a, float pdf_b) {
  float a2 = pdf_a * pdf_a;
  float b2 = pdf_b * pdf_b;
  return (a2 + b2) < 1e-10f ? 0.0f : a2 / (a2 + b2);
}

// direct light NEE + MIS
__device__ GpuVec3 direct_light(const GpuScene &scene, const GpuHitRecord &rec, const GpuRay &ray_in, curandState *rng) {
  GpuVec3 result(0,0,0);
  const GpuMaterial &mat = scene.materials[rec.mat_id];

  for (int i = 0; i < scene.num_lights; ++i) {
    const GpuLight &light = scene.lights[i];

    GpuVec3 to_light_dir;
    GpuVec3 emission;
    float dist;
    float light_pdf;

    if (light.type == GpuLightType::DIRECTIONAL) {
      to_light_dir = light.direction * -1.0f;
      dist = 1e6f;
      emission = light.color * light.intensity;
      light_pdf = 1.0f;
    } 
    else if (light.type == GpuLightType::SPHERE_AREA) {
      // cone sampling — matches total_light_pdf
      GpuVec3 oc = light.position - rec.point;
      float dist_sq = oc.dot(oc);
      float dist_c = sqrtf(dist_sq);
      float sin2_max = light.radius * light.radius / dist_sq;
      float cos_max = sqrtf(fmaxf(0.0f, 1.0f - sin2_max));
      float solid_angle = 2.0f * GPU_PI * (1.0f - cos_max);
      if (solid_angle <= 0.0f) continue;

      float r1 = rand_f(rng);
      float r2 = rand_f(rng);
      float cos_t = 1.0f + r1 * (cos_max - 1.0f);
      float sin_t = sqrtf(fmaxf(0.0f, 1.0f - cos_t * cos_t));
      float phi = 2.0f * GPU_PI * r2;

      GpuVec3 w = oc * (1.0f / dist_c);
      GpuVec3 a_vec = (fabsf(w.x) > 0.9f) ? GpuVec3{0,1,0} : GpuVec3{1,0,0};
      GpuVec3 v = w.cross(a_vec).normalize();
      GpuVec3 u = w.cross(v);
      to_light_dir = (u * (sin_t * cosf(phi)) + v * (sin_t * sinf(phi)) + w * cos_t).normalize();

      // intersect to get shadow distance
      GpuVec3 oc2 = rec.point - light.position;
      float ia = to_light_dir.dot(to_light_dir);
      float ib = oc2.dot(to_light_dir);
      float ic = oc2.dot(oc2) - light.radius * light.radius;
      float disc = ib*ib - ia*ic;
      if (disc < 0) continue;
      dist = (-ib - sqrtf(disc)) / ia;
      if (dist < 0.001f) continue;

      light_pdf = 1.0f / solid_angle;
      emission = light.color * light.intensity;
    }

    else continue;

    if (light_pdf <= 0.0f) continue;

    float cos_theta = rec.normal.dot(to_light_dir);
    if (cos_theta <= 0.0f) continue;

    // shadow ray
    GpuRay shadow{rec.point + rec.normal * 1e-3f, to_light_dir};
    GpuHitRecord shadow_rec{};
    if (bvh_hit(scene, shadow, 0.001f, dist - 0.01f, shadow_rec)) {
      if (!scene.materials[shadow_rec.mat_id].is_emissive) continue;
    }

    GpuVec3 attenuation = sample_texture(scene, mat.albedo_tex_id, rec.u, rec.v, mat.albedo);

    // MIS weight
    GpuRay to_light_ray{rec.point, to_light_dir};
    float bsdf_pdf = scattering_pdf(mat, rec, to_light_ray);
    float mis = (bsdf_pdf > 0) ? power_heuristic(light_pdf, bsdf_pdf) : 1.0f;

    GpuVec3 contribution = emission * attenuation * (cos_theta * mis / (light_pdf * GPU_PI));
    result = result + clamp_vec(contribution, GPU_MAX_THROUGHPUT);
  }
  return result;
}

// for emmisve hits, total light calculation
__device__ float total_light_pdf(const GpuScene &scene, const GpuVec3 &from, const GpuVec3 &dir) {
  float total = 0.0f;
  for (int i = 0; i < scene.num_lights; ++i) {
    const GpuLight &light = scene.lights[i];
    if (light.type == GpuLightType::SPHERE_AREA) {
      GpuVec3 oc = from - light.position;
      float   a  = dir.dot(dir);
      float   b  = oc.dot(dir);
      float   c  = oc.dot(oc) - light.radius * light.radius;
      float disc = b*b - a*c;
      if (disc < 0) continue;
      float val = 1.0f - light.radius*light.radius / oc.dot(oc);
      if (val <= 0) continue;
      float cos_max    = sqrtf(val);
      float solid_angle = 2.0f * GPU_PI * (1.0f - cos_max);
      total += (solid_angle > 0) ? 1.0f / solid_angle : 0.0f;
    }
    // directional lights are delta — pdf=0 for random directions
  }
  return total;
}

// ray color function - not recursive to prevent gpu stack overflow
__device__ GpuVec3 ray_color(GpuRay ray, const GpuScene &scene, int max_depth, curandState *rng) {
  GpuVec3 throughput{1,1,1};
  GpuVec3 result{0,0,0};
  float   prev_bsdf_pdf = -1.0f; // -1 = camera ray / specular

  for (int depth = 0; depth < max_depth; ++depth) {
    GpuHitRecord rec{};
    if (!bvh_hit(scene, ray, 0.001f, 1e30f, rec)) {
      // missed — sky gradient
      result = result + throughput * sample_sky(scene, ray.direction);
      break;
    }

    const GpuMaterial &mat = scene.materials[rec.mat_id];

    // emissive hit — MIS weighted
    if (mat.is_emissive) {
      GpuVec3 Le = rec.front_face ? sample_texture(scene, mat.emit_tex_id, rec.u, rec.v, mat.emit_color) : GpuVec3(0,0,0);
      if (prev_bsdf_pdf < 0) result = result + throughput * Le;
      else {
        // diffuse bounce — weight against NEE
        float lp  = total_light_pdf(scene, ray.origin, ray.direction);
        float mis = (lp > 0) ? power_heuristic(prev_bsdf_pdf, lp) : 1.0f;
        result = result + throughput * Le * mis;
      }
      break;
    }

    // NEE direct lighting at every diffuse bounce
    if (!mat.is_transmissive) result = result + throughput * direct_light(scene, rec, ray, rng);
    
    // scatter for next bounce
    GpuVec3 attenuation;
    GpuRay  scattered;
    if (!mat_scatter(scene, mat, ray, rec, attenuation, scattered, rng)) break;

    float bsdf_pdf = scattering_pdf(mat, rec, scattered);
    throughput = throughput * attenuation;
    prev_bsdf_pdf = (bsdf_pdf > 0) ? bsdf_pdf : -1.0f;
    ray = scattered;
    // Russian roulette after depth 3 — unbiased early termination
    if (depth > 3) {
      float p = fmaxf(throughput.x, fmaxf(throughput.y, throughput.z));
      if (p <= 0.0f) break;
      p = fminf(p, 0.95f);
      if (rand_f(rng) > p) break;
      throughput = throughput * (1.0f / p);
    }

    // Hard clamp throughput to prevent runaway energy
    throughput = clamp_vec(throughput, GPU_MAX_THROUGHPUT);
  }
  return result;
}

// Camera ray
__device__ GpuRay get_camera_ray(const GpuCamera &cam, float s, float t, curandState *rng) {
  GpuVec3 dir = cam.lower_left_corner + cam.horizontal * s + cam.vertical   * t - cam.origin;
  return {cam.origin, dir};
}

__global__ void init_rng_kernel(curandState *states, int width, int height) {
  int x = blockIdx.x * blockDim.x + threadIdx.x;
  int y = blockIdx.y * blockDim.y + threadIdx.y;
  if (x >= width || y >= height) return;
  int pixel = y * width + x;
  curand_init(1337ULL + (unsigned long long)pixel, 0, 0, &states[pixel]);
}

// Main renderer kernel
// Batch render kernel — accumulates batch_size samples into accum buffer.
// Called repeatedly from the host loop; each launch is short enough to avoid
// TDR kills and lets us download progressive results between batches.
__global__ void render_kernel(float *accum, curandState *rng_states,
                              int width, int height,
                              int batch_size, int max_depth,
                              GpuCamera camera, GpuScene scene) {
  int x = blockIdx.x * blockDim.x + threadIdx.x;
  int y = blockIdx.y * blockDim.y + threadIdx.y;
  if (x >= width || y >= height) return;

  int pixel = y * width + x;

  // Seed per pixel, sequence per batch — fast init (sequence is small),
  // statistically independent across both pixels and batches.
  curandState rng = rng_states[pixel];
  for (int s = 0; s < batch_size; ++s) {
    float u = (x + curand_uniform(&rng)) / (float)(width - 1);
    float v = (y + curand_uniform(&rng)) / (float)(height - 1);
    GpuRay ray = get_camera_ray(camera, u, v, &rng);
    GpuVec3 color = ray_color(ray, scene, max_depth, &rng);

    // Discard NaN/Inf samples entirely — prevents poisoning the accumulator
    if (!isfinite(color.x) || !isfinite(color.y) || !isfinite(color.z))
      continue;

    color = clamp_vec(color, GPU_MAX_THROUGHPUT);

    accum[pixel * 3 + 0] += color.x;
    accum[pixel * 3 + 1] += color.y;
    accum[pixel * 3 + 2] += color.z;
  }
  rng_states[pixel] = rng;
}

// Divide accumulated radiance by sample count to produce final mean image
__global__ void normalize_kernel(float *fb, const float *accum,
                                 int width, int height, int total_samples) {
  int x = blockIdx.x * blockDim.x + threadIdx.x;
  int y = blockIdx.y * blockDim.y + threadIdx.y;
  if (x >= width || y >= height) return;

  int pixel = y * width + x;
  float inv = 1.0f / (float)total_samples;
  fb[pixel * 3 + 0] = fmaxf(0.0f, accum[pixel * 3 + 0] * inv);
  fb[pixel * 3 + 1] = fmaxf(0.0f, accum[pixel * 3 + 1] * inv);
  fb[pixel * 3 + 2] = fmaxf(0.0f, accum[pixel * 3 + 2] * inv);
}


// main host entry point called in main_cuda.cu
void cuda_render(const CudaRenderParams &params, const GpuScene &host_scene, const std::vector<GpuLight> &host_lights, std::vector<float> &out_fb, const std::string &output_path) {
  int W = params.width;
  int H = params.height;
  out_fb.resize(W*H*3);

  // upload the scene to gpu
  GpuScene d_scene = host_scene;
  CUDA_CHECK(cudaMalloc(&d_scene.primitives, host_scene.num_primitives * sizeof(GpuPrimitive)));
  CUDA_CHECK(cudaMemcpy(d_scene.primitives, host_scene.primitives, host_scene.num_primitives*sizeof(GpuPrimitive), cudaMemcpyHostToDevice));

  CUDA_CHECK(cudaMalloc(&d_scene.bvh_nodes, host_scene.num_bvh_nodes * sizeof(GpuBVHNode)));
  CUDA_CHECK(cudaMemcpy(d_scene.bvh_nodes, host_scene.bvh_nodes, host_scene.num_bvh_nodes * sizeof(GpuBVHNode), cudaMemcpyHostToDevice));

  CUDA_CHECK(cudaMalloc(&d_scene.materials, host_scene.num_materials * sizeof(GpuMaterial)));
  CUDA_CHECK(cudaMemcpy(d_scene.materials, host_scene.materials, host_scene.num_materials * sizeof(GpuMaterial), cudaMemcpyHostToDevice));
  
  // adding textures
  d_scene.tex_data = nullptr;
  d_scene.textures = nullptr;
  d_scene.num_textures = host_scene.num_textures;
  if (host_scene.num_textures > 0 && host_scene.tex_data) {
    const GpuTexture &last = host_scene.textures[host_scene.num_textures - 1];
    size_t total_pixels = last.offset + last.width * last.height;
    size_t total_bytes  = total_pixels * 3;

    CUDA_CHECK(cudaMalloc(&d_scene.tex_data, total_bytes));
    CUDA_CHECK(cudaMemcpy(d_scene.tex_data, host_scene.tex_data, total_bytes, cudaMemcpyHostToDevice));

    CUDA_CHECK(cudaMalloc(&d_scene.textures, host_scene.num_textures * sizeof(GpuTexture)));
    CUDA_CHECK(cudaMemcpy(d_scene.textures, host_scene.textures, host_scene.num_textures * sizeof(GpuTexture), cudaMemcpyHostToDevice));

    printf("GPU textures: %d uploaded (%zu KB)\n", host_scene.num_textures, total_bytes/1024);
  }

  // Upload HDR sky if present
  d_scene.sky = host_scene.sky;
  d_scene.sky.data = nullptr;
  if (host_scene.sky.enabled && host_scene.sky.data) {
    size_t sky_bytes = host_scene.sky.width * host_scene.sky.height * 3 * sizeof(float);
    CUDA_CHECK(cudaMalloc(&d_scene.sky.data, sky_bytes));
    CUDA_CHECK(cudaMemcpy(d_scene.sky.data, host_scene.sky.data, sky_bytes, cudaMemcpyHostToDevice));
    printf("GPU sky: %dx%d uploaded (%zu KB)\n", host_scene.sky.width, host_scene.sky.height, sky_bytes/1024);
  }

  if (!host_lights.empty()) {
    CUDA_CHECK(cudaMalloc(&d_scene.lights, host_lights.size() * sizeof(GpuLight)));
    CUDA_CHECK(cudaMemcpy(d_scene.lights, host_lights.data(), host_lights.size() * sizeof(GpuLight), cudaMemcpyHostToDevice));
  }
  d_scene.num_lights = (int)host_lights.size();

    // output + accumulation framebuffers on GPU
  float *d_fb, *d_accum;
  size_t fb_bytes = W * H * 3 * sizeof(float);
  CUDA_CHECK(cudaMalloc(&d_fb, fb_bytes));
  CUDA_CHECK(cudaMalloc(&d_accum, fb_bytes));
  CUDA_CHECK(cudaMemset(d_accum, 0, fb_bytes));

  // launch in batches for progressive output + TDR safety
  dim3 block(16, 16);
  dim3 grid((W + 15) / 16, (H + 15) / 16);

  const int BATCH_SIZE = 4;
  int total_spp = params.samples_per_pixel;
  int num_batches = (total_spp + BATCH_SIZE - 1) / BATCH_SIZE;

  curandState *d_rng;
  CUDA_CHECK(cudaMalloc(&d_rng, W * H * sizeof(curandState)));
  init_rng_kernel<<<grid, block>>>(d_rng, W, H);
  CUDA_CHECK(cudaDeviceSynchronize());
  printf("RNG states initialized\n");

  for (int b = 0; b < num_batches; ++b) {
    int spp_this_batch = (b < num_batches - 1) ? BATCH_SIZE
                                                : total_spp - b * BATCH_SIZE;

    render_kernel<<<grid, block>>>(d_accum, d_rng, W, H,
                                   spp_this_batch, params.max_depth,
                                   params.camera, d_scene);

    CUDA_CHECK(cudaGetLastError());

    int completed = (b + 1) * BATCH_SIZE;
    if (completed < total_spp) completed = (b + 1 == num_batches) ? total_spp : completed;

    // Sync + progress every few batches (every ~128 samples)
    {      
      CUDA_CHECK(cudaDeviceSynchronize());
      int done = (b + 1 == num_batches) ? total_spp
                                        : (b + 1) * BATCH_SIZE;
      printf("Progress: %d / %d samples (%.1f%%)\n",
             done, total_spp, 100.0f * done / total_spp);

      // ---- Optional: save progressive image here ----
      if (done % 8 == 0 || b == num_batches - 1) {
        normalize_kernel<<<grid, block>>>(d_fb, d_accum, W, H, done);
        cudaDeviceSynchronize();
        cudaMemcpy(out_fb.data(), d_fb, fb_bytes, cudaMemcpyDeviceToHost);
        // ... save to disk ...
        std::vector<Vec3> fb_vec3(W * H);
        for (int i = 0; i < W * H; ++i)
          fb_vec3[i] = Vec3(out_fb[i*3], out_fb[i*3+1], out_fb[i*3+2]);
        save_png(output_path, W, H, fb_vec3, 1);
        printf("  -> Saved progressive image (%d spp)\n", done);
      }
    }
  }

  // Final normalize + download
  normalize_kernel<<<grid, block>>>(d_fb, d_accum, W, H, total_spp);
  CUDA_CHECK(cudaGetLastError());
  CUDA_CHECK(cudaDeviceSynchronize());
  CUDA_CHECK(cudaMemcpy(out_fb.data(), d_fb, fb_bytes, cudaMemcpyDeviceToHost));

  // cleanup all memory
  cudaFree(d_fb);
  cudaFree(d_accum);
  cudaFree(d_scene.primitives);
  cudaFree(d_scene.materials);
  cudaFree(d_scene.bvh_nodes);
  cudaFree(d_scene.lights);
  if (d_scene.tex_data)  cudaFree(d_scene.tex_data);
  if (d_scene.textures)  cudaFree(d_scene.textures);
  if (d_scene.sky.data) cudaFree(d_scene.sky.data);
  cudaFree(d_rng);
}
