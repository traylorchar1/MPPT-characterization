

RM = rm

TARG = counter_tb
INC = counter.v

test : test.vcd
	gtkwave -f test.vcd &

# make test vectors
test.vcd : $(TARG).o
	vvp $(TARG).o

# compile
$(TARG).o : $(TARG).v
	iverilog $(TARG).v $(INC) -o $(TARG).o

clean : 
	rm -f *.vvp *.vcd *.o

