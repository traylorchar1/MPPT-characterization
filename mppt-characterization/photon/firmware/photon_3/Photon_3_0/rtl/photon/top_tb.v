`timescale 1ns/1ns 

module top_tb;

  /* Make a reset that pulses once. */
  reg rst = 1;
  reg ov_cmp = 1;   // active low
  reg dsp_gpio_0 = 0;
  reg dsp_gpio_1 = 0;
  reg dsp_gpio_2 = 0;
  reg dsp_gpio_3 = 0;
  reg volt_cmp_0 = 0;
  reg volt_cmp_1 = 0;
  reg curr_cmp_0 = 0;
  reg curr_cmp_1 = 0;

  initial begin
     $dumpfile("top_tb.vcd");
     $dumpvars(0,top_tb);

     # 170 rst = 0;
     # 110 rst = 1;
     # 290 rst = 0;
     # 50 dsp_gpio_0 = 1;
     # 50 dsp_gpio_1 = 1;
     # 50 dsp_gpio_0 = 0;
     # 50 dsp_gpio_1 = 0;

     // turn on relays
     # 50 dsp_gpio_2 = 1;
     # 50 dsp_gpio_3 = 1;

     // glitch on current
     # 101 curr_cmp_0 = 1;
     # 21 curr_cmp_0 = 0;
    
     // actual current comparator
     # 1001 curr_cmp_0 = 1;
     # 101  curr_cmp_0 = 0;

     // voltage comparator hits
     # 1001 volt_cmp_0 = 1;
     # 101 volt_cmp_0 = 0;

     // on 
     # 1500 curr_cmp_0 = 1;
     # 101 curr_cmp_0 = 0;

     # 101 volt_cmp_0 = 1;
     # 101 volt_cmp_0 = 0;
     
     // off
     # 1000 curr_cmp_0 = 1;
     # 101 curr_cmp_0 = 0;

     # 1500 volt_cmp_0 = 1;
     # 101 volt_cmp_0 = 0;


     // Overvoltage
     # 5000 ov_cmp = 0;
     # 500 ov_cmp = 1;
     # 500 rst = 1;
     # 100 rst = 0;
     # 50 dsp_gpio_0 = 1;
     # 50 dsp_gpio_1 = 1;
     # 50 dsp_gpio_0 = 0;
     # 50 dsp_gpio_1 = 0;

     // on too long

     # 2000000 rst = 1;
     # 500 $finish;
  end

  /* Make a regular pulsing clock. */
  reg sysclk = 0;
  always #31 sysclk = !sysclk;

  wire gate_0, gate_1, fpga_fault;
  wire led_dbg_0, led_dbg_1, led_dbg_2, led_dbg_3;
  wire debug_gpio_0, debug_gpio_1;
  wire enable_gates;
  wire relay_main, relay_precharge;

  top top_inst (sysclk, rst,
                dsp_gpio_0, dsp_gpio_1,
                dsp_gpio_2, dsp_gpio_3,
                ov_cmp,
                volt_cmp_0, volt_cmp_1,
                curr_cmp_0, curr_cmp_1,
                led_dbg_0, led_dbg_1, led_dbg_2, led_dbg_3,
                debug_gpio_0, debug_gpio_1,
                gate_0, gate_1,
                enable_gates,
                relay_precharge, relay_main,
                fpga_fault);

  //initial
     //$monitor("At time %t, value = %h (%0d)", $time, value, value);
endmodule // top_tb
