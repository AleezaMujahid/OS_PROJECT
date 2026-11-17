/* drn_format.h - binary file format shared by Linux and Windows code.
 * Little-endian, IEEE-754 double. SAME on both operating systems.
 *
 *   sensor file ("DRN1") and ground-truth file ("DRN1"):
 *       DrnHeader, then double[n_tracks][n_steps][2]      (x, y in metres)
 *   output file ("KFO1"):
 *       DrnHeader, then double[n_tracks][n_steps][4]      (x, y, vx, vy)
 */
#ifndef DRN_FORMAT_H
#define DRN_FORMAT_H
#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef struct {
    char     magic[4];   /* "DRN1" or "KFO1"                        */
    uint32_t n_tracks;   /* number of drones                        */
    uint32_t n_steps;    /* measurements per drone                  */
    uint32_t reserved;   /* 0                                       */
    double   dt;         /* seconds between measurements            */
    double   q;          /* process noise (acceleration variance)   */
    double   r;          /* measurement noise variance (m^2)        */
} DrnHeader;             /* 40 bytes */

typedef char drn_header_size_check[(sizeof(DrnHeader) == 40) ? 1 : -1];

static inline size_t drn_in_bytes(const DrnHeader *h) {
    return sizeof(DrnHeader) + (size_t)h->n_tracks * h->n_steps * 2 * sizeof(double);
}
static inline size_t drn_out_bytes(const DrnHeader *h) {
    return sizeof(DrnHeader) + (size_t)h->n_tracks * h->n_steps * 4 * sizeof(double);
}
static inline const double *drn_in_track(const void *base, const DrnHeader *h, uint32_t t) {
    return (const double *)((const char *)base + sizeof(DrnHeader)) + (size_t)t * h->n_steps * 2;
}
static inline double *drn_out_track(void *base, const DrnHeader *h, uint32_t t) {
    return (double *)((char *)base + sizeof(DrnHeader)) + (size_t)t * h->n_steps * 4;
}

#endif
