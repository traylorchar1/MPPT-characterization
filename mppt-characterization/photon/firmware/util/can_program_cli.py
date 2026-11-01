#!/usr/bin/env python
"""Copyright (c) 2017, Dilithium Power Systems LLC All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

The views and conclusions contained in the software and documentation are those
of the authors and should not be interpreted as representing official policies,
either expressed or implied, of Dilithium Power Systems LLC.

can_program_cli is built as a command line tool for programming Photon 3.x MPPTs

Example useage:
  ./can_program_cli.py --help
  ./can_program_cli.py --bitrate=500000 --load_params_file=parameters_006.txt --baseaddr=0x600 --hexfile=fw.hex

"""

import os
import sys

# Get the import paths right
_MOD_DIR = os.path.abspath(os.path.dirname(__file__))
_PHOTON_3_DIR = os.path.join(_MOD_DIR, '../photon_3/Photon_3_0/')
_PHOTON_3_BOOTLOAD_DIR = os.path.join(_MOD_DIR, '../can_bootloader/')
_CAN_LOGGER_DIR = os.path.join(_MOD_DIR, '../can_logger/')

sys.path.insert(1, os.path.join(_MOD_DIR, '../canbus'))
sys.path.insert(1, os.path.join(_MOD_DIR, '../nanopb/generator/proto'))
sys.path.insert(1, os.path.join(_MOD_DIR, _PHOTON_3_DIR, 'python'))

import binascii
import gflags
import google.protobuf.text_format as text_format
import intelhex
import logging
import struct
import time

import can_ethernet
import can_utils
import can_msg_pb2
import params_pb2

# Initializing google flags module
FLAGS = gflags.FLAGS

# Control flags
gflags.DEFINE_bool('verify', True, 'Verify the code was written correctly.')
gflags.DEFINE_bool('force_program', False,
                   'Write firmware even if checksum matches.')
gflags.DEFINE_integer('base_addr', None,
                      'Address of the tracker to be programmed.')
gflags.DEFINE_integer('bitrate', None, 'Speed of the CAN Bus.')
gflags.DEFINE_string('hexfile',
                     os.path.join(_PHOTON_3_DIR, 'fw.hex'),
                     'Hexfile to read firmware from')
gflags.DEFINE_string(
    'bitstream',
    #_PHOTON_3_DIR+'rtl/photon/photon_Implmnt/sbt/outputs/bitmap/top_bitmap_int.hex',
    os.path.join(_PHOTON_3_DIR, 'rtl/fpga.hex'),
    'Bitstream to load onto FPGA')
gflags.DEFINE_string('save_params_file', None,
                     'File that device parameters will be written to.')
gflags.DEFINE_string('load_params_file', None,
                     'File that device parameters will be loaded from.')
gflags.DEFINE_integer('serial_number', None, 'Serial Number of Unit')

# Command list
_READ_LATCH = 0x01
_WRITE_LATCH = 0x02
_WRITE_BLOCK = 0x03
_ERASE_BLOCK = 0x04
_ACK = 0x05
_JUMP_TO_USER_CODE = 0x06
_PING = 0x07
_WRITE_FRAM = 0x08
_READ_FRAM = 0x09
_READ_HW_REV = 0x0A
_WRITE_FPGA_FLASH = 0x10
_READ_FPGA_FLASH = 0x11
_ERASE_FPGA_FLASH = 0x12
_RESET = 0xFE

_SPECIAL_COMMAND_OFFSET = 0xA0
_TEST_COMMAND_OFFSET = 0xB0
_BURST_COMMAND_OFFSET = 0xC0

_BOOTLOADER_PING = 0x01
_USER_CODE_PING = 0x02

_BLOCK_SIZE = 0x400
_ROW_SIZE = 0x80

_MAX_PROGRAM_ADDR = 0xa9fe
_MAX_PROGRAM_DATA = 0xffffff

_BOOTLOAD_START = 0x7000
_FW_START = 0x000

_N_BLOCKS = (_BOOTLOAD_START - _FW_START) // _BLOCK_SIZE
_N_ROWS = (_BOOTLOAD_START - _FW_START) // _ROW_SIZE
_N_LATCHES = (_BOOTLOAD_START - _FW_START) // 2

_FRAM_SIZE = 0x200
_FLASH_SIZE = 0x20000

# Time from sending reset command to reliably in the bootloader
_RESET_TIME = 1

_SUPPORTED_BITRATES = [500000, 250000, 125000]
#_SUPPORTED_BITRATES = [500000]

_MAX_CAN_ADDR = 0x70F

_BITSTREAM_SZ = 71337

#_BITSTREAM_SZ = 128



class CanProgram(object):
  """CanProgram is a class for holding the functions for programming a
  Photon 3.1 over a CANBUS.

  """

  def _PrintProgress(self, progress):
    progress = int(progress)
    bars = max(0, min(50, progress // 2))
    sys.stdout.write('\r[{0}] {1}%'.format('#' * bars, progress))
    sys.stdout.flush()

  def _FinishProgress(self):
    self._PrintProgress(100)
    sys.stdout.write('\r\n')
    sys.stdout.flush()

  def _CheckCanBitrate(self, bitr):
    if not isinstance(bitr, int):
      raise TypeError('CAN Bitrate must be integer: %s' % repr(type(bitr)))
    if bitr not in _SUPPORTED_BITRATES:
      raise ValueError('CAN Bitrate %d must be be in : %s' %
                       (bitr, repr(_SUPPORTED_BITRATES)))

  def _CheckCanAddr(self, addr):
    if not isinstance(addr, int):
      raise TypeError('CAN Address must be integer: %s' % repr(type(addr)))
    if not 0 <= addr <= _MAX_CAN_ADDR:
      raise ValueError('CAN Address: 0x%04x must be between 0 and 0x04x' %
                       (addr, _MAX_CAN_ADDR))

  def _CheckRow(self, addr):
    if not isinstance(addr, int):
      raise TypeError('Address must be integer: %s' % repr(type(addr)))
    if addr % _ROW_SIZE != 0:
      raise ValueError('Address: 0x%08x not on a row boundry' % (addr))

  def _CheckBlock(self, addr):
    if not isinstance(addr, int):
      raise TypeError('Address must be integer: %s' % repr(type(addr)))
    if addr % _BLOCK_SIZE != 0:
      raise ValueError('Address: 0x%08x not on a block boundry' % (addr))

  def _CheckProgramAddr(self, addr):
    if not isinstance(addr, int):
      raise TypeError('Address must be integer: %s' % repr(type(addr)))
    if not 0 <= addr < _BOOTLOAD_START:
      raise ValueError('Address: 0x%08x greater then max allowed: 0x08x' %
                       (addr, _BOOTLOAD_START))

  def _CheckFramAddr(self, addr):
    if not isinstance(addr, int):
      raise TypeError('Address must be integer: %s' % repr(type(addr)))
    if not 0 <= addr < _FRAM_SIZE:
      raise ValueError('Address: 0x%08x greater then max allowed: 0x08x' %
                       (addr, _FRAM_SIZE))

  def _CheckFlashAddr(self, addr):
    if not isinstance(addr, int):
      raise TypeError('Address must be integer: %s' % repr(type(addr)))
    if not 0 <= addr < _FLASH_SIZE:
      raise ValueError('Address: 0x%08x greater then max allowed: 0x08x' %
                       (addr, _FLASH_SIZE))

  def _CheckProgramData(self, data):
    if not isinstance(data, int):
      raise TypeError('Data needs to be an integer: %s' % repr(type(data)))
    if not 0 <= data <= _MAX_PROGRAM_DATA:
      raise ValueError('Data: 0x%08x needs to be less then 0x%08x' %
                       (data, _MAX_PROGRAM_DATA))

  def _CheckFramData(self, data):
    if not isinstance(data, int):
      raise TypeError('Data needs to be an integer: %s' % repr(type(data)))
    if not 0 <= data <= 0xffffffff:
      raise ValueError('Data: 0x%08x needs to be less then 0x%08x' %
                       (data, 0xffffffff))

  def _LatchDataFromHex(self, addr, hexf):
    b = addr * 2
    data_chunk = bytearray([hexf[b], hexf[b + 1], hexf[b + 2], 0])
    return struct.unpack('I', data_chunk)[0]

  def _FpgaFlashDataFromHex(self, addr, bits):
    b = addr
    data_chunk = bytearray([bits[b], bits[b + 1], bits[b + 2], bits[b + 3]])
    #data_chunk = bytearray([0x13, 0x13, 0x13, 0x13])
    return struct.unpack('I', data_chunk)[0]

  def _ToSignedInt32(self, value):
    value = value & 0xFFFFFFFF
    if value >= 0x80000000:
      value -= 0x100000000
    return value

  def _FmtHex32(self, value):
    return '%08X' % (value & 0xFFFFFFFF)

  def _GetFirmwareCRC32(self, hexf):
    crc32 = binascii.crc32(hexf.tobinstr(start=0, size=_MAX_PROGRAM_ADDR))
    return self._ToSignedInt32(crc32)

  def SendRtrPacket(self, unit_addr):
    tx = can_msg_pb2.CanMessage()
    tx.id = unit_addr
    tx.type = can_msg_pb2.STD_RTR
    logging.debug('CAN_PROG Send RTR to Device 0x%X', tx.id)
    self.interface.SendPkt(tx)

  def SendPing(self, unit_addr):
    tx = can_msg_pb2.CanMessage()
    tx.id = unit_addr + _SPECIAL_COMMAND_OFFSET
    tx.data.extend([_PING, 0, 0, 0, 0, 0, 0, 0])
    logging.info('CAN_PROG Pinging Device 0x%X (0x%X)', unit_addr, tx.id)
    self.interface.SendPkt(tx)

  def GetPing(self, timeout=1.0, unit_addr=None):
    if not unit_addr:
      unit_addr = self.unit_addr
    rx = self.WaitForAck(timeout=timeout, addr=unit_addr)
    code_type = rx.data[1]
    self.FlushPackets()
    if code_type not in [_BOOTLOADER_PING, _USER_CODE_PING]:
      raise ValueError('Code Type %d not a valid type.' % code_type)
    if code_type == _BOOTLOADER_PING:
      logging.info('CAN_PROG Bootloader detected by ping at %x', unit_addr)
    if code_type == _USER_CODE_PING:
      logging.info('CAN_PROG User Code detected by ping at %x', unit_addr)

    # return the ping type - detects what we're talking to.
    return code_type

  def Ping(self, timeout=1.0, unit_addr=None):
    if not unit_addr:
      unit_addr = self.unit_addr
    self.SendPing(unit_addr)
    return self.GetPing(timeout=timeout, unit_addr=unit_addr)

  def Reset(self, addr=None, tries=3):
    for attempt in range(tries):
      try: 
        if not addr:
          addr = self.unit_addr
        tx = can_msg_pb2.CanMessage()
        tx.id = addr + _SPECIAL_COMMAND_OFFSET
        logging.info('CAN_PROG Resetting Device at 0x%X (0x%X)', addr, tx.id)
        # Magic byte packet that ensures that reset is not called accidentally.
        tx.data.extend([_RESET, 0, 0, 0, 45, 78, 69, 21])
        self.FlushPackets()
        self.interface.SendPkt(tx)
        self.WaitForAck()
        time.sleep(_RESET_TIME)
        self.FlushPackets()
        break
      except can_utils.TimeoutError:
        logging.warning('CAN_PROG Reset() Retry')

  def EraseFpgaFlash(self, addr=None):
    if not addr:
      addr = self.unit_addr
    logging.info('CAN_PROG Erasing FPGA Flash at %X', addr)
    tx = can_msg_pb2.CanMessage()
    tx.id = addr + _SPECIAL_COMMAND_OFFSET
    # Magic byte packet that ensures that reset is not called accidentally.
    tx.data.extend([_ERASE_FPGA_FLASH, 0, 0, 0, 0, 0, 0, 0])
    self.FlushPackets()
    self.interface.SendPkt(tx)
    time.sleep(1)
    self.WaitForAck()
    self.FlushPackets()

  def SetTestModeDac(self, ch_0_dac, ch_1_dac):
    #logging.info('CAN_PROG Setting Dac')
    tx = can_msg_pb2.CanMessage()
    tx.id = self.unit_addr + _TEST_COMMAND_OFFSET
    # Magic byte packet that ensures that reset is not called accidentally.
    ch_0_hb, ch_0_lb = divmod(ch_0_dac, 0x100)
    ch_1_hb, ch_1_lb = divmod(ch_1_dac, 0x100)
    tx.data.extend([ch_0_lb, ch_0_hb, ch_1_lb, ch_1_hb, 45, 78, 69, 21])
    self.SendWithAckRetry(tx)

  def SetControlLoopBurstMode(self, mode, n_packets=200):
    logging.info('CAN_PROG Setting Data Burst Mode, n = %d', n_packets)
    tx = can_msg_pb2.CanMessage()
    tx.id = self.unit_addr + _BURST_COMMAND_OFFSET
    # Magic byte packet that ensures that reset is not called accidentally.
    np_hb, np_lb = divmod(n_packets, 0x100)
    tx.data.extend([np_lb, np_hb, 0, 0, 45, 78, 69, 21])
    self.interface.SendPkt(tx)

  def JumpToUserCode(self):
    logging.info('CAN_PROG Jumping to User Code')
    tx = can_msg_pb2.CanMessage()
    tx.id = self.unit_addr + _SPECIAL_COMMAND_OFFSET
    tx.data.extend([_JUMP_TO_USER_CODE, 0, 0, 0, 0, 0, 0, 0])
    self.FlushPackets()
    self.interface.SendPkt(tx)
    self.WaitForAck()

  def SendWithAckRetry(self, tx, tries=5):
    for attempt in range(tries):
      try:
        self.FlushPackets()
        self.interface.SendPkt(tx)
        self.WaitForAck()
        self.FlushPackets()
        return
      except:
        logging.warning('Send Retry %d', attempt)
    raise AssertionError('Retry Failure')

  def WaitForAck(self, timeout=1, addr=None):
    if not addr:
      addr = self.unit_addr
    logging.debug('CAN_PROG Waiting For Acknoledgement')
    tx = can_msg_pb2.CanMessage()
    tx.id = addr + _SPECIAL_COMMAND_OFFSET
    rx = self.interface.WaitForPacket(tx, timeout)
    if not rx.data[0] == _ACK:
      raise AssertionError('Failed to receive packet acknoledgement!')
    return rx

  def EraseDevice(self):
    n = 0
    logging.info('CAN_PROG Erasing Device')
    for block in range(_FW_START, _BOOTLOAD_START, _BLOCK_SIZE):
      self.EraseBlock(block)
      n = n + 1
      if n % 10:
        self._PrintProgress(n * 100 // _N_BLOCKS)
    self._FinishProgress()

  def WriteFirmware(self, hexf):
    n = 0
    logging.info('CAN_PROG Writing Firmware')
    for row in range(_FW_START, _BOOTLOAD_START, _ROW_SIZE):
      for addr in range(row, row + _ROW_SIZE, 2):
        write = self._LatchDataFromHex(addr, hexf)
        self.WriteLatch(addr, write)
      self.WriteRow(row)
      n = n + 1
      if n % 10:
        self._PrintProgress(n * 100 // _N_ROWS)
    self._FinishProgress()

  def WriteBitstream(self, bits):
    logging.info('CAN_PROG Writing FPGA_FLASH Bitstream')

    # Write the length of the bitstream as the first 4 bytes
    self.WriteFpgaFlashVal(0, _BITSTREAM_SZ)

    for addr in range(0, _BITSTREAM_SZ, 4):
      write = self._FpgaFlashDataFromHex(addr, bits)
      #print '%x:\t%x\t%x\t%x\t%x' % (addr, bits[addr], bits[addr+1], bits[addr+2], bits[addr+3])
      self.WriteFpgaFlashVal(addr + 4, write)

      if addr % 100:
        self._PrintProgress(addr * 100 / _BITSTREAM_SZ)
    self._FinishProgress()

  def VerifyBitstream(self, bits):
    logging.info('CAN_PROG Verifying Bitstream')
    self.VerifyFpgaFlash(0, bits)
    for addr in range(0, _BITSTREAM_SZ, 4):
      self.VerifyFpgaFlash(addr, bits)
      if addr % 100:
        self._PrintProgress(addr * 100 / _BITSTREAM_SZ)
    self._FinishProgress()

  def VerifyFirmware(self, hexf):
    n = 0
    logging.info('CAN_PROG Verifying Firmware')
    for addr in range(_FW_START, _BOOTLOAD_START, 2):
      self.VerifyLatch(addr, hexf)
      n = n + 1
      if n % 10:
        self._PrintProgress(n * 100 // _N_LATCHES)
    self._FinishProgress()

  def EraseBlock(self, addr):
    self._CheckProgramAddr(addr)
    self._CheckBlock(addr)

    tx = can_msg_pb2.CanMessage()
    tx.id = self.unit_addr + _SPECIAL_COMMAND_OFFSET
    tx.data.extend([_ERASE_BLOCK, 0, 0, 0, 0, 0, 0, 0])
    tx.data[1:3] = bytearray(struct.pack('H', addr))

    self.FlushPackets()
    self.interface.SendPkt(tx)
    self.WaitForAck()

    logging.debug('CAN_PROG er_blk:\t%08x', addr)

  def WriteRow(self, addr):
    self._CheckProgramAddr(addr)
    self._CheckRow(addr)

    tx = can_msg_pb2.CanMessage()
    tx.id = self.unit_addr + _SPECIAL_COMMAND_OFFSET
    tx.data.extend([_WRITE_BLOCK, 0, 0, 0, 0, 0, 0, 0])
    tx.data[1:3] = bytearray(struct.pack('H', addr))

    self.FlushPackets()
    self.interface.SendPkt(tx)
    self.WaitForAck()

    logging.debug('CAN_PROG wr_blk:\t%08x', addr)

  def WriteLatch(self, addr, data):
    self._CheckProgramAddr(addr)
    self._CheckProgramData(data)

    tx = can_msg_pb2.CanMessage()
    tx.id = self.unit_addr + _SPECIAL_COMMAND_OFFSET
    tx.data.extend([_WRITE_LATCH, 0, 0, 0, 0, 0, 0, 0])
    tx.data[1:3] = bytearray(struct.pack('H', addr))
    tx.data[4:9] = bytearray(struct.pack('I', data))

    self.FlushPackets()
    self.interface.SendPkt(tx)
    self.WaitForAck()

    logging.debug('CAN_PROG wr_lat:\t%08x\tdata:\t%08x', addr, data)

  def WriteFramVal(self, addr, data):
    self._CheckFramAddr(addr)
    self._CheckFramData(data)

    tx = can_msg_pb2.CanMessage()
    tx.id = self.unit_addr + _SPECIAL_COMMAND_OFFSET
    tx.data.extend([_WRITE_FRAM, 0, 0, 0, 0, 0, 0, 0])
    tx.data[1:3] = bytearray(struct.pack('H', addr))
    tx.data[4:9] = bytearray(struct.pack('I', data))

    self.FlushPackets()
    self.interface.SendPkt(tx)
    self.WaitForAck()

    logging.debug('CAN_PROG wr_fram:\t%08x\tdata:\t%08x', addr, data)

  def ReadFramVal(self, addr):
    self._CheckFramAddr(addr)

    tx = can_msg_pb2.CanMessage()
    tx.id = self.unit_addr + _SPECIAL_COMMAND_OFFSET
    tx.data.extend([_READ_FRAM, 0, 0, 0, 0, 0, 0, 0])
    tx.data[1:3] = bytearray(struct.pack('H', addr))

    self.FlushPackets()
    self.interface.SendPkt(tx)
    rx = self.interface.WaitForPacket(tx)

    addr_chunk = bytearray(rx.data[1:4] + [0])
    data_chunk = bytearray(rx.data[4:9])

    addr = struct.unpack('I', addr_chunk)[0]
    data = struct.unpack('I', data_chunk)[0]

    logging.debug('CAN_PROG rd_fram:\t%08x\tdata:\t%08x', addr, data)

    return (addr, data)

  def ReadFpgaFlashVal(self, addr):
    self._CheckFlashAddr(addr)

    tx = can_msg_pb2.CanMessage()
    tx.id = self.unit_addr + _SPECIAL_COMMAND_OFFSET
    tx.data.extend([_READ_FPGA_FLASH, 0, 0, 0, 0, 0, 0, 0])
    tx.data[1:3] = bytearray(struct.pack('H', (addr & 0xFFFF)))
    tx.data[3] = addr >> 16

    self.FlushPackets()
    self.interface.SendPkt(tx)
    rx = self.interface.WaitForPacket(tx)

    addr_chunk = bytearray(rx.data[1:4] + [0])
    data_chunk = bytearray(rx.data[4:9])

    addr = struct.unpack('I', addr_chunk)[0]
    data = struct.unpack('I', data_chunk)[0]

    logging.debug('CAN_PROG rd_fram:\t%08x\tdata:\t%08x', addr, data)

    return (addr, data)

  def WriteFpgaFlashVal(self, addr, data):
    self._CheckFlashAddr(addr)

    tx = can_msg_pb2.CanMessage()
    tx.id = self.unit_addr + _SPECIAL_COMMAND_OFFSET
    tx.data.extend([_WRITE_FPGA_FLASH, 0, 0, 0, 0, 0, 0, 0])
    tx.data[1:3] = bytearray(struct.pack('H', (addr & 0xFFFF)))
    tx.data[4:9] = bytearray(struct.pack('I', data))
    tx.data[3] = addr >> 16

    self.FlushPackets()
    self.interface.SendPkt(tx)
    self.WaitForAck()

    logging.debug('CAN_PROG wr_fpga:\t%08x\tdata:\t%08x', addr, data)

  def ReadHwRev(self):
    tx = can_msg_pb2.CanMessage()
    tx.id = self.unit_addr + _SPECIAL_COMMAND_OFFSET
    tx.data.extend([_READ_HW_REV, 0, 0, 0, 0, 0, 0, 0])

    self.FlushPackets()
    self.interface.SendPkt(tx)
    rx = self.interface.WaitForPacket(tx)
    data_chunk = bytearray(rx.data[4:9])
    hw_rev = struct.unpack('I', data_chunk)[0]

    logging.info('CAN_PROG Detected Hardware Revision %d', hw_rev)

    return hw_rev

  def FlushPackets(self, addr=None):

    tx = can_msg_pb2.CanMessage()
    tx.id = self.unit_addr + _SPECIAL_COMMAND_OFFSET
    self.interface.FlushQueueType(tx)

  def ReadLatch(self, addr):
    self._CheckProgramAddr(addr)

    tx = can_msg_pb2.CanMessage()
    tx.id = self.unit_addr + _SPECIAL_COMMAND_OFFSET
    tx.data.extend([_READ_LATCH, 0, 0, 0, 0, 0, 0, 0])
    tx.data[1:3] = bytearray(struct.pack('H', addr))

    self.interface.SendPkt(tx)
    rx = self.interface.WaitForPacket(tx)

    addr_chunk = bytearray(rx.data[1:4] + [0])
    data_chunk = bytearray(rx.data[4:9])

    addr = struct.unpack('I', addr_chunk)[0]
    data = struct.unpack('I', data_chunk)[0]

    logging.debug('CAN_PROG rd_lat:\t%08x\tdata:\t%08x', addr, data)

    return (addr, data)

  def EraseFram(self):
    logging.info('CAN_PROG Erasing FRAM')
    for addr in range(0, _FRAM_SIZE, 4):
      self.WriteFramVal(addr, 0)
      self._PrintProgress(addr * 100 / _FRAM_SIZE)
    self._FinishProgress()

  def VerifyLatch(self, addr, hexf):
    # This hack is to account for the fact that we're jumping to the bootloader
    if addr == 0:
      verify = 0x040000 + _BOOTLOAD_START
    else:
      verify = self._LatchDataFromHex(addr, hexf)
    (addr, data) = self.ReadLatch(addr)

    if data != verify:
      raise ValueError(
          'Data: 0x%08x at addr: 0x%08x does not match expected: 0x%08x' %
          (data, addr, verify))

  def VerifyFpgaFlash(self, addr, bits):
    verify = self._FpgaFlashDataFromHex(addr, bits)
    (addr, data) = self.ReadFpgaFlashVal(addr + 4)

    if data != verify:
      raise ValueError(
          'Data: 0x%08x at addr: 0x%08x does not match expected: 0x%08x' %
          (data, addr, verify))

  def LoadParamsFromFile(self, fname):
    f = open(fname, 'r')
    s = f.read()
    f.close()
    msg = params_pb2.ParamsMessage()
    text_format.Merge(s, msg)
    logging.info('CAN_PROG Loading parameters from file %s:', fname)
    print(str(msg))
    return msg

  def SaveParamsToFile(self, fname, msg):
    f = open(fname, 'w')
    f.write(str(msg))
    f.close()
    logging.info('CAN_PROG Saving parameters to file %s:', fname)
    print(str(msg))

  def ReadParamsFromFram(self):
    logging.info('CAN_PROG Reading Parameters From FRAM')

    # Read out the length
    (_, msg_len) = self.ReadFramVal(0)

    fram = bytearray()
    for addr in range(4, msg_len + 4, 4):
      (_, data) = self.ReadFramVal(addr)
      data = bytearray(struct.pack('I', data))
      for i in range(4):
        if addr - 4 + i < msg_len:
          fram.append(data[i])
      self._PrintProgress(addr * 100 / _FRAM_SIZE)
    self._FinishProgress()
    msg = params_pb2.ParamsMessage()
    msg.ParseFromString(bytes(fram))
    return msg

  def WriteParamsToFram(self, params):
    logging.info('CAN_PROG Writing Parameters To FRAM')
    s = params.SerializeToString()

    # Write out the length
    self.WriteFramVal(0, len(s))

    # Write the rest of the message
    c = 0
    for addr in range(4, len(s) + 4, 4):
      data_chunk = bytearray([0] * 4)
      c = 0
      for i in range(addr, min(addr + 4, len(s) + 4)):
        data_chunk[c] = s[i - 4]
        c = c + 1
      data = struct.unpack('I', data_chunk)[0]
      self.WriteFramVal(addr, data)
      self._PrintProgress(addr * 100 / _FRAM_SIZE)
    self._FinishProgress()

  def BusEnumerate(self):
    units = []
    for unit_addr in range(self.base_addr, self.base_addr + 0x10):
      self.SendPing(unit_addr=unit_addr)

    time.sleep(0.2)
    for unit_addr in range(self.base_addr, self.base_addr + 0x10):
      try:
        self.SetUnitAddr(unit_addr)
        self.SendRtrPacket(unit_addr)
        code = self.GetPing(timeout=0.02, unit_addr=unit_addr)
        units.append(MpptUnit(self.interface, unit_addr))
      except can_utils.TimeoutError:
        logging.info('CAN_PROG Unit %x did not respond to ping.', unit_addr)
        pass
    for unit in units:
        unit.Reset()
        unit.fram = unit.ReadParamsFromFram()
        #unit.Reset()
        print(str(unit.fram))
    return units 

  def Program(self, hexfile, bitstream, params_file=None, gui_mode=False,
      force_program=False):
    start_t = time.time()

    force_program = force_program or FLAGS.force_program
    
    # Try out different bitrates if one is not specified.
    if not gui_mode:
      if FLAGS.bitrate:
        self._CheckCanBitrate(FLAGS.bitrate)
        self.interface.Connect(FLAGS.bitrate)
      else:
        success = False
        for bitrate in _SUPPORTED_BITRATES:
          self.interface.Connect(bitrate)
          try:
            self.BusEnumerate()
            ping = self.Ping()
            if ping in [_USER_CODE_PING, _BOOTLOADER_PING]:
              # If we got here, this is the right speed - break
              success = True
              break
          except can_utils.TimeoutError:
            # try again
            pass
        if not success:
          logging.error('Device did not respond to ping')

    self.Reset()
    try:
      ping = self.Ping()
    except can_utils.TimeoutError:
      ping = None

    if ping != _BOOTLOADER_PING:
      logging.error('Could not talk to bootloader - ping failed.')

    fram = self.ReadParamsFromFram()

    logging.info('CAN_PROG Current Firmware CRC32: %s',
                 self._FmtHex32(fram.code.crc32))

    hex_crc32 = self._GetFirmwareCRC32(hexfile)
    logging.info('CAN_PROG Firmware Hexfile CRC32: %s',
                 self._FmtHex32(hex_crc32))

    logging.info(
        'CAN_PROG Current Bitstream CRC32: %s',
        self._FmtHex32(fram.code.bitstream_crc32))

    bitstream_crc32 = self._GetFirmwareCRC32(bitstream)
    logging.info('CAN_PROG Bitstream Hexfile CRC32: %s',
                 self._FmtHex32(bitstream_crc32))

    hw_rev = self.ReadHwRev()


    # Test if the crc matches - if not, program the device
    if fram.code.crc32 != hex_crc32 or force_program:
      t_p = time.time()
      self.EraseDevice()
      self.WriteFirmware(hexfile)
      if FLAGS.verify:
        self.VerifyFirmware(hexfile)
      logging.info('CAN_PROG Writing CRC32 to FRAM: %s',
                   self._FmtHex32(hex_crc32))
      fram = self.ReadParamsFromFram()
      fram.code.crc32 = hex_crc32
      fram.device.hw_rev = hw_rev
      self.WriteParamsToFram(fram)
      logging.info('CAN_PROG Programmed DSP in %0.1fs', (time.time() - t_p))
    else:
      logging.info('CAN_PROG DSP CRC Match - Not overwriting firmware')

    # If the device is a 3.2.0 or greater also program the bitstream
    if fram.device.hw_rev >= 2:

      if fram.code.bitstream_crc32 != bitstream_crc32 or force_program:
        t_p = time.time()
        self.EraseFpgaFlash()
        self.WriteBitstream(bitstream)
        if FLAGS.verify:
          self.VerifyBitstream(bitstream)
        fram = self.ReadParamsFromFram()
        fram.code.bitstream_crc32 = bitstream_crc32
        self.WriteParamsToFram(fram)
        logging.info('CAN_PROG Programmed FPGA in %0.1fs', (time.time() - t_p))
      else:
        logging.info('CAN_PROG FPGA CRC Match - Not overwriting bitstream')

    # If the load file is defined - try loading those parameters to the device
    if FLAGS.load_params_file and params_file is None:
      params_file = FLAGS.load_params_file
    if params_file:
      fram = self.ReadParamsFromFram()
      params = self.LoadParamsFromFile(params_file)

      # Ensure that this params file is for the right unit
      if fram.device.serial_number and not force_program:
        if params.device.serial_number != fram.device.serial_number:
          raise IndexError(
              'Device serial %d does not match parameters file %s.' %
              (fram.device.serial_number, FLAGS.load_params_file))

      self._CheckCanBitrate(params.comm.can_bitrate)
      self._CheckCanAddr(params.comm.can_base_addr)

      # Overwrite the crc32 and hw_rev with the value read from the device
      params.code.crc32 = fram.code.crc32
      params.device.hw_rev = hw_rev

      if fram.device.hw_rev >= 2:
        params.code.bitstream_crc32 = fram.code.bitstream_crc32

      # Write out the data
      self.WriteParamsToFram(params)

    # If the save file is defined - try saving the parameters to the file
    if FLAGS.save_params_file:
      self.SaveParamsToFile(FLAGS.save_params_file, self.ReadParamsFromFram())

    self.Reset()
    logging.info('CAN PROG Programming Complete')

  def SetBaseAddr(self, addr):
    self.base_addr = addr

  def SetUnitAddr(self, addr):
    self.unit_addr = addr

  def SetInterface(self, interface):
    self.interface = interface

  def __init__(self, interface):
    self.SetInterface(interface)


class MpptUnit(CanProgram):
  def __init__(self, interface, unit_addr):
    CanProgram.__init__(self, interface)
    self.SetUnitAddr(unit_addr)
    self.SetBaseAddr(unit_addr)


def main(argv):
  # Parse commandline flags.
  try:
    argv = FLAGS(argv)
  except gflags.FlagsError as e:
    print('%s\\nUsage: %s ARGS\\n%s' % (e, sys.argv[0], FLAGS))
    sys.exit(1)

  logging.basicConfig(
      level=logging.INFO, format='%(asctime)s %(levelname)s %(message)s')

  if not FLAGS.load_params_file:
    FLAGS.load_params_file = (
        _CAN_LOGGER_DIR +
        'params/parameters_%03d.txt' % FLAGS.serial_number)

  if FLAGS.hexfile:
    # load application firmware
    logging.info('CAN_PROG Loading hexfile: %s', FLAGS.hexfile)
    hexfile = intelhex.IntelHex(FLAGS.hexfile)

  if FLAGS.bitstream:
    logging.info('CAN_PROG Loading bitstream: %s', FLAGS.bitstream)
    bitstream = intelhex.IntelHex(FLAGS.bitstream)

  can = can_ethernet.CanEthernet()

  cli = CanProgram(can)

  # If a base_addr or bitrate isn't defined - try loading from the file
  if (not FLAGS.base_addr) or (not FLAGS.bitrate):
    params = cli.LoadParamsFromFile(FLAGS.load_params_file)

  if not FLAGS.base_addr:
    cli.SetBaseAddr(params.comm.can_base_addr)
    cli.SetUnitAddr(params.comm.can_base_addr)
  else:
    cli.SetBaseAddr(FLAGS.base_addr)
    cli.SetUnitAddr(FLAGS.base_addr)

  #if not FLAGS.bitrate:
  #  FLAGS.bitrate = params.comm.can_bitrate

  cli.Program(hexfile, bitstream)

  can.Close()


if __name__ == '__main__':
  main(sys.argv)
