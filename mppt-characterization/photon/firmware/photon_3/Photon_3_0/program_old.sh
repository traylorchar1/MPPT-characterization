#!/bin/bash
# utility to flash dspics
#sudo /opt/microchip/mplabx/mplab_ide/bin/mdb.sh program.cmd

#../../can_bootloader/python/can_program_cli.py --bitrate=500000 --baseaddr=0x602 --hexfile=fw.hex
#../../can_bootloader/python/can_program_cli.py --bitrate=500000 --load_params_file=../../can_bootloader/python/parameters_013.txt --bitstream=../../photon_3/Photon_3_0/rtl/photon/photon_Implmnt/sbt/outputs/bitmap/top_bitmap_int.hex --verify=False
../../can_bootloader/python/can_program_cli.py --baseaddr=0x600 --bitrate=500000 --load_params_file=../../can_bootloader/python/parameters_011.txt --bitstream=../../photon_3/Photon_3_0/rtl/photon/photon_Implmnt/sbt/outputs/bitmap/top_bitmap_int.hex --verify=False --force_program --hexfile=/tmp/fw_old.hex
