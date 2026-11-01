#!/usr/bin/env python 

import cobs
import gflags
import google.protobuf.text_format as text_format
import io
import os
import serial
import signal
import sys

sys.path.append('../../../nanopb/generator/proto')

import log_pb2

FLAGS = gflags.FLAGS


class StateReader(object):
    def __init__(self, device):
        self.ser = serial.Serial(port=device, baudrate=921600, timeout=0.1)
        self.sio = io.BufferedRWPair(self.ser, self.ser, 255)
        self.sequence = 0
        self.log = log_pb2.StateLog()
        signal.signal(signal.SIGINT, self.Exit)

    def Exit(self, signal, frame):
        print 'Exiting!'
        f = open('/tmp/log.bin', 'wb')
        f.write(self.log.SerializeToString())
        f.close()
        sys.exit(0)
        

    def Read(self):
        rx = ''
        while True:
            try:
                c = self.sio.read(1)
                if c=='\0':
                    if len(rx) > 1:
                        break
                    else:
                        rx = ''
                else:
                    rx += c
            except serial.SerialTimeoutException:
                rx = ''
        self.Parse(rx)

    def Parse(self, msg):
        log_msg = log_pb2.LogMessage()
        state = log_msg.state
        try:
            cobs_msg = cobs.decode(msg)
        except cobs.DecodeError:
            print 'Decode error!'
            print msg
            return

        try:
            state.ParseFromString(cobs_msg)
        except:
            pass
        if len(state.chan) != 2:
            return
        if state.sequence != self.sequence + 1 and state.sequence > 0:
            print 'sequence error!'
            print text_format.MessageToString(state)
        self.sequence = state.sequence


        filt_len = 8;
        log_msg.batt_v = state.batt_v_raw / (6.10 * filt_len)
        log_msg.batt_i = state.batt_i_raw / (64.0 * filt_len)
        log_msg.batt_p = log_msg.batt_v * log_msg.batt_i
        log_msg.chan.add()
        log_msg.chan.add()

        if len(state.chan) == 2:
            for c in range(2):
                log_msg.chan[c].v = state.chan[c].v_raw / (6.10 * filt_len)
                log_msg.chan[c].i = state.chan[c].i_raw / (128.0 * filt_len)
                log_msg.chan[c].p = log_msg.chan[c].v * log_msg.chan[c].i
                if state.chan[c].period:
                    log_msg.chan[c].freq = 40.0e6 / state.chan[c].period
                else:
                    log_msg.chan[c].freq = 0.0
        
            try:
                log_msg.eff = (log_msg.batt_p /
                               (log_msg.chan[0].p + log_msg.chan[1].p))
            except ZeroDivisionError:
                log_msg.eff = float('nan')
            else:
                pass
        else:
            print text_format.MessageToString(log_msg)
            print len(msg)
            assert False
 

        if state.sequence % 1 == 0:
            #print 'orig: ' + ''.join('%02X '%ord(c) for c in msg)
            print text_format.MessageToString(log_msg)
        try:
            log_msg.SerializeToString()
            self.log.state_data.extend([log_msg])
        except Exception as e:
            print e


def main(argv):
    # Parse commandline flags.
    try:
        argv = FLAGS(argv)
    except gflags.FlagsError, e:
        print '%s\\nUsage: %s ARGS\\n%s' % (e, sys.argv[0], FLAGS)
        sys.exit(1)

    state_reader = StateReader('/dev/tty_hs_dbg')


    while True:
        state_reader.Read()


if __name__ == '__main__':
        main(sys.argv)


