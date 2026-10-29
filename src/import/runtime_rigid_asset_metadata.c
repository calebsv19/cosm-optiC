#include "runtime_rigid_asset_metadata.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static bool tail(const char *line, int at) {
  if (at <= 0)
    return false;
  for (const char *p = line + at; *p; ++p)
    if (!isspace((unsigned char)*p))
      return false;
  return true;
}
static bool values(const char *p, size_t count, double *out) {
  for (size_t i = 0; i < count; ++i) {
    char *end = NULL;
    out[i] = strtod(p, &end);
    if (end == p || !isfinite(out[i]))
      return false;
    p = end;
  }
  while (isspace((unsigned char)*p))
    ++p;
  return *p == 0;
}
bool RuntimeRigidCompoundMetadata(const char *bytes, size_t size,
                                  uint32_t *children, char source_id[64],
                                  char source_sha256[65], double center[3],
                                  double basis[9], double moments[3]) {
  if (!bytes || !size || size > 1024u * 1024u || memchr(bytes, 0, size))
    return false;
  const char *magic = "ball_collision_representation_artifact_v1\n";
  size_t start = strlen(magic);
  if (size < start || memcmp(bytes, magic, start))
    return false;
  unsigned mask = 0;
  size_t parts = 0;
  uint32_t count = 0;
  while (start < size) {
    const char *end = memchr(bytes + start, '\n', size - start);
    size_t n = end ? (size_t)(end - (bytes + start)) : size - start;
    if (n > 1023)
      return false;
    char line[1024];
    memcpy(line, bytes + start, n);
    line[n] = 0;
    int at = 0;
    unsigned bit = 0;
    if (!strncmp(line, "compound ", 9)) {
      char id[64];
      unsigned v;
      if (sscanf(line, "compound %63s %u%n", id, &v, &at) != 2 ||
          !tail(line, at) || v < 1 || v > 12)
        return false;
      count = v;
      bit = 1;
    } else if (!strncmp(line, "source ", 7)) {
      if (sscanf(line, "source %63s %64s%n", source_id, source_sha256, &at) !=
              2 ||
          !tail(line, at) || strlen(source_sha256) != 64)
        return false;
      for (size_t i = 0; i < 64; ++i)
        if (!((source_sha256[i] >= '0' && source_sha256[i] <= '9') ||
              (source_sha256[i] >= 'a' && source_sha256[i] <= 'f')))
          return false;
      bit = 2;
    } else if (!strncmp(line, "frame_center ", 13)) {
      if (!values(line + 13, 3, center))
        return false;
      bit = 4;
    } else if (!strncmp(line, "frame_matrix ", 13)) {
      if (!values(line + 13, 9, basis))
        return false;
      bit = 8;
    } else if (!strncmp(line, "frame_moments ", 14)) {
      if (!values(line + 14, 3, moments))
        return false;
      bit = 16;
    } else if (!strncmp(line, "part ", 5)) {
      char child[64], id[64];
      size_t vertices, faces;
      if (sscanf(line, "part %63s %63s %zu %zu", child, id, &vertices,
                 &faces) != 4 ||
          vertices < 4 || vertices > 256 || faces < 4 || faces > 512)
        return false;
      if (++parts > 12)
        return false;
    }
    if (bit && (mask & bit))
      return false;
    mask |= bit;
    start += n + (end ? 1 : 0);
  }
  if (mask != 31 || parts != count)
    return false;
  for (size_t i = 0; i < 3; ++i)
    if (moments[i] <= 0)
      return false;
  *children = count;
  return true;
}
