#pragma once
#include <cuda_runtime.h>

// redefine all core features and functions for cuda 
// Vec3 - we need float not double due to limited double cores on gpus
struct GpuVec3 { // use structs since we won't be using polymorphism anyways
  float x, y, z;
  // constructors
  __host__ __device__ GpuVec3() : x(0), y(0), z(0) {}
  __host__ __device__ GpuVec3(float x, float y, float z) : x(x), y(y), z(z) {}

  // operator overloads
  __host__ __device__ GpuVec3 operator+(const GpuVec3 &v) const { return {x+v.x, y+v.y, z+v.z}; }
  __host__ __device__ GpuVec3 operator-(const GpuVec3 &v) const { return {x-v.x, y-v.y, z-v.z}; }
  __host__ __device__ GpuVec3 operator*(float t)          const { return {x*t,   y*t,   z*t};   }
  __host__ __device__ GpuVec3 operator/(float t)          const { return {x/t,   y/t,   z/t};   }
  __host__ __device__ GpuVec3 operator*(const GpuVec3 &v) const { return {x*v.x, y*v.y, z*v.z}; }
  __host__ __device__ GpuVec3 operator-()                 const { return {-x, -y, -z}; }

  // custom functions
  __host__ __device__ float dot(const GpuVec3 &v) const { return x*v.x + y*v.y + z*v.z; }
  __host__ __device__ GpuVec3 cross(const GpuVec3 &v) const {return {y*v.z - z*v.y, z*v.x - x*v.z, x*v.y - y*v.x};}
  __host__ __device__ float norm() const { return sqrtf(x*x + y*y + z*z); }
  __host__ __device__ GpuVec3 normalize() const { float n = norm(); return {x/n, y/n, z/n}; }
  __host__ __device__ float operator[](int i) const { return i==0?x : i==1?y : z; }
};
// commutativity for multiplication (lhs for ease)
__host__ __device__ inline GpuVec3 operator*(float t, const GpuVec3 &v) {return {t*v.x, t*v.y, t*v.z};}

// Ray 
struct GpuRay {
  GpuVec3 origin;
  GpuVec3 direction;
  __host__ __device__ GpuVec3 at(float t) const{ return origin + direction * t; }
};

// Material 
enum class GpuMatType : uint8_t { LAMBERTIAN, METAL, DIELECTRIC, DIFFUSE_LIGHT }; // using class to keep scoped
struct GpuMaterial {
  GpuMatType type;
  GpuVec3 albedo; // lamb + met col
  float fuzz; // met shine
  float ir; // idx refract dielectric
  GpuVec3 emit_color; // diffuse light emission color
  bool is_emissive;
  bool is_transmissive;
};

enum class GpuGeomType : uint8_t { SPHERE, TRIANGLE };
struct GpuSphere {
  GpuVec3 center;
  float   radius;
};
struct GpuTriangle {
  GpuVec3 v0, v1, v2;
  GpuVec3 uv0, uv1, uv2;
};

struct GpuPrimitive {
  GpuGeomType type; // which union member is active - only one active at a time
  int mat_id;  
  union {
    GpuSphere sphere;
    GpuTriangle triangle;
  };
};

// Flat BVH Node - we cannot use shared ptrs with cuda
struct GpuBVHNode {
  GpuVec3 aabb_min;
  GpuVec3 aabb_max;
  int left; // child index, == -1 if leaf
  int right; // child index, == -1 if leaf
  int prim_start; // leaf: first primitive index
  int prim_count; // leaf: number of primitives (0 -> internal node)
};

// HitRecord
struct GpuHitRecord {
  GpuVec3 point;
  GpuVec3 normal;
  float t;
  float u, v;
  int mat_id;
  bool front_face;
};

enum class GpuLightType : uint8_t { DIRECTIONAL, POINT, SPHERE_AREA };
struct GpuLight {
  GpuLightType type;
  GpuVec3 color;
  float intensity;
  GpuVec3 direction;
  GpuVec3 position;
  float radius;
};

// Camera
struct GpuCamera {
  GpuVec3 origin;
  GpuVec3 lower_left_corner;
  GpuVec3 horizontal;
  GpuVec3 vertical;
  float lens_radius;
};


// Full scene to be passed to GPU kernel - at this point all the pointers are gpu device ptrs after upload
struct GpuScene {
  GpuPrimitive *primitives;
  int num_primitives;

  GpuBVHNode *bvh_nodes;
  int num_bvh_nodes;
  int bvh_root;

  GpuMaterial *materials;
  int num_materials;

  GpuLight *lights;
  int num_lights;
};