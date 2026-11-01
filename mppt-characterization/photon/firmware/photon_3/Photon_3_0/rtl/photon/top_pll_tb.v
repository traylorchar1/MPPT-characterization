module top_pll(PACKAGEPIN,
               PLLOUTCORE,
               PLLOUTGLOBAL,
               RESET,
               LOCK);

input PACKAGEPIN;
input RESET; 
output PLLOUTCORE;
output PLLOUTGLOBAL;
output LOCK;

wire PACKAGEPIN;
wire RESET;

reg PLLOUTCORE;
reg PLLOUTGLOBAL;
reg LOCK;

always #12
  begin
    PLLOUTCORE <= !PLLOUTCORE & RESET;
    PLLOUTGLOBAL <= !PLLOUTCORE & RESET;
    LOCK = RESET;
  end



endmodule
