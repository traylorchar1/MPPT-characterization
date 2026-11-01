/* Tracker tuning registers: TUNE(id, member, default, min, max).
 * IDs are protocol ABI. All values are int32. Time units are 20 ms tracker
 * ticks; voltages are channel ADC counts (multiply by chan v_scale for volts).
 * Every TUNE register can be written while tracking. */
TUNE(1100, algorithm, 1, 1, 2)              /* 1 = power P&O, 2 = incremental conductance */
TUNE(1101, algorithm_ticks, 4, 1, 1000)     /* sweep / IC cadence */
TUNE(1102, retrack_ticks, 15000, 50, 180000)/* periodic Voc + sweep (5 min) */
TUNE(1103, sweep_timeout_ticks, 250, 1, 3000)
TUNE(1104, voc_ticks, 1, 1, 1000)
TUNE(1105, sweep_dac_step, 64, 1, 256)
TUNE(1106, inc_step, 24, 1, 256)
TUNE(1107, p_divisor, 4, 1, 1024)           /* voltage PI: P = err / p_divisor */
TUNE(1108, i_divisor, 16, 1, 4096)          /* voltage PI: I += err / i_divisor per 5 ms, remainder kept */
TUNE(1109, min_voltage_raw, 96, 96, 5911)
TUNE(1110, max_voltage_raw, 5912, 97, 5912)
TUNE(1111, max_pid_error, 248, 1, 1024)
TUNE(1112, sweep_floor_percent, 50, 10, 95)
TUNE(1113, power_drop_percent, 50, 1, 99)
TUNE(1114, po_period_ticks, 5, 2, 100)      /* P&O: one perturbation per period */
TUNE(1115, po_settle_ticks, 2, 0, 99)       /* P&O: ticks ignored after each step before averaging */
TUNE(1116, po_step_min, 12, 1, 500)         /* P&O: step after a reversal (~0.25 V) */
TUNE(1117, po_step_max, 100, 1, 1000)       /* P&O: largest step after repeated same-direction gains (~2 V) */
TUNE(1118, po_up_clamp, 24, 1, 1000)        /* P&O: goal may not exceed measured V by more than this */
TUNE(1119, po_down_clamp, 24, 1, 2000)      /* P&O: goal may not fall below measured V by more than this (~0.5 V); larger values overshoot the source current limit */
TUNE(1120, hold_v_raw, 0, 0, 5912)          /* 0 = track; otherwise hold this input voltage */
TUNE(1121, stream, 0, 0, 1)                 /* 1 = send tracker debug frames on base+0xE1/0xE2 at 50 Hz */
TUNE(1122, max_dac, 2023, 64, 2400)         /* input current ceiling, DAC counts (2023 ~ 7.5 A, ~3.7 mA/count); rev 3.2+ */
TUNE(1123, oc_arr_raw, 8192, 1024, 9216)    /* array overcurrent trip, Photon-measured counts (8192 = 8.0 A, 9216 = 9.0 A) */
TUNE(1124, ceiling_creep, 2, 0, 100)         /* learned current ceiling after a source collapse rises this many DAC counts per P&O period */
