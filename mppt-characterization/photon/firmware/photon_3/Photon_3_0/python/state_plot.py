#!/usr/bin/env python

import gflags
import matplotlib.pyplot as plt
import numpy as np
import sys

sys.path.append('../../../nanopb/generator/proto')

import log_pb2

FLAGS = gflags.FLAGS

gflags.DEFINE_string('logfile', '/tmp/log.bin', 'Name of log file to parse')

class trivial_class(object):
    pass

class StatePlot(object):

    def __init__(self, fname):
        print fname
        self.log = log_pb2.StateLog()
        f = open(fname, 'rb')
        self.log.ParseFromString(f.read())
        f.close
        
        self.ms = 2 # markersize
        self.point = '.'
        self.line = '-'
        
        self.reddot = 'r' + self.point
        self.greendot = 'g' + self.point
        self.bluedot = 'b' + self.point
        self.magentadot = 'm' + self.point
        self.blackdot = 'k' + self.point
        self.cyandot = 'c' + self.point
        
        self.redline = 'r' + self.point + self.line
        self.greenline = 'g' + self.point + self.line
        self.blueline = 'b' + self.point + self.line
        self.magentaline = 'm' + self.point + self.line
        self.blackline = 'k' + self.point + self.line
        self.cyanline = 'c' + self.point + self.line

        self.LoadNumpyArrays()

    def LoadVectorCmd(self, name):
        l  = list()
        for i, e in enumerate(self.log.state_data):
            try:
                exec('l.append(e.{0:s})'.format(name))
            except:
                l.append(l[i-1])
                print 'bad element at {0:d} in {1:s}'.format(i, name)
        exec('self.{0:s} = np.array(l, dtype=np.float64)'.format(name))

    def CheckSequenceGaps(self):
        for i, s in enumerate(self.state.sequence):
            if i > 0:
                if s != self.state.sequence[i-1] + 1:
                    print 'sequence gap at {0:0.3f}'.format(self.time[i])
            

    def LoadNumpyArrays(self):
        self.chan = list()
        self.chan.append(trivial_class())
        self.chan.append(trivial_class())
        self.state = trivial_class()
        self.state.chan = list()
        self.state.chan.append(trivial_class())
        self.state.chan.append(trivial_class())

        self.LoadVectorCmd('state.sequence')
        self.LoadVectorCmd('time')
        #self.time = np.divide(self.state.sequence, 50.0)
        self.CheckSequenceGaps()
        for ch in range(2):
            self.LoadVectorCmd('chan[{:d}].v'.format(ch))
            self.LoadVectorCmd('state.chan[{:d}].v_raw'.format(ch))
            self.LoadVectorCmd('state.chan[{:d}].i_raw'.format(ch))
            self.LoadVectorCmd('state.chan[{:d}].p_raw'.format(ch))
            self.LoadVectorCmd('state.chan[{:d}].p_last_raw'.format(ch))
            self.LoadVectorCmd('state.chan[{:d}].p_last_change_raw'.format(ch))
            self.LoadVectorCmd('state.chan[{:d}].v_goal_raw'.format(ch))
            self.LoadVectorCmd('chan[{:d}].i'.format(ch))
            self.LoadVectorCmd('chan[{:d}].p'.format(ch))
            self.LoadVectorCmd('chan[{:d}].freq'.format(ch))
            self.LoadVectorCmd('state.chan[{:d}].dac'.format(ch))
            self.LoadVectorCmd('state.chan[{:d}].dir'.format(ch))
            self.LoadVectorCmd('state.chan[{:d}].dbg'.format(ch))
            self.LoadVectorCmd('state.chan[{:d}].temp'.format(ch))
        self.LoadVectorCmd('state.batt_v')
        self.LoadVectorCmd('state.batt_i')
        self.LoadVectorCmd('batt_p')
        self.LoadVectorCmd('state.batt_p_raw')
        self.LoadVectorCmd('state.batt_p_last_raw')
        self.LoadVectorCmd('state.temp_ambient')
        
    def PlotVoltage(self):
        plt.figure()
        plt.hold(True)
        plt.plot(self.time, self.state.batt_v, self.blueline)
        plt.plot(self.time, self.chan[0].v, self.redline, markersize=self.ms)
        plt.plot(self.time, self.chan[1].v, self.greenline, markersize=self.ms)
        plt.legend(['Battery', 'Array Ch 0', 'Array Ch 1'])
        plt.xlabel('Time')
        plt.ylabel('Voltage')
        plt.title('Voltages vs Time')
        plt.grid(True)
        plt.hold(False)

    def PlotCurrent(self):
        plt.figure()
        plt.hold(True)
        plt.plot(self.time, self.state.batt_i, self.blueline, markersize=self.ms)
        plt.plot(self.time, self.chan[0].i, self.redline, markersize=self.ms)
        plt.plot(self.time, self.chan[1].i, self.greenline, markersize=self.ms)
        plt.legend(['Battery', 'Array Ch 0', 'Array Ch 1'])
        plt.xlabel('Time')
        plt.ylabel('Current')
        plt.title('Currents vs Time')
        plt.grid(True)
        plt.hold(False)

    def PlotPower(self):
        plt.figure()
        plt.hold(True)
        plt.plot(self.time, self.batt_p, self.blueline, markersize=self.ms)
        plt.plot(self.time, self.chan[0].p, self.redline, markersize=self.ms)
        plt.plot(self.time, self.chan[1].p, self.greenline, markersize=self.ms)
        plt.legend(['Battery', 'Array Ch 0', 'Array Ch 1'])
        plt.xlabel('Time')
        plt.ylabel('Power')
        plt.title('Power vs Time')
        plt.grid(True)
        plt.hold(False)

    def PlotRawBattPower(self):
        plt.figure()
        plt.hold(True)
        plt.plot(self.time, self.state.batt_p_raw, self.blueline, markersize=self.ms)
        plt.plot(self.time, self.state.batt_p_last_raw, self.redline, markersize=self.ms)
        plt.legend(['Battery', 'Battery Last'])
        plt.xlabel('Time')
        plt.ylabel('Power Raw')
        plt.title('Power vs Time')
        plt.grid(True)
        plt.hold(False)

    def PlotFreq(self):
        plt.figure()
        plt.hold(True)
        plt.plot(self.time, self.chan[0].freq, self.redline, markersize=self.ms)
        plt.plot(self.time, self.chan[1].freq, self.greenline, markersize=self.ms)
        plt.legend(['Array Ch 0', 'Array Ch 1'])
        plt.xlabel('Time')
        plt.ylabel('Frequency')
        plt.title('Frequency vs Time')
        plt.grid(True)
        plt.hold(False)

    def PlotState(self):
        self.LoadVectorCmd('state.control')
        self.LoadVectorCmd('state.safety')
        plt.figure()
        plt.hold(True)
        plt.plot(self.time, self.state.control, self.blueline, markersize=self.ms)
        plt.plot(self.time, self.state.safety, self.greenline, markersize=self.ms)
        plt.plot(self.time, self.chan[1].i, self.redline, markersize=self.ms)
        plt.legend(['State', 'Safety', 'Current'])
        plt.xlabel('Time')
        plt.ylabel('Power')
        plt.grid(True)
        plt.hold(False)

    def PlotTemperatures(self):
        tscale = 100
        plt.figure()
        plt.hold(True)
        plt.plot(self.time, self.state.temp_ambient/tscale, self.blueline, markersize=self.ms)
        plt.plot(self.time, self.state.chan[0].temp/tscale, self.redline, markersize=self.ms)
        plt.plot(self.time, self.state.chan[1].temp/tscale, self.greenline, markersize=self.ms)
        plt.legend(['Ambient', 'Array Ch 0', 'Array Ch 1'])
        plt.xlabel('Time')
        plt.ylabel('Temp (C)')
        plt.title('Temperature vs Time')
        plt.grid(True)
        plt.hold(False)

    def PlotTestDac(self, ch):
        plt.figure()
        plt.hold(True)
        plt.plot(self.time, self.state.chan[ch].v_raw/8.0, self.blueline, markersize=self.ms)
        plt.plot(self.time, self.state.chan[ch].i_raw/8.0, self.redline, markersize=self.ms)
        plt.plot(self.time, self.state.chan[ch].dbg, self.blackline, markersize=self.ms)
        plt.plot(self.time, self.state.chan[ch].dac, self.magentaline, markersize=self.ms)
        plt.legend(['AV %d' % ch, 'AI %d' % ch, 'ADBG %d' % ch, 'ADAC %d' % ch])
        plt.xlabel('Time')
        plt.title('Channel %d DAC Test' % ch)
        plt.grid(True)
        plt.hold(False)

    def PlotTestControlVoltage(self, ch):
        plt.figure()
        plt.hold(True)
        plt.plot(self.time, self.state.chan[ch].v_raw/8.0, self.blueline, markersize=self.ms)
        plt.plot(self.time, self.state.chan[ch].i_raw/8.0, self.redline, markersize=self.ms)
        plt.plot(self.time, self.state.chan[ch].v_goal_raw/8.0, self.blackline, markersize=self.ms)
        plt.plot(self.time, self.state.chan[ch].dac, self.magentaline, markersize=self.ms)
        plt.legend(['AV %d' % ch, 'AI %d' % ch, 'ADBG %d' % ch, 'ADAC %d' % ch])
        plt.xlabel('Time')
        plt.title('Channel %d Control Voltage Test' % ch)
        plt.grid(True)
        plt.hold(False)

    def PlotIV(self):
        plt.figure()
        plt.hold(True)
        plt.plot(self.chan[0].v, self.chan[0].i, self.reddot, markersize=self.ms)
        plt.plot(self.chan[1].v, self.chan[1].i, self.bluedot, markersize=self.ms)
        plt.legend(['Array Ch 0', 'Array Ch 1'])
        plt.xlabel('Voltage')
        plt.ylabel('Current')
        plt.title('IV Curve')
        plt.grid(True)
        plt.hold(False)

    def PlotPV(self):
        plt.figure()
        plt.hold(True)
        plt.plot(self.chan[0].v, self.chan[0].p, self.reddot, markersize=self.ms)
        plt.plot(self.chan[1].v, self.chan[1].p, self.bluedot, markersize=self.ms)
        plt.legend(['Array Ch 0', 'Array Ch 1'])
        plt.xlabel('Voltage')
        plt.ylabel('Power')
        plt.title('PV Curve')
        plt.grid(True)
        plt.hold(False)

    def PlotTracking(self, channel):
        exec('a_v = self.chan[{:d}].v'.format(channel))
        exec('a_i = self.chan[{:d}].i'.format(channel))
        exec('a_p = self.chan[{:d}].p'.format(channel))
        exec('a_v_raw = self.state.chan[{:d}].v_raw'.format(channel))
        exec('a_i_raw = self.state.chan[{:d}].i_raw'.format(channel))
        exec('a_p_raw = self.state.chan[{:d}].p_raw'.format(channel))
        exec('a_p_raw_last_change = self.state.chan[{:d}].p_last_change_raw'.format(channel))
        exec('a_p_raw_last = self.state.chan[{:d}].p_last_raw'.format(channel))
        exec('a_dac = self.state.chan[{:d}].dac'.format(channel))
        exec('a_dir = self.state.chan[{:d}].dir'.format(channel))
        exec('a_v_goal = self.state.chan[{:d}].v_goal_raw'.format(channel))
        b_v = self.state.batt_v
        b_i = self.state.batt_i
        b_p = self.batt_p

        plt.figure()
        plt.hold(True)
        plt.plot(self.time, a_v_raw, self.blueline, markersize=self.ms)
        plt.plot(self.time, a_v_goal, self.redline, markersize=self.ms)
        plt.plot(self.time, a_dac, self.blackline, markersize=self.ms)
        plt.legend(['Voltage', 'Goal', 'DAC'])
        plt.xlabel('Time')
        plt.ylabel('Counts')
        plt.title('Voltage Control Ch{0:d}'.format(channel))
        plt.grid(True)
        plt.hold(False)

        plt.figure()
        plt.hold(True)
        plt.plot(self.time, a_v_raw, self.blueline, markersize=self.ms)
        plt.plot(self.time, a_i_raw, self.redline, markersize=self.ms)
        plt.plot(self.time, a_p_raw/5000, self.blackline, markersize=self.ms)
        plt.plot(self.time, a_p_raw_last/5000, self.magentaline, markersize=self.ms)
        plt.plot(self.time, a_p_raw_last_change/5000, self.cyanline, markersize=self.ms)
        plt.legend(['Voltage', 'Current', 'Power', 'Power Last', 'Power Last Change'])
        plt.title('Raw Tracking Ch{0:d}'.format(channel))
        plt.xlabel('Time')
        plt.ylabel('Counts')
        plt.grid(True)
        plt.hold(False)

        plt.figure()
        plt.hold(True)
        plt.plot(self.time, a_v, self.blueline, markersize=self.ms)
        plt.plot(self.time, a_i*10, self.redline, markersize=self.ms)
        plt.plot(self.time, a_p, self.blackline, markersize=self.ms)
        plt.legend(['Voltage', 'Current * 10', 'Power'])
        plt.title('Tracking Ch{0:d}'.format(channel))
        plt.xlabel('Time')
        plt.grid(True)
        plt.hold(False)


def main(argv):
    # Parse commandline flags.
    try:
        argv = FLAGS(argv)
    except gflags.FlagsError, e:
        print '%s\\nUsage: %s ARGS\\n%s' % (e, sys.argv[0], FLAGS)
        sys.exit(1)

    p = StatePlot(FLAGS.logfile)
    #p.PlotVoltage()
    #p.PlotFreq()
    #p.PlotCurrent()
    #p.PlotTemperatures()
    #p.PlotState()
    #p.PlotPower()
    p.PlotIV()
    p.PlotPV()
    p.PlotTracking(0)
    p.PlotTracking(1)
    #p.PlotRawBattPower()
    p.PlotTestDac(0)
    p.PlotTestDac(1)
    p.PlotTestControlVoltage(0)
    p.PlotTestControlVoltage(1)
    plt.show()

if __name__ == '__main__':
        main(sys.argv)





