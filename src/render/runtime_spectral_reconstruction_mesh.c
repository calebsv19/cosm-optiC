#include "runtime_spectral_reconstruction_internal.h"
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

static double axis(Vec3 p, int a) { return a == 0 ? p.x : a == 1 ? p.y : p.z; }
static bool equal(Vec3 a, Vec3 b) {
  return a.x == b.x && a.y == b.y && a.z == b.z;
}
static double distance(SpectralMeshPlane p, Vec3 x) {
  return vec3_dot(p.n, x) - p.d;
}
bool spectral_mesh_admit(SpectralReconstruction *r, const RuntimeScene3D *s,
                         int primitive) {
  SpectralMeshBoundary *m = &r->domain.mesh;
  if (m->count)
    return spectral_reconstruction_fail(
        r, "continuous mesh admits one convex obstacle");
  const RuntimeTriangle3D *tri[128];
  int nt = 0;
  Vec3 center = {0};
  m->lo = vec3(INFINITY, INFINITY, INFINITY);
  m->hi = vec3(-INFINITY, -INFINITY, -INFINITY);
  for (int i = 0; i < s->triangleMesh.triangleCount; i++)
    if (s->triangleMesh.triangles[i].primitiveIndex == primitive) {
      if (nt == 128)
        return spectral_reconstruction_fail(
            r, "continuous mesh triangle budget exceeded");
      const RuntimeTriangle3D *t = tri[nt++] = &s->triangleMesh.triangles[i];
      Vec3 p[] = {t->p0, t->p1, t->p2};
      for (int j = 0; j < 3; j++) {
        if (!isfinite(p[j].x) || !isfinite(p[j].y) || !isfinite(p[j].z))
          goto invalid;
        center = vec3_add(center, p[j]);
        m->lo = vec3(fmin(m->lo.x, p[j].x), fmin(m->lo.y, p[j].y),
                     fmin(m->lo.z, p[j].z));
        m->hi = vec3(fmax(m->hi.x, p[j].x), fmax(m->hi.y, p[j].y),
                     fmax(m->hi.z, p[j].z));
      }
    }
  if (nt < 4)
    goto invalid;
  center = vec3_scale(center, 1.0 / (3 * nt));
  double extent = vec3_length(vec3_sub(m->hi, m->lo));
  double eps = 128 * DBL_EPSILON * fmax(extent, vec3_length(center));
  for (int i = 0; i < nt; i++) {
    Vec3 p[] = {tri[i]->p0, tri[i]->p1, tri[i]->p2};
    Vec3 normal = vec3_cross(vec3_sub(p[1], p[0]), vec3_sub(p[2], p[0]));
    double length = vec3_length(normal);
    if (!(length > eps * extent))
      goto invalid;
    normal = vec3_scale(normal, 1 / length);
    if (vec3_dot(normal, vec3_sub(center, p[0])) > 0)
      normal = vec3_scale(normal, -1);
    SpectralMeshPlane plane = {normal, vec3_dot(normal, p[0])};
    if (!(distance(plane, center) < -eps))
      goto invalid;
    for (int j = 0; j < nt; j++) {
      Vec3 q[] = {tri[j]->p0, tri[j]->p1, tri[j]->p2};
      for (int k = 0; k < 3; k++)
        if (distance(plane, q[k]) > eps)
          goto invalid;
    }
    for (int e = 0; e < 3; e++) {
      int matches = 0;
      for (int j = 0; j < nt; j++) {
        Vec3 q[] = {tri[j]->p0, tri[j]->p1, tri[j]->p2};
        for (int k = 0; k < 3; k++)
          if ((equal(p[e], q[k]) && equal(p[(e + 1) % 3], q[(k + 1) % 3])) ||
              (equal(p[e], q[(k + 1) % 3]) && equal(p[(e + 1) % 3], q[k])))
            matches++;
      }
      if (matches != 2)
        goto invalid;
    }
    bool found = false;
    for (int j = 0; j < m->count; j++)
      if (vec3_length(vec3_sub(m->plane[j].n, normal)) < 1e-12 &&
          fabs(m->plane[j].d - plane.d) <= eps)
        found = true;
    if (!found) {
      if (m->count == SPECTRAL_MESH_FACES)
        goto invalid;
      m->plane[m->count++] = plane;
    }
  }
  return true;
invalid:
  return spectral_reconstruction_fail(
      r, "continuous mesh requires a closed nondegenerate convex mesh with at "
         "most eight faces");
}
bool spectral_mesh_weights(const SpectralMeshBoundary *m, Vec3 p,
                           double w[SPECTRAL_MESH_PARTS]) {
  memset(w, 0, SPECTRAL_MESH_PARTS * sizeof(*w));
  w[0] = 1;
  if (!m->count)
    return true;
  double sum = 0;
  for (int i = 0; i < m->count; i++)
    sum += (w[i + 1] = fmax(0, distance(m->plane[i], p)));
  if (!(sum > 0))
    return false;
  for (int i = 1; i <= m->count; i++)
    w[i] /= sum;
  return true;
}
unsigned spectral_mesh_node_parts(const RuntimeSpectralVolume *v,
                                  const int id[3], int level) {
  const SpectralMeshBoundary *m = &v->reconstruction->domain.mesh;
  if (!m->count)
    return 1;
  double h = v->config.cell_m * (level ? 2 : 1);
  Vec3 lo, hi;
  lo = vec3(fmax(v->lo.x, v->lo.x + (id[0] - 1) * h),
            fmax(v->lo.y, v->lo.y + (id[1] - 1) * h),
            fmax(v->lo.z, v->lo.z + (id[2] - 1) * h));
  hi = vec3(fmin(v->hi.x, v->lo.x + (id[0] + 1) * h),
            fmin(v->hi.y, v->lo.y + (id[1] + 1) * h),
            fmin(v->hi.z, v->lo.z + (id[2] + 1) * h));
  unsigned mask = 0;
  for (int i = 0; i < m->count; i++) {
    Vec3 n = m->plane[i].n;
    Vec3 near = vec3(n.x >= 0 ? lo.x : hi.x, n.y >= 0 ? lo.y : hi.y,
                     n.z >= 0 ? lo.z : hi.z);
    Vec3 far = vec3(n.x >= 0 ? hi.x : lo.x, n.y >= 0 ? hi.y : lo.y,
                    n.z >= 0 ? hi.z : lo.z);
    if (distance(m->plane[i], near) >= 0)
      return 1; /* Entire support lies outside one supporting face. */
    if (distance(m->plane[i], far) > 0)
      mask |= 1u << (i + 1);
  }
  return mask;
}

/* Clip each linear half of the tensor tent into face-sign cells. On each
 * convex cell the partition weight is a linear/linear rational function.
 * Integrate tetrahedra with positive Duffy/Gauss quadrature and refinement. */
enum { MAX_FACES = 24, MAX_VERTICES = 32 };
typedef struct {
  int count;
  Vec3 p[MAX_VERTICES];
} Face;
typedef struct {
  int count;
  Face f[MAX_FACES];
} Poly;
static bool append(Face *f, Vec3 p) {
  if (f->count == MAX_VERTICES)
    return false;
  f->p[f->count++] = p;
  return true;
}
static Poly box(Vec3 lo, Vec3 hi) {
  Poly p = {0};
  Vec3 v[8];
  for (int i = 0; i < 8; i++)
    v[i] = vec3(i & 1 ? hi.x : lo.x, i & 2 ? hi.y : lo.y, i & 4 ? hi.z : lo.z);
  const int face[6][4] = {{0, 1, 3, 2}, {4, 6, 7, 5}, {0, 4, 5, 1},
                          {2, 3, 7, 6}, {0, 2, 6, 4}, {1, 5, 7, 3}};
  p.count = 6;
  for (int i = 0; i < 6; i++)
    for (int j = 0; j < 4; j++)
      append(&p.f[i], v[face[i][j]]);
  return p;
}
static bool clip(Poly *poly, SpectralMeshPlane plane) {
  Poly out = {0};
  Face cap = {0};
  for (int f = 0; f < poly->count; f++) {
    Face next = {0};
    const Face *old = &poly->f[f];
    for (int i = 0; i < old->count; i++) {
      Vec3 a = old->p[i], b = old->p[(i + 1) % old->count];
      double da = distance(plane, a), db = distance(plane, b);
      if (da <= 0 && !append(&next, a))
        return false;
      if (da == 0) {
        bool seen = false;
        for (int j = 0; j < cap.count; j++)
          if (vec3_length(vec3_sub(cap.p[j], a)) < 1e-14)
            seen = true;
        if (!seen && !append(&cap, a))
          return false;
      }
      if ((da < 0 && db > 0) || (da > 0 && db < 0)) {
        Vec3 q = vec3_add(a, vec3_scale(vec3_sub(b, a), da / (da - db)));
        if (!append(&next, q))
          return false;
        bool seen = false;
        for (int j = 0; j < cap.count; j++)
          if (vec3_length(vec3_sub(cap.p[j], q)) < 1e-14)
            seen = true;
        if (!seen && !append(&cap, q))
          return false;
      }
    }
    if (next.count >= 3) {
      if (out.count == MAX_FACES)
        return false;
      out.f[out.count++] = next;
    }
  }
  if (cap.count >= 3) {
    Vec3 c = {0};
    for (int j = 0; j < cap.count; j++)
      c = vec3_add(c, cap.p[j]);
    c = vec3_scale(c, 1.0 / cap.count);
    Vec3 u = vec3_normalize(vec3_sub(cap.p[0], c)), v = vec3_cross(plane.n, u);
    double angle[MAX_VERTICES];
    for (int j = 0; j < cap.count; j++) {
      Vec3 q = vec3_sub(cap.p[j], c);
      angle[j] = atan2(vec3_dot(q, v), vec3_dot(q, u));
    }
    for (int j = 1; j < cap.count; j++)
      for (int k = j; k > 0 && angle[k] < angle[k - 1]; k--) {
        double a = angle[k];
        angle[k] = angle[k - 1];
        angle[k - 1] = a;
        Vec3 q = cap.p[k];
        cap.p[k] = cap.p[k - 1];
        cap.p[k - 1] = q;
      }
    if (out.count == MAX_FACES)
      return false;
    out.f[out.count++] = cap;
  }
  *poly = out;
  return true;
}
typedef struct {
  SpectralReconstruction *r;
  const double *left, *peak, *right;
  int part;
} Integrand;
static double value(const Integrand *f, Vec3 p) {
  double w[SPECTRAL_MESH_PARTS];
  if (!spectral_mesh_weights(&f->r->domain.mesh, p, w))
    return 0;
  double result = w[f->part];
  for (int a = 0; a < 3; a++) {
    double x = axis(p, a),
           t = x <= f->peak[a] ? (x - f->left[a]) / (f->peak[a] - f->left[a])
                               : (f->right[a] - x) / (f->right[a] - f->peak[a]);
    result *= fmax(0, fmin(1, t));
  }
  return result;
}
static double gauss(const Integrand *f, const Vec3 input[4], int n) {
  Vec3 t[4];
  double denominator[4];
  memcpy(t, input, sizeof(t));
  for (int i = 0; i < 4; i++) {
    denominator[i] = 0;
    for (int j = 0; j < f->r->domain.mesh.count; j++)
      denominator[i] += fmax(0, distance(f->r->domain.mesh.plane[j], t[i]));
  }
  for (int i = 1; i < 4; i++)
    for (int j = i; j > 0 && denominator[j] < denominator[j - 1]; j--) {
      Vec3 p = t[j];
      t[j] = t[j - 1];
      t[j - 1] = p;
      double d = denominator[j];
      denominator[j] = denominator[j - 1];
      denominator[j - 1] = d;
    }
  const double x2[] = {.1127016653792583, .5, .8872983346207417},
               w2[] = {.2777777777777778, .4444444444444444, .2777777777777778};
  const double x4[] = {.046910077030668, .23076534494715845, .5,
                       .7692346550528415, .953089922969332},
               w4[] = {.11846344252809455, .23931433524968325,
                       .28444444444444444, .23931433524968325,
                       .11846344252809455};
  const double *x = n == 3 ? x2 : x4, *w = n == 3 ? w2 : w4;
  Vec3 e1 = vec3_sub(t[1], t[0]), e2 = vec3_sub(t[2], t[0]),
       e3 = vec3_sub(t[3], t[0]);
  double det = fabs(vec3_dot(e1, vec3_cross(e2, e3))), sum = 0;
  for (int i = 0; i < n; i++)
    for (int j = 0; j < n; j++)
      for (int k = 0; k < n; k++) {
        double u = x[i] * (1 - x[j]), v = x[i] * x[j] * (1 - x[k]),
               z = x[i] * x[j] * x[k];
        Vec3 p = vec3_add(
            t[0], vec3_add(vec3_scale(e1, u),
                           vec3_add(vec3_scale(e2, v), vec3_scale(e3, z))));
        sum += w[i] * w[j] * w[k] * x[i] * x[i] * x[j] * value(f, p);
      }
  return det * sum;
}
typedef struct {
  Vec3 t[4];
  double value, error;
} QuadratureCell;
static QuadratureCell estimate(const Integrand *f, const Vec3 t[4]) {
  QuadratureCell c;
  memcpy(c.t, t, sizeof(c.t));
  double a = gauss(f, t, 3);
  c.value = gauss(f, t, 5);
  c.error = fabs(a - c.value);
  return c;
}
static void heap_push(QuadratureCell *heap, int *count, QuadratureCell c) {
  int i = (*count)++;
  while (i) {
    int p = (i - 1) / 2;
    if (heap[p].error >= c.error)
      break;
    heap[i] = heap[p];
    i = p;
  }
  heap[i] = c;
}
static QuadratureCell heap_pop(QuadratureCell *heap, int *count) {
  QuadratureCell out = heap[0], last = heap[--*count];
  int i = 0;
  while (2 * i + 1 < *count) {
    int child = 2 * i + 1;
    if (child + 1 < *count && heap[child + 1].error > heap[child].error)
      child++;
    if (last.error >= heap[child].error)
      break;
    heap[i] = heap[child];
    i = child;
  }
  heap[i] = last;
  return out;
}
static double tetra(const Integrand *f, const Vec3 t[4], int depth,
                    double tol) {
  (void)depth;
  /* A total error budget does not demand a fixed relative tolerance from
   * vanishing tetrahedra at a face/edge/corner singularity. Bounded scratch. */
  if (!f->r->mesh_quadrature) {
    f->r->mesh_quadrature_bytes = 8192 * sizeof(QuadratureCell);
    f->r->mesh_quadrature = spectral_reconstruction_resize(
        f->r, NULL, 0, f->r->mesh_quadrature_bytes);
    if (!f->r->mesh_quadrature)
      return 0;
  }
  QuadratureCell *heap = f->r->mesh_quadrature;
  int count = 0;
  QuadratureCell initial = estimate(f, t);
  double sum = initial.value, error = initial.error;
  heap_push(heap, &count, initial);
  while (error > tol) {
    if (f->r->error[0])
      return 0;
    if (count >= 8191 || ++f->r->normalization_visits > 64000000) {
      spectral_reconstruction_fail(
          f->r, "continuous mesh normalization quadrature budget exceeded");
      return 0;
    }
    QuadratureCell parent = heap_pop(heap, &count);
    int i = 0, j = 1;
    double best = 0;
    for (int x = 0; x < 4; x++)
      for (int y = x + 1; y < 4; y++) {
        Vec3 d = vec3_sub(parent.t[x], parent.t[y]);
        double n = vec3_dot(d, d);
        if (n > best) {
          best = n;
          i = x;
          j = y;
        }
      }
    Vec3 mid = vec3_scale(vec3_add(parent.t[i], parent.t[j]), .5), a[4], b[4];
    memcpy(a, parent.t, sizeof(a));
    memcpy(b, parent.t, sizeof(b));
    a[i] = mid;
    b[j] = mid;
    QuadratureCell ca = estimate(f, a), cb = estimate(f, b);
    sum += ca.value + cb.value - parent.value;
    error = fmax(0, error + ca.error + cb.error - parent.error);
    heap_push(heap, &count, ca);
    heap_push(heap, &count, cb);
  }
  return sum;
}
static double cells(const Integrand *f, const Poly *p, int plane, double tol) {
  const SpectralMeshBoundary *m = &f->r->domain.mesh;
  if (!p->count)
    return 0;
  if (++f->r->normalization_visits > 64000000) {
    spectral_reconstruction_fail(
        f->r, "continuous normalization work budget exceeded");
    return 0;
  }
  if (plane < m->count) {
    double lo = INFINITY, hi = -INFINITY;
    for (int i = 0; i < p->count; i++)
      for (int j = 0; j < p->f[i].count; j++) {
        double d = distance(m->plane[plane], p->f[i].p[j]);
        lo = fmin(lo, d);
        hi = fmax(hi, d);
      }
    if (lo >= 0 || hi <= 0)
      return cells(f, p, plane + 1, tol);
    Poly a = *p, b = *p;
    SpectralMeshPlane flip = {vec3_scale(m->plane[plane].n, -1),
                              -m->plane[plane].d};
    if (!clip(&a, m->plane[plane]) || !clip(&b, flip)) {
      spectral_reconstruction_fail(
          f->r, "continuous mesh clipping capacity exceeded");
      return 0;
    }
    return cells(f, &a, plane + 1, tol * .5) +
           cells(f, &b, plane + 1, tol * .5);
  }
  Vec3 center = {0};
  int count = 0;
  for (int i = 0; i < p->count; i++)
    for (int j = 0; j < p->f[i].count; j++) {
      center = vec3_add(center, p->f[i].p[j]);
      count++;
    }
  if (!count)
    return 0;
  center = vec3_scale(center, 1.0 / count);
  double weights[SPECTRAL_MESH_PARTS];
  if (!spectral_mesh_weights(m, center, weights) || weights[f->part] == 0)
    return 0;
  int triangles = 0;
  for (int i = 0; i < p->count; i++)
    triangles += p->f[i].count - 2;
  double sum = 0;
  for (int i = 0; i < p->count; i++)
    for (int j = 1; j + 1 < p->f[i].count; j++) {
      Vec3 t[] = {center, p->f[i].p[0], p->f[i].p[j], p->f[i].p[j + 1]};
      sum += tetra(f, t, 0, tol / triangles);
    }
  return sum;
}
double spectral_mesh_integral(SpectralReconstruction *r, const double left[3],
                              const double peak[3], const double right[3],
                              Vec3 lo, Vec3 hi, int part) {
  Integrand f = {r, left, peak, right, part};
  double sum = 0;
  double scale = fmax(0, fmin(hi.x, right[0]) - fmax(lo.x, left[0])) *
                 fmax(0, fmin(hi.y, right[1]) - fmax(lo.y, left[1])) *
                 fmax(0, fmin(hi.z, right[2]) - fmax(lo.z, left[2]));
  for (int corner = 0; corner < 8; corner++) {
    Vec3 a = vec3(fmax(lo.x, corner & 1 ? peak[0] : left[0]),
                  fmax(lo.y, corner & 2 ? peak[1] : left[1]),
                  fmax(lo.z, corner & 4 ? peak[2] : left[2]));
    Vec3 b = vec3(fmin(hi.x, corner & 1 ? right[0] : peak[0]),
                  fmin(hi.y, corner & 2 ? right[1] : peak[1]),
                  fmin(hi.z, corner & 4 ? right[2] : peak[2]));
    if (a.x >= b.x || a.y >= b.y || a.z >= b.z)
      continue;
    Poly p = box(a, b);
    sum += cells(&f, &p, 0, scale * 1e-6 / 8);
  }
  return sum;
}

bool spectral_mesh_camera_interval(const RuntimeSpectralVolume *v, Ray3D ray,
                                   double a, double b) {
  if (!v->reconstruction || !v->reconstruction->domain.mesh.count)
    return false;
  const SpectralMeshBoundary *m = &v->reconstruction->domain.mesh;
  Vec3 x = vec3_add(ray.origin, vec3_scale(ray.direction, a)),
       y = vec3_add(ray.origin, vec3_scale(ray.direction, b));
  double h = 4 * v->config.cell_m;
  for (int i = 0; i < m->count; i++) {
    SpectralMeshPlane plane = m->plane[i];
    double margin = h * (fabs(plane.n.x) + fabs(plane.n.y) + fabs(plane.n.z));
    if (fmin(distance(plane, x), distance(plane, y)) > margin)
      return false;
  }
  return fmax(x.x, y.x) >= m->lo.x - h && fmin(x.x, y.x) <= m->hi.x + h &&
         fmax(x.y, y.y) >= m->lo.y - h && fmin(x.y, y.y) <= m->hi.y + h &&
         fmax(x.z, y.z) >= m->lo.z - h && fmin(x.z, y.z) <= m->hi.z + h;
}
