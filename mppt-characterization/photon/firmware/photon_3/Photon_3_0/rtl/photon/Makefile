

RM = rm

SIM_TARG = top_tb
SIM_INC = top.v mppt_fsm.v top_pll_tb.v dff.v dff_deglitch.v
SYN_INC = top.v top_pll.v mppt_fsm.v
SYN_TOP = top.v

SYNPRO = /opt/lscc/iCEcube2.2017.01/sbt_backend/bin/linux/opt/synpwrap/synpwrap 

#syn : $(SYN_INC)
#	$(SYNPRO) -prj "photon_syn.prj" -log "build/photon.srr"

top : top.v
	iverilog $(SIM_TARG).v $(SIM_INC) -o $(SIM_TARG).o
	vvp $(SIM_TARG).o
	gtkwave -f $(SIM_TARG).vcd &

dff : dff_deglitch_tb.v
	iverilog dff_deglitch_tb.v dff_deglitch.v -o dff_deglitch_tb.o
	vvp dff_deglitch_tb.o
	gtkwave -f dff_deglitch_tb.vcd

clean : 
	rm -f *.vvp *.vcd *.o *.log *.bak
	rm -rf photon_Implmnt
