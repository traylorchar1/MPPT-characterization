`timescale 1ns/1ns 

module test;

  /* Make a reset that pulses once. */
  reg reset = 0;
  initial begin
     $dumpfile("test.vcd");
     $dumpvars(0,test);

     # 170 reset = 1;
     # 110 reset = 0;
     # 290 reset = 1;
     # 50  reset =0;
     # 5130 $finish;
  end

  /* Make a regular pulsing clock. */
  reg clk = 0;
  always #10 clk = !clk;

  wire [7:0] value;
  counter c1 (value, clk, reset);

  //initial
     //$monitor("At time %t, value = %h (%0d)", $time, value, value);
endmodule // test
