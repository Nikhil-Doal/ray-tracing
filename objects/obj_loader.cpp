#define TINYOBJLOADER_IMPLEMENTATION
#include "obj_loader.h"

std::vector <std::shared_ptr<Triangle>> load_obj_triangle(const std::string &path, std::shared_ptr<Material> default_mat, double scale, Vec3 offset) {
  std::vector<std::shared_ptr<Triangle>> triangles;
  tinyobj::attrib_t attrib;
  std::vector<tinyobj::shape_t> shapes;
  std::vector<tinyobj::material_t> materials;
  std::string warn, err;

  std::string dir = path.substr(0, path.find_last_of("/\\") + 1); // mtl rel paths

  if(!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, path.c_str(), dir.c_str())) {
    std::cerr << warn << err << std::endl;
    return triangles;
  }

  std::vector<std::shared_ptr<Material>> mats;
  for (const auto &m : materials) {
    std::shared_ptr<Texture> tex;
    if (!m.diffuse_texname.empty()) {
      tex = std::make_shared<ImageTexture>(dir + m.diffuse_texname);
    } else {
      tex = std::make_shared<SolidColor>(Vec3(m.diffuse[0], m.diffuse[1], m.diffuse[2]));
    }
    // check for metal (illum model 3 = mirror)
    if (m.illum == 3) {
      double fuzz = 1.0 - m.shininess / 1000.0;
      mats.push_back(std::make_shared<Metal>(tex, fuzz));
    } else if (m.illum == 5 || m.illum == 7) {
      double dissolve = m.dissolve; // 1.0 = fully opaque, 0.0 = fully transparent
      if (dissolve < 0.99) {
        mats.push_back(std::make_shared<Dielectric>(m.ior > 0 ? m.ior : 1.5, 
                      std::make_shared<SolidColor>(Vec3(m.transmittance[0], m.transmittance[1], m.transmittance[2]))));
    } else {
      mats.push_back(std::make_shared<Lambertian>(tex));
    }
  }  

  for(const auto &shape : shapes) {
    size_t index_offset = 0;
    for(size_t f = 0; f < shape.mesh.num_face_vertices.size(); ++f) {
      size_t fv = size_t(shape.mesh.num_face_vertices[f]);
      if (fv != 3) continue;

      Vec3 v[3], uv[3];
      for(size_t i = 0; i < 3; ++i) {
        tinyobj::index_t idx = shape.mesh.indices[index_offset + i];
        v[i] = Vec3(attrib.vertices[3*idx.vertex_index + 0], attrib.vertices[3*idx.vertex_index + 1], attrib.vertices[3*idx.vertex_index + 2]) * scale + offset;        
        
        if (idx.texcoord_index >= 0) uv[i] = Vec3(attrib.texcoords[2*idx.texcoord_index+0], attrib.texcoords[2*idx.texcoord_index+1], 0);
        else uv[i] = Vec3(0,0,0);
      }

      int mat_id = shape.mesh.material_ids[f];
      auto mat = (mat_id >= 0 && mat_id < (int)mats.size()) ? mats[mat_id] : default_mat;
      triangles.push_back(std::make_shared<Triangle>(v[0], v[1], v[2], mat, uv[0], uv[1], uv[2]));
      index_offset += fv;
    }
  }
  return triangles;
}