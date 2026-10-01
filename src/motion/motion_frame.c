#include "motion/motion_frame.h"
#include <math.h>
#include <string.h>
static double dot(const double *a, const double *b) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}
static bool norm(double *a) {
  double n = sqrt(dot(a, a));
  if (!isfinite(n) || n < 1e-12)
    return false;
  for (int i = 0; i < 3; ++i)
    a[i] /= n;
  return true;
}
static void cross(const double *a, const double *b, double *c) {
  double t[3] = {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2],
                 a[0] * b[1] - a[1] * b[0]};
  memcpy(c, t, sizeof(t));
}
bool MotionFrameValid(const MotionFrame *f) {
  if (!f)
    return false;
  const double *v[3] = {f->forward, f->up, f->right};
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 3; ++j)
      if (!isfinite(v[i][j]))
        return false;
    if (fabs(dot(v[i], v[i]) - 1) > 1e-6)
      return false;
    for (int j = i + 1; j < 3; ++j)
      if (fabs(dot(v[i], v[j])) > 1e-6)
        return false;
  }
  double right[3];
  cross(f->up, f->forward, right);
  return dot(right, f->right) > 1 - 1e-6;
}
bool MotionFrameSeed(MotionFrame *out, const double forward[3],
                     const double up[3]) {
  MotionFrame f;
  memcpy(f.forward, forward, sizeof(f.forward));
  if (!norm(f.forward))
    return false;
  memcpy(f.up, up, sizeof(f.up));
  double d = dot(f.up, f.forward);
  for (int i = 0; i < 3; ++i)
    f.up[i] -= d * f.forward[i];
  if (!norm(f.up)) {
    int k = 0;
    for (int i = 1; i < 3; ++i)
      if (fabs(f.forward[i]) < fabs(f.forward[k]))
        k = i;
    memset(f.up, 0, sizeof(f.up));
    f.up[k] = 1;
    d = dot(f.up, f.forward);
    for (int i = 0; i < 3; ++i)
      f.up[i] -= d * f.forward[i];
    norm(f.up);
  }
  cross(f.up, f.forward, f.right);
  norm(f.right);
  cross(f.forward, f.right, f.up);
  *out = f;
  return true;
}
bool MotionFrameTransport(const MotionFrame *from, const double forward[3],
                          MotionFrame *out) {
  double f[3];
  memcpy(f, forward, sizeof(f));
  if (!norm(f))
    return false;
  double axis[3];
  cross(from->forward, f, axis);
  double c = fmax(-1, fmin(1, dot(from->forward, f))), u[3];
  if (c < -1 + 1e-10) { /* Exact reversal: deterministic half-turn around prior
                           up. */
    memcpy(u, from->up, sizeof(u));
  } else {
    double v[3], w[3];
    cross(axis, from->up, v);
    cross(axis, v, w);
    for (int i = 0; i < 3; ++i)
      u[i] = from->up[i] + v[i] + w[i] / (1 + c);
  }
  return MotionFrameSeed(out, f, u);
}
void MotionFrameRoll(MotionFrame *f, double angle) {
  double c = cos(angle), s = sin(angle), u[3];
  for (int i = 0; i < 3; ++i)
    u[i] = c * f->up[i] - s * f->right[i];
  memcpy(f->up, u, sizeof(u));
  cross(f->up, f->forward, f->right);
}
void MotionFrameModelRotation(const MotionFrame *f, int axis,
                              const double offset[3], double euler[3]) {
  double mf[3] = {0}, mu[3] = {0}, mr[3];
  mf[axis / 2] = (axis % 2) ? -1 : 1;
  mu[axis / 2 == 1 ? 2 : 1] = 1;
  cross(mu, mf, mr);
  double x = offset[0] * 0.017453292519943295,
         y = offset[1] * 0.017453292519943295,
         z = offset[2] * 0.017453292519943295;
  double sx = sin(x), cx = cos(x), sy = sin(y), cy = cos(y), sz = sin(z),
         cz = cos(z);
  double o[3][3] = {{cz * cy, cz * sy * sx - sz * cx, cz * sy * cx + sz * sx},
                    {sz * cy, sz * sy * sx + cz * cx, sz * sy * cx - cz * sx},
                    {-sy, cy * sx, cy * cx}},
         m[3][3], r[3][3];
  for (int i = 0; i < 3; ++i)
    for (int j = 0; j < 3; ++j)
      m[i][j] = f->right[i] * mr[j] + f->up[i] * mu[j] + f->forward[i] * mf[j];
  for (int i = 0; i < 3; ++i)
    for (int j = 0; j < 3; ++j) {
      r[i][j] = 0;
      for (int k = 0; k < 3; ++k)
        r[i][j] += m[i][k] * o[k][j];
    }
  double ry = asin(fmax(-1, fmin(1, -r[2][0]))), rx, rz;
  if (fabs(cos(ry)) > 1e-8) {
    rx = atan2(r[2][1], r[2][2]);
    rz = atan2(r[1][0], r[0][0]);
  } else {
    rx = 0;
    rz = atan2(-r[0][1], r[1][1]);
  }
  euler[0] = rx;
  euler[1] = ry;
  euler[2] = rz;
}
