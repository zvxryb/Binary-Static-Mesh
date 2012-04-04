#ifndef LIBBSM_H
#define LIBBSM_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef float float32_t;

typedef struct bsm_bbox {
  float32_t x0, y0, z0;
  float32_t x1, y1, z1;
} bsm_bbox_t;

typedef struct bsm_bsphere {
  float32_t x, y, z, radius;
} bsm_bsphere_t;

typedef struct bsm_header_v1 {
  int32_t magic[4];
  int32_t version;
  int32_t extension;
  bsm_bsphere_t bsphere;
  bsm_bbox_t bbox;
  int32_t num_verts;
  int32_t offs_positions;
  int32_t offs_texcoords;
  int32_t offs_normals;
  int32_t offs_tangents;
  int32_t num_tris;
  int32_t offs_tris;
  int32_t num_meshes;
  int32_t offs_meshes;
  int32_t num_hullverts;
  int32_t offs_hullverts;
  int32_t num_hulls;
  int32_t offs_hulls;
  int32_t num_visverts;
  int32_t offs_visverts;
  int32_t num_vistris;
  int32_t offs_vistris;
} bsm_header_v1_t;

/* if we were to define an extension to the v1 format, it would go something like this:
typedef struct bsm_header_ext_adj {
  bsm_header_v1_t header_v1;
  int32_t num_adjacency;
  int32_t offs_adjacency;
  etc...
} bsm_header_ext_adj_t;
*/

extern const int32_t bsm_magic[4];

typedef struct bsm_position {
  float32_t x, y, z;
} bsm_position_t;

typedef struct bsm_texcoord {
  float32_t u, v;
} bsm_texcoord_t;

typedef struct bsm_normal {
  float32_t x, y, z;
} bsm_normal_t;

typedef struct bsm_tangent {
  float32_t x, y, z, handedness;
} bsm_tangent_t;

typedef struct bsm_triangle {
  int32_t index[3];
} bsm_triangle_t;

typedef struct bsm_mesh {
  int32_t idx_tris;
  int32_t num_tris;
  uint8_t material[256];
} bsm_mesh_t;

typedef struct bsm_hullvert {
  float32_t x, y, z;
} bsm_hullvert_t;

typedef struct bsm_hull {
  int32_t idx_vert;
  int32_t num_vert;
} bsm_hull_t;

typedef struct bsm_visvert {
  float32_t x, y, z;
} bsm_visvert_t;

typedef struct bsm_vistri {
  int32_t index[3];
} bsm_vistri_t;

/* reads a header from a raw data buffer -- returns true if file is a valid BSM-format model, false if not */
bool bsm_read_header_v1(uint8_t *data, size_t n, bsm_header_v1_t *header);

size_t bsm_positions_bytes(bsm_header_v1_t *header);
size_t bsm_texcoords_bytes(bsm_header_v1_t *header);
size_t bsm_normals_bytes(bsm_header_v1_t *header);
size_t bsm_tangents_bytes(bsm_header_v1_t *header);
bool bsm_read_positions(uint8_t *data, size_t n, bsm_header_v1_t *header, bsm_position_t *positions);
bool bsm_read_texcoords(uint8_t *data, size_t n, bsm_header_v1_t *header, bsm_texcoord_t *texcoords);
bool bsm_read_normals(uint8_t *data, size_t n, bsm_header_v1_t *header, bsm_normal_t *normals);
bool bsm_read_tangents(uint8_t *data, size_t n, bsm_header_v1_t *header, bsm_tangent_t *tangents);

size_t bsm_tris_bytes(bsm_header_v1_t *header);
bool bsm_read_tris(uint8_t *data, size_t n, bsm_header_v1_t *header, bsm_triangle_t *tris);

size_t bsm_meshes_bytes(bsm_header_v1_t *header);
bool bsm_read_meshes(uint8_t *data, size_t n, bsm_header_v1_t *header, bsm_mesh_t *meshes);

size_t bsm_hullverts_bytes(bsm_header_v1_t *header);
size_t bsm_hulls_bytes(bsm_header_v1_t *header);
size_t bsm_visverts_bytes(bsm_header_v1_t *header);
size_t bsm_vistris_bytes(bsm_header_v1_t *header);
bool bsm_read_hullverts(uint8_t *data, size_t n, bsm_header_v1_t *header, bsm_hullvert_t *hullverts);
bool bsm_read_hulls(uint8_t *data, size_t n, bsm_header_v1_t *header, bsm_hull_t *hulls);
bool bsm_read_visverts(uint8_t *data, size_t n, bsm_header_v1_t *header, bsm_visvert_t *visverts);
bool bsm_read_vistris(uint8_t *data, size_t n, bsm_header_v1_t *header, bsm_vistri_t *vistris);

#endif /* LIBBSM_H */