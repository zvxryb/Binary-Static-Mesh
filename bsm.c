#include "bsm.h"

#include <assert.h>
#include <string.h>
#include <math.h>

#ifdef BIGENDIAN
#define BYTEFLIP32(x) (((x) & 0x000000FF) << 24 | ((x) & 0x0000FF00) << 8 | ((x) & 0x00FF0000) >> 8 | ((x) & 0xFF000000) >> 24)
#else
#define BYTEFLIP32(x) (x)
#endif

#define ASSERT_PACKING(x) assert(sizeof(x##_t) == x##_size);

const int32_t bsm_magic[4] = {
  0x414E4942,
  0x54535952,
  0x43495441,
  0x4853454D
};

static const size_t bsm_header_v1_size = 0x84;
static const size_t bsm_position_size  = 0x0C;
static const size_t bsm_texcoord_size  = 0x08;
static const size_t bsm_normal_size    = 0x0C;
static const size_t bsm_tangent_size   = 0x10;
static const size_t bsm_triangle_size  = 0x0C;
static const size_t bsm_mesh_size      = 0x108;
static const size_t bsm_hullvert_size  = 0x0C;
static const size_t bsm_hull_size      = 0x08;
static const size_t bsm_visvert_size   = 0x0C;
static const size_t bsm_vistri_size    = 0x0C;

static void reordercpy32(void *dst, const void *src, size_t bytes) {
  assert(bytes % 4 == 0);
  
  uint32_t *src32 = src;
  uint32_t *dst32 = dst;
  uint32_t x;
  for (int i = 0; i < bytes / 4; i++) {
    memcpy(&x, &src32[i], 4);
    x = BYTEFLIP32(x);
    dst32[i] = x;
  }
}

static void normalize_normal(bsm_normal_t *normal) {
  float m = sqrtf(normal->x * normal->x + normal->y * normal->y + normal->z * normal->z);
  normal->x /= m;
  normal->y /= m;
  normal->z /= m;
}

static void normalize_tangent(bsm_tangent_t *tangent) {
  float m = sqrtf(tangent->x * tangent->x + tangent->y * tangent->y + tangent->z * tangent->z);
  tangent->x /= m;
  tangent->y /= m;
  tangent->z /= m;
  tangent->handedness = tangent->handedness >= 0.0f ? 1.0f : -1.0f;
}

bool bsm_read_header_v1(uint8_t *data, size_t n, bsm_header_v1_t *header) {
  ASSERT_PACKING(bsm_header_v1);
  
  if (n < sizeof(bsm_header_v1_t)) return false;
  
  reordercpy32(header, data, sizeof(bsm_header_v1_t));
  
  if (header->magic[0] != bsm_magic[0]) return false;
  if (header->magic[1] != bsm_magic[1]) return false;
  if (header->magic[2] != bsm_magic[2]) return false;
  if (header->magic[3] != bsm_magic[3]) return false;
  if (header->num_verts < 0) return false;
  if (header->offs_positions < 0) return false;
  if (header->offs_texcoords < 0) return false;
  if (header->offs_normals < 0) return false;
  if (header->offs_tangents < 0) return false;
  if (header->num_tris < 0) return false;
  if (header->offs_tris < 0) return false;
  if (header->num_meshes < 0) return false;
  if (header->offs_meshes < 0) return false;
  if (header->num_hullverts < 0) return false;
  if (header->offs_hullverts < 0) return false;
  if (header->num_hulls < 0) return false;
  if (header->offs_hulls < 0) return false;
  if (header->num_visverts < 0) return false;
  if (header->offs_visverts < 0) return false;
  if (header->num_vistris < 0) return false;
  if (header->offs_vistris < 0) return false;
  if (header->num_verts * sizeof(bsm_position_t) + header->offs_positions > n) return false;
  if (header->num_verts * sizeof(bsm_texcoord_t) + header->offs_texcoords > n) return false;
  if (header->num_verts * sizeof(bsm_normal_t) + header->offs_normals > n) return false;
  if (header->num_verts * sizeof(bsm_tangent_t) + header->offs_tangents > n) return false;
  if (header->num_tris * sizeof(bsm_triangle_t) + header->offs_tris > n) return false;
  if (header->num_meshes * sizeof(bsm_mesh_t) + header->offs_meshes > n) return false;
  if (header->num_hullverts * sizeof(bsm_hullvert_t) + header->offs_hullverts > n) return false;
  if (header->num_hulls * sizeof(bsm_hull_t) + header->offs_hulls > n) return false;
  if (header->num_visverts * sizeof(bsm_visvert_t) + header->offs_visverts > n) return false;
  if (header->num_vistris * sizeof(bsm_vistri_t) + header->offs_vistris > n) return false;
  /* TODO: check for overlapping chunks */
  return true;
}

size_t bsm_positions_bytes(bsm_header_v1_t *header) {
  ASSERT_PACKING(bsm_position);
  return header->num_verts * sizeof(bsm_position_t);
}

size_t bsm_texcoords_bytes(bsm_header_v1_t *header) {
  ASSERT_PACKING(bsm_texcoord);
  return header->num_verts * sizeof(bsm_texcoord_t);
}

size_t bsm_normals_bytes(bsm_header_v1_t *header) {
  ASSERT_PACKING(bsm_normal);
  return header->num_verts * sizeof(bsm_normal_t);
}

size_t bsm_tangents_bytes(bsm_header_v1_t *header) {
  ASSERT_PACKING(bsm_tangent);
  return header->num_verts * sizeof(bsm_tangent_t);
}

bool bsm_read_positions(uint8_t *data, size_t n, bsm_header_v1_t *header, bsm_position_t *positions) {
  size_t bytes = bsm_positions_bytes(header);
  size_t offs  = header->offs_positions;
  if (offs + bytes > n) return false;
  
  reordercpy32(positions, data + offs, bytes);
  return true;
}

bool bsm_read_texcoords(uint8_t *data, size_t n, bsm_header_v1_t *header, bsm_texcoord_t *texcoords) {
  size_t bytes = bsm_texcoords_bytes(header);
  size_t offs  = header->offs_texcoords;
  if (offs + bytes > n) return false;
  
  reordercpy32(texcoords, data + offs, bytes);
  return true;
}

bool bsm_read_normals(uint8_t *data, size_t n, bsm_header_v1_t *header, bsm_normal_t *normals) {
  size_t bytes = bsm_normals_bytes(header);
  size_t offs  = header->offs_normals;
  if (offs + bytes > n) return false;
  
  reordercpy32(normals, data + offs, bytes);
  for (int i = 0; i < header->num_verts; i++) {
    normalize_normal(&normals[i]);
  }
  return true;
}

bool bsm_read_tangents(uint8_t *data, size_t n, bsm_header_v1_t *header, bsm_tangent_t *tangents) {
  size_t bytes = bsm_tangents_bytes(header);
  size_t offs  = header->offs_tangents;
  if (offs + bytes > n) return false;
  
  reordercpy32(tangents, data + offs, bytes);
  for (int i = 0; i < header->num_verts; i++) {
    normalize_tangent(&tangents[i]);
  }
  return true;
}

size_t bsm_tris_bytes(bsm_header_v1_t *header) {
  ASSERT_PACKING(bsm_triangle);
  return header->num_tris * sizeof(bsm_triangle_t);
}

bool bsm_read_tris(uint8_t *data, size_t n, bsm_header_v1_t *header, bsm_triangle_t *tris) {
  size_t bytes = bsm_tris_bytes(header);
  size_t offs  = header->offs_tris;
  if (offs + bytes > n) return false;
  
  reordercpy32(tris, data + offs, bytes);
  return true;
}

size_t bsm_meshes_bytes(bsm_header_v1_t *header) {
  ASSERT_PACKING(bsm_mesh);
  return header->num_meshes * sizeof(bsm_mesh_t);
}

bool bsm_read_meshes(uint8_t *data, size_t n, bsm_header_v1_t *header, bsm_mesh_t *meshes) {
  size_t bytes = bsm_meshes_bytes(header);
  size_t offs  = header->offs_meshes;
  if (offs + bytes > n) return false;
  
  reordercpy32(meshes, data + offs, bytes);
  return true;
}

size_t bsm_hullverts_bytes(bsm_header_v1_t *header) {
  ASSERT_PACKING(bsm_hullvert);
  return header->num_hullverts * sizeof(bsm_hullvert_t);
}

size_t bsm_hulls_bytes(bsm_header_v1_t *header) {
  ASSERT_PACKING(bsm_hull);
  return header->num_hulls * sizeof(bsm_hull_t);
}

size_t bsm_visverts_bytes(bsm_header_v1_t *header) {
  ASSERT_PACKING(bsm_visvert);
  return header->num_visverts * sizeof(bsm_visvert_t);
}

size_t bsm_vistris_bytes(bsm_header_v1_t *header) {
  ASSERT_PACKING(bsm_vistri);
  return header->num_vistris * sizeof(bsm_vistri_t);
}

bool bsm_read_hullverts(uint8_t *data, size_t n, bsm_header_v1_t *header, bsm_hullvert_t *hullverts) {
  size_t bytes = bsm_hullverts_bytes(header);
  size_t offs  = header->offs_hullverts;
  if (offs + bytes > n) return false;
  
  reordercpy32(hullverts, data + offs, bytes);
  return true;
}
  
bool bsm_read_hulls(uint8_t *data, size_t n, bsm_header_v1_t *header, bsm_hull_t *hulls) {
  size_t bytes = bsm_hulls_bytes(header);
  size_t offs  = header->offs_hulls;
  if (offs + bytes > n) return false;
  
  reordercpy32(hulls, data + offs, bytes);
  return true;
}

bool bsm_read_visverts(uint8_t *data, size_t n, bsm_header_v1_t *header, bsm_visvert_t *visverts) {
  size_t bytes = bsm_visverts_bytes(header);
  size_t offs  = header->offs_visverts;
  if (offs + bytes > n) return false;
  
  reordercpy32(visverts, data + offs, bytes);
  return true;
}

bool bsm_read_vistris(uint8_t *data, size_t n, bsm_header_v1_t *header, bsm_vistri_t *vistris) {
  size_t bytes = bsm_vistris_bytes(header);
  size_t offs  = header->offs_vistris;
  if (offs + bytes > n) return false;
  
  reordercpy32(vistris, data + offs, bytes);
  return true;
}