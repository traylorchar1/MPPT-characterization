#!/bin/bash
# utility to flash dspics

# make build.h
echo "#define BUILDNUMBER $(date +%y%m%d%H%M)" > build.h
make clean
make
