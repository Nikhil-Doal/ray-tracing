#define TINYOBJLOADER_IMPLEMENTATION
#include "obj_loader.h"
#include <fstream>
#include <map>
#include <sstream>

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

  if (!warn.empty()) std::cerr << "OBJ warn: " << warn << "\n";
  if (!err.empty())  std::cerr << "OBJ err: "  << err  << "\n";
  
  // ── FIX: tinyobjloader splits mtllib filenames on spaces, so MTL files
  //    like "Residential Buildings 001.mtl" silently fail to load.
  //    Fallback: derive the .mtl path from the .obj path and load manually.
  if (materials.empty()) {
    std::string mtl_path = path;
    auto dot = mtl_path.rfind('.');
    if (dot != std::string::npos) mtl_path = mtl_path.substr(0, dot) + ".mtl";

    std::ifstream mtl_in(mtl_path);
    if (mtl_in.good()) {
      std::map<std::string, int> mat_map_fallback;
      std::string warn2, err2;
      tinyobj::LoadMtl(&mat_map_fallback, &materials, &mtl_in, &warn2, &err2);
      if (!warn2.empty()) std::cerr << "MTL warn: " << warn2 << "\n";
      if (!err2.empty())  std::cerr << "MTL err: "  << err2  << "\n";

      std::cout << "MTL fallback loaded " << materials.size() << " materials from " << mtl_path << "\n";

      // Fix up material_ids in shapes — tinyobj assigned -1 because it never found the mtl.
      // Map usemtl names back to our freshly loaded material indices.
      // Since tinyobj already parsed usemtl directives (just couldn't resolve them),
      // material_ids are all -1.  We remap: faces with the same material_id grouping
      // get the first material (index 0) by default.  For these Blender-exported
      // buildings there are only 2 materials and the shapes correspond to usemtl groups,
      // so we assign them sequentially per shape.
      if (!materials.empty()) {
        for (size_t s = 0; s < shapes.size(); ++s) {
          // Try to match shape name to material name
          int matched_id = -1;
          for (size_t mi = 0; mi < materials.size(); ++mi) {
            // Check if shape name contains the material name or vice versa
            if (shapes[s].name.find(materials[mi].name) != std::string::npos ||
                materials[mi].name.find(shapes[s].name) != std::string::npos) {
              matched_id = (int)mi;
              break;
            }
          }

          for (auto &mid : shapes[s].mesh.material_ids) {
            if (mid < 0 || mid >= (int)materials.size()) {
              mid = (matched_id >= 0) ? matched_id : 0;
            }
          }
        }
      }
    } else {
      std::cerr << "MTL fallback failed: could not open " << mtl_path << "\n";
    }
  }

  // build materials
  std::vector<std::shared_ptr<Material>> mats;
  for (const auto &m : materials) {
    std::shared_ptr<Texture> tex;
    if (!m.diffuse_texname.empty()) tex = std::make_shared<ImageTexture>(dir + m.diffuse_texname);
    else tex = std::make_shared<SolidColor>(Vec3(m.diffuse[0], m.diffuse[1], m.diffuse[2]));
    
    // normal map texture (from MTL "norm" or "map_bump" / "bump" directive)
    std::shared_ptr<Texture> normal_tex;
    if (!m.normal_texname.empty()) {
      normal_tex = std::make_shared<ImageTexture>(dir + m.normal_texname, true);  // linear
    } else if (!m.bump_texname.empty()) {
      // many models store normal maps under bump_texname
      normal_tex = std::make_shared<ImageTexture>(dir + m.bump_texname, true);  // linear
    }
 
    std::shared_ptr<Material> mat_ptr;

    // check for metal (illum model 3 = mirror)
    if (m.illum == 3) {
      double fuzz = 1.0 - m.shininess / 1000.0;
      mat_ptr = std::make_shared<Metal>(tex, fuzz);
    } else if (m.illum == 5 || m.illum == 7) {
      double dissolve = m.dissolve;
      if (dissolve < 0.99) {
        mat_ptr = std::make_shared<Dielectric>(m.ior > 0 ? m.ior : 1.5, std::make_shared<SolidColor>(Vec3(m.transmittance[0], m.transmittance[1], m.transmittance[2])));
      } else {
        if (m.shininess > 100.0) mat_ptr = std::make_shared<Metal>(tex, 1.0 - m.shininess / 1000.0);
        else mat_ptr = std::make_shared<Lambertian>(tex);  // FIX: was missing 'else'
      }
    } else if (m.dissolve < 0.95) {
      // FIX: handle transparency for any illum model (e.g. hotel_glas with illum 1, d 0.3)
      mat_ptr = std::make_shared<Dielectric>(m.ior > 0 ? m.ior : 1.5,
          std::make_shared<SolidColor>(Vec3(m.diffuse[0], m.diffuse[1], m.diffuse[2])));
    } else {
      mat_ptr = std::make_shared<Lambertian>(tex);
    }

    // attach normal map if found
    if (normal_tex) {
      mat_ptr->set_normal_map(normal_tex);
    }
    mats.push_back(mat_ptr);
  }  

  std::cout << "OBJ loaded: " << shapes.size() << " shapes, " << mats.size() << " materials, "
            << attrib.vertices.size()/3 << " verts\n";

  // build triangles
  for(const auto &shape : shapes) {
    size_t index_offset = 0;
    for(size_t f = 0; f < shape.mesh.num_face_vertices.size(); ++f) {
      size_t fv = size_t(shape.mesh.num_face_vertices[f]);
      if (fv != 3) { index_offset += fv; continue; }

      Vec3 v[3], uv[3], vn[3];
      bool has_normals = true;
      for(size_t i = 0; i < 3; ++i) {
        tinyobj::index_t idx = shape.mesh.indices[index_offset + i];
        v[i] = Vec3(attrib.vertices[3*idx.vertex_index + 0], attrib.vertices[3*idx.vertex_index + 1], attrib.vertices[3*idx.vertex_index + 2]) * scale + offset;        
        
        if (idx.texcoord_index >= 0) uv[i] = Vec3(attrib.texcoords[2*idx.texcoord_index+0], attrib.texcoords[2*idx.texcoord_index+1], 0);
        else uv[i] = Vec3(0,0,0);

        if (idx.normal_index >= 0) {
          vn[i] = Vec3( attrib.normals[3 * idx.normal_index + 0], attrib.normals[3 * idx.normal_index + 1], attrib.normals[3 * idx.normal_index + 2]).normalize();
        } else {
          has_normals = false;
        }
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