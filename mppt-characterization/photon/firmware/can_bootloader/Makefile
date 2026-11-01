# Makefile for building can bootloader code

AS = /opt/microchip/xc16/v1.35/bin/xc16-as 
CC = /opt/microchip/xc16/v1.35/bin/xc16-gcc
LD = /opt/microchip/xc16/v1.35/bin/xc16-ld 
AR = /opt/microchip/xc16/v1.35/bin/xc16-ar
HX = /opt/microchip/xc16/v1.35/bin/xc16-bin2hex
RM = rm
CFLAGS = -mcpu=33FJ64GS606 -x c -g -Wall -merrata=psv_trap -O1 -mlarge-data
AFLAGS = -mcpu=33FJ64GS606 -Wa,-ahmcls=$*.lst
LFLAGS = -o "fw.cof" -Wl,--script="../dspic_include/p33FJ64GS606_bootloader.gld",--defsym=__MPLAB_BUILD=1,--heap=512,-Map="fw.map",--report-mem
OBJS := can.o general.o fram.o leds.o main.o oscillator.o timer.o debug.o memory.o prog_memory.o params.pb.o pb_encode.o pb_decode.o pb_common.o

NANOPB_DIR = ../nanopb
PHOTON3_DIR = ../photon_3/Photon_3_0

INC := $(NANOPB_DIR)



# from http://scottmcpeak.com/autodepend/autodepend.html

# make hex
fw.hex : fw.cof
	$(HX) "fw.cof"

# link
fw.cof : $(OBJS)
	$(CC) -mcpu=33FJ64GS606 $(OBJS) $(LFLAGS)
-include $(OBJS:.o=.d)

# compile
%.o : %.c params.pb
	$(CC) -c $(CFLAGS) $*.c -o $*.o -I $(INC)
	$(CC) -MM  $(CFLAGS) $*.c > $*.d -I $(INC)
	@mv -f $*.d $*.d.tmp
	@sed -e 's|.*:|$*.o:|' < $*.d.tmp > $*.d
	@sed -e 's/.*://' -e 's/\\$$//' < $*.d.tmp | fmt -1 | \
	  sed -e 's/^ *//' -e 's/$$/:/' >> $*.d
	@rm -f $*.d.tmp

# assemble
%.o : %.s
	$(CC) $(AFLAGS) -c $*.s -o $*.o 

params.pb : $(PHOTON3_DIR)/params.proto
	protoc -I /usr/include -I $(NANOPB_DIR)/generator/proto -o params.pb $(PHOTON3_DIR)/params.proto --proto_path=$(PHOTON3_DIR) 
	$(CC) -c $(CFLAGS) $(PHOTON3_DIR)/params.pb.c -I $(INC)
	$(CC) -c $(CFLAGS) $(NANOPB_DIR)/pb_encode.c -I $(INC)
	$(CC) -c $(CFLAGS) $(NANOPB_DIR)/pb_decode.c -I $(INC)
	$(CC) -c $(CFLAGS) $(NANOPB_DIR)/pb_common.c -I $(INC)



clean : 
	rm -f *.o *.cof *.hex *.map *.lst *.d *.pb* *.xml *.xml.* *.[0-9]



