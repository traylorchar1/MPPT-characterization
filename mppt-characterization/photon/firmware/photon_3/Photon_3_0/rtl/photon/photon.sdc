# ##############################################################################

# iCEcube SDC

# Version:            2017.01.27914

# File Generated:     Apr 4 2017 20:36:51

# ##############################################################################

####---- CreateClock list ----2
create_clock  -period 62.50 -name {sysclk} [get_ports {sysclk}] 
create_clock  -period 25.00 -name {clk} [get_nets {clk}] 

