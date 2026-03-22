#include "sphere.h"

Sphere::Sphere(Vec3 c, double r, std::shared_ptr<Material> m) : center(c), radius(r), mat(m) {}

bool Sphere::hit(const Ray &ray, double t_min, double t_max, HitRecord &rec) const {
  Vec3 oc = ray.origin - center;

  double a = ray.direction.dot(ray.direction);
  double b = oc.dot(ray.direction);
  double c = oc.dot(oc) - radius*radius;

  double discriminant = b*b - a*c;

  if (discriminant > 0) {
    double t = (-b - sqrt(discriminant)) / a;
    if (t <= t_min || t >= t_max) {
      t = (-b + sqrt(discriminant)) / a;  // try far root
      if (t <= t_min || t >= t_max) return false;
    }

    rec.t = t;
    rec.mat = mat;
    rec.point = ray.at(t);
    
    Vec3 outward_normal = (rec.point - center) / radius;
    rec.set_face_normal(ray, outward_normal);
    get_sphere_uv(outward_normal, rec.u, rec.v);

    double phi   = rec.u * 2.0 * PI - PI;   // atan2(-z, x) + PI was used in get_sphere_uv
    double theta = rec.v * PI;                // acos(-y) was used
 
    double sin_phi   = sin(phi);
    double cos_phi   = cos(phi);
    double sin_theta = sin(theta);
    double cos_theta = cos(theta);
 
    // Tangent (dP/dphi direction, normalized)
    Vec3 tangent(-sin_phi * sin_theta, 0.0, cos_phi * sin_theta);
    double tangent_len = tangent.norm();
 
    if (tangent_len > 1e-6) {
      tangent = tangent / tangent_len;
    } else {
      // Degenerate at poles — pick arbitrary tangent perpendicular to normal
      Vec3 arbitrary = (fabs(outward_normal.x) > 0.9) ? Vec3(0,1,0) : Vec3(1,0,0);
      tangent = outward_normal.cross(arbitrary).normalize();
    }
 
    // Bitangent = normal x tangent (right-handed)
    Vec3 shading_normal = rec.front_face ? outward_normal : outward_normal * -1;
    Vec3 bitangent = shading_normal.cross(tangent).normalize();
 
    rec.tangent = tangent;
    rec.bitangent = bitangent;
    rec.has_tbn = true;
 
    // ---- Apply bump map (grayscale heightmap -> perturbed normal) ----
    if (mat && mat->bump_map) {
      double du = 1.0 / 1024.0;
      double dv = 1.0 / 1024.0;
 
      auto sample_h = [&](double su, double sv) -> double {
        Vec3 c = mat->bump_map->value(su, sv, rec.point);
        return (c.x + c.y + c.z) / 3.0;
      };
 
      double h_center = sample_h(rec.u, rec.v);
      double h_right  = sample_h(rec.u + du, rec.v);
      double h_up     = sample_h(rec.u, rec.v + dv);
 
      double dh_du = (h_right - h_center) / du * mat->bump_strength;
      double dh_dv = (h_up    - h_center) / dv * mat->bump_strength;
 
      Vec3 mapped = (shading_normal - tangent * dh_du - bitangent * dh_dv).normalize();
 
      if (mapped.dot(outward_normal) < 0.01) {
        mapped = (mapped + outward_normal * 0.1).normalize();
      }
 
      shading_normal = mapped;
      rec.normal = rec.front_face ? shading_normal : shading_normal * -1;
    }
 
    // ---- Apply normal map (tangent-space RGB -> perturbed normal) ----
    if (mat && mat->normal_map) {
      Vec3 map_sample = mat->normal_map->value(rec.u, rec.v, rec.point, rec.t);
 
      double tn_x = (map_sample.x * 2.0 - 1.0) * mat->normal_map_strength;
      double tn_y = (map_sample.y * 2.0 - 1.0) * mat->normal_map_strength;
      double tn_z = map_sample.z * 2.0 - 1.0;
 
      Vec3 tangent_normal = Vec3(tn_x, tn_y, tn_z).normalize();
 
      Vec3 mapped = (rec.tangent * tangent_normal.x +
                     rec.bitangent * tangent_normal.y +
                     shading_normal * tangent_normal.z).normalize();
 
      if (mapped.dot(outward_normal) < 0.01) {
        mapped = (mapped + outward_normal * 0.1).normalize();
      }
 
      rec.normal = rec.front_face ? mapped : mapped * -1;
    }


    return true;
  }
  return false;
}

bool Sphere::bounding_box(AABB &output_box) const {
  output_box = AABB(center - Vec3(radius, radius, radius), center + Vec3(radius, radius, radius));
  return true;
}

void get_sphere_uv(const Vec3 &p, double &u, double &v) {
 double theta = acos(-p.y);
 double phi = atan2(-p.z, p.x) + PI;
 u = phi/ (2 * PI);
 v = theta / PI;
}