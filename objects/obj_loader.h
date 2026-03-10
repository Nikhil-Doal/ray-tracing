#pragma once
#include <iostream>
#include "mesh.h"
#include "../external/tinyobjloader/tiny_obj_loader.h"

Mesh *load_obj(const std::string &filename, Material *mat, double scale, Vec3 offset) {
  tinyobj::attrib_t attrib;
  std::vector<tinyobj::shape_t> shapes;
  std::vector<tinyobj::material_t> materials;

  std::string warn, err;
  bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filename.c_str());
  if (!warn.empty()) std::cout << warn << std::endl;
  if (!err.empty()) std::cerr << err << std::endl;
  if (!ret) {
      std::cerr << "Failed to load OBJ: " << filename << std::endl;
      return nullptr;
  }

  Mesh *mesh = new Mesh();

  for (const auto &shape : shapes) {
    size_t index_offset = 0;

    for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); ++f) {
      int fv = shape.mesh.num_face_vertices[f];
      Vec3 verts[3];

      for (size_t v = 0; v < fv; ++v) {
        tinyobj::index_t idx = shape.mesh.indices[index_offset + v];
        float vx = attrib.vertices[3*idx.vertex_index];
        float vy = attrib.vertices[3*idx.vertex_index + 1];
        float vz = attrib.vertices[3*idx.vertex_index + 2];

        Vec3 vertex(vx, vy, vz);
        vertex = vertex * scale;
        vertex = vertex + offset;

        verts[v] = vertex;
      }

      mesh -> add (new Triangle (verts[0], verts[1], verts[2], mat));
      index_offset += fv;
    }
  }

  return mesh;
}