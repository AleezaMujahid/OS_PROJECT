/* gen_test_data.c - STAND-IN drone data generator, so the Windows code can be tested
 * before Person C's generator is ready. It writes the SAME file format (drn_format.h).
 * Plain C + stdio on purpose (it is a tool, not part of the system-call work), and its
 * own random generator, so the same seed gives the same data on every OS.
 *
 *   gen_test_data <n_tracks> <n_steps> <seed> <sensor.bin> <truth.bin> [sigma_m=3.0] [q=10]
 *
 * Each drone follows one of three paths (straight line, circle, sine weave) at 5-15 m/s;
 * the GPS sensor adds Gaussian noise with standard deviation sigma_m (metres). */
#include "drn_format.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define PI 3.14159265358979323846

static uint64_t s;
static uint64_t rng(void) { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return s; } /* xorshift64 */
static double u01(void) { return (double)(rng() >> 11) * (1.0 / 9007199254740992.0); }
static double gauss(void) {
    double u1 = u01(), u2 = u01();
    if (u1 < 1e-300) u1 = 1e-300;
    return sqrt(-2.0 * log(u1)) * cos(2.0 * PI * u2);
}

int main(int argc, char **argv) {
    if (argc < 6) {
        fprintf(stderr, "usage: gen_test_data <n_tracks> <n_steps> <seed> <sensor.bin> <truth.bin> [sigma_m=3.0]\n");
        return 1;
    }
    uint32_t nt = (uint32_t)strtoul(argv[1], NULL, 10), ns = (uint32_t)strtoul(argv[2], NULL, 10);
    s = strtoull(argv[3], NULL, 10) * 2654435761u + 88172645463325252ull;
    double sigma = argc > 6 ? atof(argv[6]) : 3.0;
    /* process noise handed to the filter via the header. 10 suits these manoeuvring paths:
     * with q=0.5 the filter lags behind turns and removes only ~7% of the sensor error. */
    double q = argc > 7 ? atof(argv[7]) : 10.0;
    if (nt == 0 || ns == 0) { fprintf(stderr, "n_tracks and n_steps must be > 0\n"); return 1; }

    DrnHeader h = { {'D', 'R', 'N', '1'}, nt, ns, 0, 0.1, q, sigma * sigma };
    FILE *fs = fopen(argv[4], "wb"), *ft = fopen(argv[5], "wb");
    if (!fs || !ft) { perror("fopen"); return 1; }
    fwrite(&h, sizeof h, 1, fs);
    fwrite(&h, sizeof h, 1, ft);

    double *zs = malloc(sizeof(double) * 2 * ns), *zt = malloc(sizeof(double) * 2 * ns);
    if (!zs || !zt) { fprintf(stderr, "out of memory\n"); return 1; }

    for (uint32_t t = 0; t < nt; t++) {
        double x0 = 1000.0 * u01(), y0 = 1000.0 * u01();
        double speed = 5.0 + 10.0 * u01(), heading = 2.0 * PI * u01();
        double R = 50.0 + 150.0 * u01(), amp = 20.0 + 40.0 * u01(), per = 20.0 + 40.0 * u01();
        for (uint32_t k = 0; k < ns; k++) {
            double tm = k * h.dt, x, y;
            if (t % 3 == 0) {                       /* straight line */
                x = x0 + speed * cos(heading) * tm;
                y = y0 + speed * sin(heading) * tm;
            } else if (t % 3 == 1) {                /* circle */
                double w = speed / R;
                x = x0 + R * cos(heading + w * tm);
                y = y0 + R * sin(heading + w * tm);
            } else {                                /* sine weave */
                x = x0 + speed * cos(heading) * tm - amp * sin(2.0 * PI * tm / per) * sin(heading);
                y = y0 + speed * sin(heading) * tm + amp * sin(2.0 * PI * tm / per) * cos(heading);
            }
            zt[2 * k] = x;
            zt[2 * k + 1] = y;
            zs[2 * k] = x + sigma * gauss();
            zs[2 * k + 1] = y + sigma * gauss();
        }
        fwrite(zs, sizeof(double), 2 * (size_t)ns, fs);
        fwrite(zt, sizeof(double), 2 * (size_t)ns, ft);
    }
    fclose(fs);
    fclose(ft);
    printf("wrote %s and %s: %u drones x %u steps, dt=%.2f s, sensor sigma=%.2f m\n", argv[4], argv[5], nt, ns, h.dt, sigma);
    free(zs);
    free(zt);
    return 0;
}
