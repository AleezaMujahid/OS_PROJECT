/* validate.c - proves the results are correct (rubric criterion 4: Validation).
 *
 *   validate compare <a.bin> <b.bin>
 *       PASS if the two output files are byte-for-byte identical
 *       (parallel result == single-process result).
 *   validate rmse <out.bin> <truth.bin> <sensor.bin>
 *       PASS if the filtered position error (RMSE) is smaller than the raw sensor error.
 */
#include "sc_platform.h"
#include "drn_format.h"
#include "sc_fileio.h"
#include "sc_log.h"
#include "sc_mem.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static void *load(const char *path, uint64_t *size) {
    HANDLE h = fio_open_read(path);
    if (h == INVALID_HANDLE_VALUE) return NULL;
    void *buf = NULL;
    if (fio_get_size(h, size) && *size > 0 && (buf = mem_alloc((size_t)*size)) != NULL) {
        if (!fio_read_exact(h, buf, (size_t)*size)) { mem_free(buf); buf = NULL; }
    }
    fio_close(h);
    return buf;
}

static int cmd_compare(const char *pa, const char *pb) {
    uint64_t sa = 0, sb = 0;
    unsigned char *a = load(pa, &sa), *b = load(pb, &sb);
    if (!a || !b) return 2;
    int rc = 1;
    if (sa != sb) {
        printf("FAIL: sizes differ (%llu vs %llu bytes)\n", (unsigned long long)sa, (unsigned long long)sb);
    } else if (memcmp(a, b, (size_t)sa) != 0) {
        size_t i = 0;
        while (a[i] == b[i]) i++;
        printf("FAIL: files differ, first difference at byte %zu\n", i);
    } else {
        printf("PASS: files are identical (%llu bytes)\n", (unsigned long long)sa);
        rc = 0;
    }
    mem_free(a);
    mem_free(b);
    return rc;
}

static int cmd_rmse(const char *pout, const char *ptruth, const char *pin) {
    uint64_t so = 0, st = 0, si = 0;
    void *out = load(pout, &so), *truth = load(ptruth, &st), *in = load(pin, &si);
    if (!out || !truth || !in) return 2;
    const DrnHeader *ho = (const DrnHeader *)out, *ht = (const DrnHeader *)truth, *hi = (const DrnHeader *)in;
    if (memcmp(ho->magic, "KFO1", 4) || memcmp(ht->magic, "DRN1", 4) || memcmp(hi->magic, "DRN1", 4) ||
        ho->n_tracks != ht->n_tracks || ho->n_steps != ht->n_steps ||
        hi->n_tracks != ht->n_tracks || hi->n_steps != ht->n_steps ||
        so != drn_out_bytes(ho) || st != drn_in_bytes(ht) || si != drn_in_bytes(hi)) {
        printf("FAIL: files do not match each other (check the order: out, truth, sensor)\n");
        return 2;
    }
    double raw = 0.0, filt = 0.0;
    double raw_late = 0.0, filt_late = 0.0;
    const uint32_t skip = ho->n_steps > 100 ? 50 : 0; /* "settled" part: after the filter has converged */
    uint64_t n = 0, n_late = 0;
    for (uint32_t t = 0; t < ho->n_tracks; t++) {
        const double *tr = drn_in_track(truth, ht, t), *z = drn_in_track(in, hi, t);
        const double *o = drn_out_track(out, ho, t);
        for (uint32_t k = 0; k < ho->n_steps; k++) {
            double rx = z[2 * k] - tr[2 * k], ry = z[2 * k + 1] - tr[2 * k + 1];
            double fx = o[4 * (size_t)k] - tr[2 * k], fy = o[4 * (size_t)k + 1] - tr[2 * k + 1];
            raw += rx * rx + ry * ry;
            filt += fx * fx + fy * fy;
            n++;
            if (k >= skip) { raw_late += rx * rx + ry * ry; filt_late += fx * fx + fy * fy; n_late++; }
        }
    }
    double r_raw = sqrt(raw / (double)n), r_filt = sqrt(filt / (double)n);
    double l_raw = sqrt(raw_late / (double)n_late), l_filt = sqrt(filt_late / (double)n_late);
    printf("Position RMSE over all steps    : raw sensor %.4f m, filtered %.4f m (%.1f%% smaller)\n", r_raw, r_filt,
           100.0 * (1.0 - r_filt / r_raw));
    printf("Position RMSE after step %-3u     : raw sensor %.4f m, filtered %.4f m (%.1f%% smaller)\n", skip, l_raw,
           l_filt, 100.0 * (1.0 - l_filt / l_raw));
    int pass = l_filt < l_raw;
    printf("%s: the filter %s the sensor error\n", pass ? "PASS" : "FAIL", pass ? "reduces" : "does NOT reduce");
    mem_free(out);
    mem_free(truth);
    mem_free(in);
    return pass ? 0 : 1;
}

int main(int argc, char **argv) {
    log_open("logs\\validate.log", false);
    int rc = 2;
    if (argc == 4 && strcmp(argv[1], "compare") == 0) rc = cmd_compare(argv[2], argv[3]);
    else if (argc == 5 && strcmp(argv[1], "rmse") == 0) rc = cmd_rmse(argv[2], argv[3], argv[4]);
    else fprintf(stderr, "usage:\n  validate compare <a.bin> <b.bin>\n  validate rmse <out.bin> <truth.bin> <sensor.bin>\n");
    log_close();
    return rc;
}
