#define TINYOBJLOADER_IMPLEMENTATION
#include "obj_loader.h"
#include "../materials/glossy.h"

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
  
  // build materials
  std::vector<std::shared_ptr<Material>> mats;
  for (const auto &m : materials) {
    std::shared_ptr<Texture> tex;
    if (!m.diffuse_texname.empty()) {
      std::string tex_path = m.diffuse_texname;
      if (tex_path[0] != '/' && (tex_path.size() < 2 || tex_path[1] != ':')) tex_path = dir + tex_path;
      tex = std::make_shared<ImageTexture>(tex_path);
    } 
    else tex = std::make_shared<SolidColor>(Vec3(m.diffuse[0], m.diffuse[1], m.diffuse[2]));
    
    // normal map texture (from MTL "norm" or "map_bump" / "bump" directive)
    std::shared_ptr<Texture> normal_tex;
    if (!m.normal_texname.empty()) {
      std::string npath = m.normal_texname;
      if (npath[0] != '/' && (npath.size() < 2 || npath[1] != ':')) npath = dir + npath;
      normal_tex = std::make_shared<ImageTexture>(npath, true);
    } else if (!m.bump_texname.empty()) {
      std::string bpath = m.bump_texname;
      if (bpath[0] != '/' && (bpath.size() < 2 || bpath[1] != ':')) bpath = dir + bpath;
      normal_tex = std::make_shared<ImageTexture>(bpath, true);
    }
 
    std::shared_ptr<Material> mat_ptr;

    // check for metal (illum model 3 = mirror)
    if (m.illum == 3) {
      // Mirror metal
      double fuzz = 1.0 - m.shininess / 1000.0;
      mat_ptr = std::make_shared<Metal>(tex, fuzz);
    } 
    
    else if (m.illum == 5 || m.illum == 7) {
      // Glass/transparent
      double dissolve = m.dissolve;
      if (dissolve < 0.99) mat_ptr = std::make_shared<Dielectric>(m.ior > 0 ? m.ior : 1.5, std::make_shared<SolidColor>(Vec3(m.transmittance[0], m.transmittance[1], m.transmittance[2])));
      else mat_ptr = std::make_shared<Lambertian>(tex);
    } 
    
    else if (m.shininess > 10.0 && m.illum >= 2) {
      double roughness = 1.0 - std::min(m.shininess / 500.0, 1.0);
      roughness = std::max(roughness, 0.05); // never perfectly smooth
      // Map shininess to specular strength (higher = more specular)
      double spec = std::min(m.shininess / 200.0, 0.5);
      spec = std::max(spec, 0.05);
      mat_ptr = std::make_shared<Glossy>(tex, roughness, spec);
    } 
    
    else {
      mat_ptr = std::make_shared<Lambertian>(tex);
    }


    // attach normal map if found
    if (normal_tex) {
      mat_ptr->set_normal_map(normal_tex);
    }
    mats.push_back(mat_ptr);
  }  

  // build triangles
  for(const auto &shape : shapes) {
    size_t index_offset = 0;
    for(size_t f = 0; f < shape.mesh.num_face_vertices.size(); ++f) {
      size_t fv = size_t(shape.mesh.num_face_vertices[f]);
      if (fv != 3) {
        index_offset += fv;
        continue;
      }

      Vec3 v[3], uv[3], vn[3];
      bool has_normals = true;
      for(size_t i = 0; i < 3; ++i) {
        tinyobj::index_t idx = shape.mesh.indices[index_offset + i];
        v[i] = Vec3(attrib.vertices[3*idx.vertex_index + 0], attrib.vertices[3*idx.vertex_index + 1], attrib.vertices[3*idx.vertex_index + 2]) * scale + offset;        
        
        if (idx.texcoord_index >= 0) uv[i] = Vec3(attrib.texcoords[2*idx.texcoord_index+0], attrib.texcoords[2*idx.texcoord_index+1], 0);
        else uv[i] = Vec3(0,0,0);

        if (idx.normal_index >= 0) vn[i] = Vec3( attrib.normals[3 * idx.normal_index + 0], attrib.normals[3 * idx.normal_index + 1], attrib.normals[3 * idx.normal_index + 2]).normalize();
        else has_normals = false;
      }

      int mat_id = shape.mesh.material_ids[f];
      auto mat = (mat_id >= 0 && mat_id < (int)mats.size()) ? mats[mat_id] : default_mat;
      if (has_normals) triangles.push_back(std::make_shared<Triangle>(v[0], v[1], v[2], mat, uv[0], uv[1], uv[2], vn[0], vn[1], vn[2]));
      else triangles.push_back(std::make_shared<Triangle>(v[0], v[1], v[2], mat, uv[0], uv[1], uv[2]));
      index_offset += fv;
    }
  }
  return triangles;
}