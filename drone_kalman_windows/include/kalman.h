/* kalman.h - pure maths, NO operating-system calls. Identical on Linux and Windows.
 *
 * Model: constant-velocity drone in 2D.
 *   state  x = [px, py, vx, vy]
 *   F = [[1,0,dt,0],[0,1,0,dt],[0,0,1,0],[0,0,0,1]]
 *   H = [[1,0,0,0],[0,1,0,0]]                 (GPS measures position only)
 *   Q = q * white-noise-acceleration matrix   (dt^4/4, dt^3/2, dt^2 blocks)
 *   R = r * I2
 * Initial state: first measurement, zero velocity; P = diag(r, r, 100, 100).
 */
#ifndef KALMAN_H
#define KALMAN_H
#include <stdint.h>

typedef struct {
    double dt;
    double q;
    double r;
} KfParams;

/* meas: n_steps * 2 doubles (x,y).  out: n_steps * 4 doubles (x,y,vx,vy). */
void kf_run_track(const KfParams *p, const double *meas, uint32_t n_steps, double *out);

#endif
