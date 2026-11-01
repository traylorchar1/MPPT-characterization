`timescale 1ns/1ns 

module dff_deglitch_tb;

  /* Make a reset that pulses once. */
  reg rst = 1;
  reg in = 0;

  initial begin
     $dumpfile("dff_deglitch_tb.vcd");
     $dumpvars(0,dff_deglitch_tb);

     # 170 rst = 0;
     # 110 rst = 1;
     # 290 rst = 0;
     
     # 100 in = 1;
     # 10 in = 0;
     # 100 in = 1;
     # 10 in = 0;
     # 100 in = 1;
     # 10 in = 0;
     
     # 50 in = 1;
     # 50 in = 0;
     # 50 in = 1;
     # 50 in = 0;
     
     # 100 in = 1;
     # 100 in = 0;
     # 100 in = 1;
     # 100 in = 0;
     
     # 200 in = 1;
     # 200 in = 0;
     # 200 in = 1;
     # 200 in = 0;
     
     # 2000000 rst = 1;
     # 500 $finish;
  end

  /* Make a regular pulsing clock. */
  reg clk = 0;
  always #12 clk = !clk;

  wire out;

  dff_deglitch d0 (clk, rst, in, out);

  //initial
     //$monitor("At time %t, value = %h (%0d)", $time, value, value);
endmodule // top_tb
