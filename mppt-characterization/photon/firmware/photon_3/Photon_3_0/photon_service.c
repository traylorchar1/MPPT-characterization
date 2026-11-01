/* Application-only CAN diagnostics and tuning. No flash, FRAM or bootloader
 * commands.
 *
 * Requests on base+0xD0, replies on base+0xE0, all 8 bytes:
 *   [0] op  [1] sequence  [2..3] field id (LE)  [4..7] value (LE int32/float32)
 * Ops: 0 identify, 1 read, 2 write, 3 stop, 4 start, 5 retrack.
 * Replies echo the request with op | 0x80; errors reply op 0xFF with the error
 * code in the value: 1 unknown field/op, 2 state does not allow it,
 * 3 read-only or out of range, 4 active fault.
 * TUNE registers can be written while tracking. PARAM writes require stop.
 */
#include "photon_service.h"
#include "main.h"
#include "pwm.h"
#include "queue.h"
#include "tracker.h"
#include "params.pb.h"
#include "state.pb.h"
#include <string.h>

extern volatile StateMessage state;
extern volatile ParamsMessage params;
volatile bool photon_disabled = false;

PhotonTuning photon_tuning = {
#define TUNE(id, member, def, lo, hi) def,
#include "photon_fields.inc"
#undef TUNE
};

typedef struct { int32 lo, hi; } Range;
static const Range tune_range[] = {
#define TUNE(id, member, def, lo, hi) {lo, hi},
#include "photon_fields.inc"
#undef TUNE
};
#define TUNE_FIRST 1100
#define TUNE_COUNT (sizeof(tune_range) / sizeof(tune_range[0]))

/* Field types: storage size and encoding. */
enum { T_I32, T_F32, T_I16, T_U8 };
/* Flags: writable while stopped (PARAM) */
#define F_RO 0
#define F_RW 1

typedef struct {
  uint16 id;
  uint8 type;
  uint8 flags;
  volatile void *ptr;
  float lo, hi;
} Field;

#define CH(c, base) \
  {base + 1,  T_I32, F_RO, &state.chan[c].v_raw, 0, 0}, \
  {base + 3,  T_I32, F_RO, &state.chan[c].v_goal_raw, 0, 0}, \
  {base + 6,  T_I32, F_RO, &state.chan[c].i_term, 0, 0}, \
  {base + 7,  T_I32, F_RO, &state.chan[c].i_raw, 0, 0}, \
  {base + 10, T_I32, F_RO, &state.chan[c].temp, 0, 0}, \
  {base + 11, T_I32, F_RO, &state.chan[c].dac, 0, 0}, \
  {base + 12, T_I16, F_RO, &state.chan[c].dir, 0, 0}, \
  {base + 22, T_I32, F_RO, &state.chan[c].p_raw_sweep_mpp, 0, 0}, \
  {base + 23, T_I32, F_RO, &state.chan[c].v_raw_sweep_mpp, 0, 0}, \
  {base + 25, T_U8,  F_RO, &state.chan[c].sweep_done, 0, 0}, \
  {base + 27, T_I32, F_RO, &state.chan[c].voc_raw, 0, 0}, \
  {base + 28, T_I32, F_RO, &state.chan[c].vsc_raw, 0, 0}, \
  {base + 40, T_I32, F_RO, &po_step[c], 0, 0}, \
  {base + 41, T_I32, F_RO, &po_p_prev[c], 0, 0}, \
  {base + 42, T_I32, F_RO, &po_v_avg[c], 0, 0}, \
  {base + 43, T_I32, F_RO, &po_i_avg[c], 0, 0}, \
  {base + 44, T_I16, F_RO, &po_clamp[c], 0, 0}

static const Field fields[] = {
  {2,  T_I32, F_RO, &state.batt_v_raw, 0, 0},
  {3,  T_I32, F_RO, &state.batt_i_raw, 0, 0},
  {4,  T_I32, F_RO, &state.temp_ambient, 0, 0},
  {5,  T_I16, F_RO, &state.safety, 0, 0},
  {6,  T_I16, F_RO, &state.control, 0, 0},
  {9,  T_I32, F_RO, &state.loops, 0, 0},
  {17, T_I16, F_RO, &state.alg, 0, 0},
  {18, T_I32, F_RO, &state.max_dac, 0, 0},
  {21, T_U8,  F_RO, &state.fpga_fault, 0, 0},
  CH(0, 100),
  CH(1, 200),
  {1000, T_I32, F_RO, &params.device.serial_number, 0, 0},
  {1001, T_I32, F_RO, &params.device.hw_rev, 0, 0},
  {1002, T_F32, F_RW, &params.device.v_scale, 0.000001, 1},
  {1003, T_F32, F_RW, &params.device.i_scale, 0.000001, 1},
  {1004, T_F32, F_RW, &params.device.v_cv_charge, 1, 150},
  {1005, T_F32, F_RW, &params.device.v_cv_charge_hyst, 0.01, 20},
  {1010, T_F32, F_RW, &params.chan[0].v_scale, 0.000001, 1},
  {1011, T_F32, F_RW, &params.chan[0].i_scale, 0.000001, 1},
  {1012, T_F32, F_RW, &params.chan[1].v_scale, 0.000001, 1},
  {1013, T_F32, F_RW, &params.chan[1].i_scale, 0.000001, 1},
  {1020, T_I32, F_RO, &params.comm.can_bitrate, 0, 0},
  {1021, T_I32, F_RO, &params.comm.can_base_addr, 0, 0},
  {1031, T_I32, F_RW, &params.track.min_retrack_time, 1, 90000},
};
#define N_FIELDS (sizeof(fields) / sizeof(fields[0]))

typedef union { uint32 u; int32 i; float f; } Value;
static volatile bool pending;
static uint8 request[8];

/* Called in the CAN ISR: the real work runs from the main queue. */
void PhotonReceive(const canMsg *msg) {
  if (pending || msg->frame_type != CAN_FRAME_STD ||
      msg->message_type != CAN_MSG_DATA || msg->data_length != 8) return;
  memcpy(request, msg->data, 8);
  pending = true;
  if (!QueuePush(PhotonPoll)) pending = false;
}

static const Field *Find(uint16 id) {
  uint16 k;
  for (k = 0; k < N_FIELDS; k++) if (fields[k].id == id) return &fields[k];
  return 0;
}

static int ReadField(uint16 id, Value *v) {
  const Field *f;
  if (id >= TUNE_FIRST && id < TUNE_FIRST + TUNE_COUNT) {
    v->i = ((int32 *)&photon_tuning)[id - TUNE_FIRST];
    return 1;
  }
  f = Find(id);
  if (!f) return 0;
  disableInterrupts();
  switch (f->type) {
    case T_I32: v->i = *(volatile int32 *)f->ptr; break;
    case T_F32: v->f = *(volatile float *)f->ptr; break;
    case T_I16: v->i = *(volatile int16 *)f->ptr; break;
    default:    v->i = *(volatile uint8 *)f->ptr; break;
  }
  restoreInterrupts();
  return 1;
}

/* Returns 0 on success or an error code. Range checks reject NaN. */
static uint32 WriteField(uint16 id, Value v) {
  const Field *f;
  if (id >= TUNE_FIRST && id < TUNE_FIRST + TUNE_COUNT) {
    const Range *r = &tune_range[id - TUNE_FIRST];
    if (v.i < r->lo || v.i > r->hi) return 3;
    if (id == 1109 && v.i >= photon_tuning.max_voltage_raw) return 3;
    if (id == 1110 && v.i <= photon_tuning.min_voltage_raw) return 3;
    ((int32 *)&photon_tuning)[id - TUNE_FIRST] = v.i;
    if (id == 1122) TrackerApplyMaxDac();
    return 0;
  }
  f = Find(id);
  if (!f) return 1;
  if (f->flags != F_RW) return 3;
  if (!photon_disabled || state.control != ControlEnum_CAN_DISABLE) return 2;
  if (f->type == T_F32) {
    if (!(v.f >= f->lo && v.f <= f->hi)) return 3;
    if (id == 1004 && !(v.f > params.device.v_cv_charge_hyst)) return 3;
    if (id == 1005 && !(v.f < params.device.v_cv_charge)) return 3;
    *(volatile float *)f->ptr = v.f;
  } else {
    if (!(v.i >= f->lo && v.i <= f->hi)) return 3;
    *(volatile int32 *)f->ptr = v.i;
  }
  /* Recompute calibrated thresholds while outputs are held disabled. */
  InitTracker();
  state.control = ControlEnum_CAN_DISABLE;
  return 0;
}

void PhotonPoll(void) {
  uint8 reply[8], op;
  uint16 id;
  uint32 error = 0;
  Value v;
  if (!pending) return;
  memcpy(reply, request, 8);
  op = reply[0];
  id = (uint16)reply[2] | ((uint16)reply[3] << 8);
  v.u = (uint32)reply[4] | ((uint32)reply[5] << 8) |
        ((uint32)reply[6] << 16) | ((uint32)reply[7] << 24);
  switch (op) {
    case 0: v.u = PHOTON_PROTOCOL; break;
    case 1: if (!ReadField(id, &v)) error = 1; break;
    case 2: error = WriteField(id, v); break;
    case 3: /* Explicit stop; safety recovery cannot silently re-enable it. */
      photon_disabled = true;
      disablePWM();
      SetPrechargeRelayState(OFF);
      SetMainRelayState(OFF);
      state.control = ControlEnum_CAN_DISABLE;
      v.u = 0;
      break;
    case 4:
      if (state.safety != SafetyEnum_NO_ERROR || state.fpga_fault) error = 4;
      else { photon_disabled = false; state.control = ControlEnum_INIT; v.u = 0; }
      break;
    case 5:
      if (photon_disabled || state.control != ControlEnum_TRACKING) error = 2;
      else { state.alg = AlgorithmEnum_MEASURE_VOC; state.loops = 0; v.u = 0; }
      break;
    default: error = 1; break;
  }
  reply[0] = error ? 0xff : op | 0x80;
  if (error) v.u = error;
  reply[4] = v.u; reply[5] = v.u >> 8;
  reply[6] = v.u >> 16; reply[7] = v.u >> 24;
  disableInterrupts();
  sendCANMsg(params.comm.can_base_addr + 0xe0, reply, 8);
  pending = false;
  restoreInterrupts();
}

/* Tracker debug frame per channel on base+0xE1 / base+0xE2:
 * [0..1] v_goal_raw  [2..3] dac  [4..5] P&O period mean v_raw
 * [6] alg | dir << 2 | clamp << 3 | control << 4   [7] step (saturated) */
void PhotonStream(void) {
  uint8 d[8], c;
  int32 s;
  if (!photon_tuning.stream) return;
  for (c = 0; c < 2; c++) {
    d[0] = state.chan[c].v_goal_raw; d[1] = state.chan[c].v_goal_raw >> 8;
    d[2] = state.chan[c].dac;        d[3] = state.chan[c].dac >> 8;
    d[4] = po_v_avg[c];              d[5] = po_v_avg[c] >> 8;
    d[6] = (state.alg & 3) | ((state.chan[c].dir & 1) << 2) |
           ((po_clamp[c] & 1) << 3) | ((state.control & 0xf) << 4);
    s = po_step[c];
    d[7] = s > 255 ? 255 : s;
    sendCANMsg(params.comm.can_base_addr + 0xe1 + c, d, 8);
  }
}
