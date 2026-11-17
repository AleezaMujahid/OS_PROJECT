/* kalman.c - pure maths, no operating-system calls (see kalman.h). */
#include "kalman.h"
#include <string.h>

typedef double M4[4][4];

static void mul(M4 a, M4 b, M4 c) { /* c = a * b */
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++) {
            double s = 0.0;
            for (int k = 0; k < 4; k++) s += a[i][k] * b[k][j];
            c[i][j] = s;
        }
}

static void mul_bt(M4 a, M4 b, M4 c) { /* c = a * b^T */
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++) {
            double s = 0.0;
            for (int k = 0; k < 4; k++) s += a[i][k] * b[j][k];
            c[i][j] = s;
        }
}

void kf_run_track(const KfParams *p, const double *z, uint32_t n, double *out) {
    if (n == 0) return;
    const double dt = p->dt, q = p->q, r = p->r;
    const double dt2 = dt * dt, dt3 = dt2 * dt, dt4 = dt3 * dt;

    M4 F = {{1, 0, dt, 0}, {0, 1, 0, dt}, {0, 0, 1, 0}, {0, 0, 0, 1}};
    M4 Q = {{q * dt4 / 4, 0, q * dt3 / 2, 0},
            {0, q * dt4 / 4, 0, q * dt3 / 2},
            {q * dt3 / 2, 0, q * dt2, 0},
            {0, q * dt3 / 2, 0, q * dt2}};
    M4 P;
    memset(P, 0, sizeof P);
    P[0][0] = P[1][1] = r;
    P[2][2] = P[3][3] = 100.0;

    double x[4] = {z[0], z[1], 0.0, 0.0};
    memcpy(out, x, sizeof x);

    for (uint32_t k = 1; k < n; k++) {
        /* ---- predict: x = F x,  P = F P F^T + Q ---- */
        double xp[4];
        for (int i = 0; i < 4; i++) {
            double s = 0.0;
            for (int j = 0; j < 4; j++) s += F[i][j] * x[j];
            xp[i] = s;
        }
        M4 FP, Pp;
        mul(F, P, FP);
        mul_bt(FP, F, Pp);
        for (int i = 0; i < 4; i++)
            for (int j = 0; j < 4; j++) Pp[i][j] += Q[i][j];

        /* ---- update ---- */
        /* S = H Pp H^T + R  (2x2) and its inverse */
        double s00 = Pp[0][0] + r, s01 = Pp[0][1], s10 = Pp[1][0], s11 = Pp[1][1] + r;
        double det = s00 * s11 - s01 * s10;
        double i00 = s11 / det, i01 = -s01 / det, i10 = -s10 / det, i11 = s00 / det;

        /* K = Pp H^T S^-1  (4x2) */
        double K[4][2];
        for (int i = 0; i < 4; i++) {
            K[i][0] = Pp[i][0] * i00 + Pp[i][1] * i10;
            K[i][1] = Pp[i][0] * i01 + Pp[i][1] * i11;
        }

        double y0 = z[2 * k] - xp[0], y1 = z[2 * k + 1] - xp[1];
        for (int i = 0; i < 4; i++) x[i] = xp[i] + K[i][0] * y0 + K[i][1] * y1;

        /* Joseph form: P = (I-KH) Pp (I-KH)^T + K R K^T  (keeps P symmetric) */
        M4 A, AP, Pn;
        for (int i = 0; i < 4; i++)
            for (int j = 0; j < 4; j++) A[i][j] = (i == j ? 1.0 : 0.0) - (j < 2 ? K[i][j] : 0.0);
        mul(A, Pp, AP);
        mul_bt(AP, A, Pn);
        for (int i = 0; i < 4; i++)
            for (int j = 0; j < 4; j++) P[i][j] = Pn[i][j] + r * (K[i][0] * K[j][0] + K[i][1] * K[j][1]);

        memcpy(out + 4 * (size_t)k, x, sizeof x);
    }
}
