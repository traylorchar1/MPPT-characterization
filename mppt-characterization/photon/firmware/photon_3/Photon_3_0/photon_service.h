#ifndef PHOTON_SERVICE_H
#define PHOTON_SERVICE_H
#include "can.h"
#include <stdbool.h>
#define PHOTON_PROTOCOL 0x50330201UL
typedef struct {
#define TUNE(id, member, def, lo, hi) int32 member;
#include "photon_fields.inc"
#undef TUNE
} PhotonTuning;
extern PhotonTuning photon_tuning;
extern volatile bool photon_disabled;
/* Tracker internals exposed for diagnostics (tracker.c). */
extern int32 po_step[2], po_p_prev[2], po_v_avg[2], po_i_avg[2];
extern int16 po_clamp[2];
void PhotonReceive(const canMsg *msg);
void PhotonPoll(void);
void PhotonStream(void);
#endif
