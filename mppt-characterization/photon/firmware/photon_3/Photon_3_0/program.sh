#!/bin/bash
# utility to flash dspics
#sudo /opt/microchip/mplabx/mplab_ide/bin/mdb.sh program.cmd

#../../can_bootloader/python/can_program_cli.py --bitrate=500000 --baseaddr=0x602 --hexfile=fw.hex
#../../can_bootloader/python/can_program_cli.py --bitrate=500000 --load_params_file=../../can_bootloader/python/parameters_005.txt --bitstream=../../photon_3/Photon_3_0/rtl/photon/photon_Implmnt/sbt/outputs/bitmap/top_bitmap_int.hex --verify=False
#../../can_bootloader/python/can_program_cli.py --load_params_file=../../can_bootloader/python/parameters_005.txt --bitstream=../../photon_3/Photon_3_0/rtl/photon/photon_Implmnt/sbt/outputs/bitmap/top_bitmap_int.hex
../../util/can_program_cli.py --serial_number=16 --verify=False --bitrate=125000 --force_program=True
