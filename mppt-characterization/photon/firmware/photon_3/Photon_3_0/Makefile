# MPLAB IDE generated this makefile for use with GNU make.
# Project: photon 1_4.mcp
# Date: Mon May 19 22:53:17 2014

AS = xc16-as 
CC = xc16-gcc
LD = xc16-ld 
AR = xc16-ar
HX = xc16-bin2hex
RM = rm
CFLAGS = -mcpu=33FJ64GS606 -x c -g -Wall -merrata=psv_trap -O1 -mlarge-data -fgnu89-inline
AFLAGS = -mcpu=33FJ64GS606 -Wa,-ahmcls=$*.lst
LFLAGS = -o "fw.cof" -Wl,--script="../../dspic_include/p33FJ64GS606_main.gld",--defsym=__MPLAB_BUILD=1,--heap=512,-Map="fw.map",--report-mem
OBJS := periph_compat.o photon_service.o adc.o can.o cobs.o debug.o fifo.o fpga.o general.o i2c.o leds.o main.o oscillator.o pwm.o queue.o timer.o tracker.o packet.o safety.o comparator.o fram.o params.pb.o pb_decode.o pb_common.o thermistor.o

NANOPB_DIR = ../../nanopb
VPATH = $(NANOPB_DIR)

INC := $(NANOPB_DIR) ../../dspic_include
INCLUDES := $(addprefix -I,$(INC))


# from http://scottmcpeak.com/autodepend/autodepend.html

# make hex
fw.hex : fw.cof
	$(HX) "fw.cof"

# link
fw.cof : $(OBJS)
	$(CC) -mcpu=33FJ64GS606 $(OBJS) $(LFLAGS)
ifneq ($(filter clean,$(MAKECMDGOALS)),clean)
-include $(OBJS:.o=.d)
endif

# compile
%.o : %.c state.pb.h params.pb.h
	$(CC) -c $(CFLAGS) $< -o $*.o $(INCLUDES)
	$(CC) -MM $(CFLAGS) $< $(INCLUDES) > $*.d

# assemble
%.o : %.s
	$(CC) $(AFLAGS) -c $*.s -o $*.o 

python/log_pb2.py : log.proto
	protoc -I /usr/include -I $(NANOPB_DIR)/generator/proto -I . --python_out=python/ log.proto

params.pb : params.proto
	protoc -I /usr/include -I $(NANOPB_DIR)/generator/proto -I . -o params.pb params.proto
	protoc -I /usr/include -I $(NANOPB_DIR)/generator/proto -I . --python_out=python/ params.proto
	python $(NANOPB_DIR)/generator/nanopb_generator.py params.pb
	$(CC) -c $(CFLAGS) params.pb.c $(INCLUDES)
	$(CC) -c $(CFLAGS) $(NANOPB_DIR)/pb_encode.c $(INCLUDES)
	$(CC) -c $(CFLAGS) $(NANOPB_DIR)/pb_decode.c $(INCLUDES)
	$(CC) -c $(CFLAGS) $(NANOPB_DIR)/pb_common.c $(INCLUDES)

state.pb : state.proto python/log_pb2.py
	protoc -I /usr/include -I $(NANOPB_DIR)/generator/proto -I . -o state.pb state.proto
	protoc -I /usr/include -I $(NANOPB_DIR)/generator/proto -I . --python_out=python/ state.proto
	python $(NANOPB_DIR)/generator/nanopb_generator.py state.pb
	$(CC) -c $(CFLAGS) state.pb.c $(INCLUDES)
	$(CC) -c $(CFLAGS) $(NANOPB_DIR)/pb_encode.c $(INCLUDES)
	$(CC) -c $(CFLAGS) $(NANOPB_DIR)/pb_decode.c $(INCLUDES)
	$(CC) -c $(CFLAGS) $(NANOPB_DIR)/pb_common.c $(INCLUDES)


clean : 
	rm -f *.o *.cof *.hex *.map *.lst *.d *.pb*

