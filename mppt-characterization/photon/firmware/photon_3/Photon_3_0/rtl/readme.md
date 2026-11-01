Install icarus verilog

sudo apt-get update
sudo apt-get install iverilog

Install iCEcube2

latticesemi.com
disk2/Software/Lattice/iCEcube2setup
sudo apt-get install libxext6:i386
it only looks for an eth0 adapter
change /etc/udev/rules.d/70-persistent-net.rules

Very stupid:
synthesize needs sh to point to bash
sudo ln -sf bash /bin/sh



