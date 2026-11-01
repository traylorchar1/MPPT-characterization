module top(
  sysclk,
  rst,          // active high
  dsp_gpio_0,   // reset mppt_0 
  dsp_gpio_1,   // reset mppt_1
  dsp_gpio_2,   // relay precharge 
  dsp_gpio_3,   // relay main
  ov_cmp,       // active low
  v_cmp_0,  v_cmp_1,
  i_cmp_0,  i_cmp_1,
  led_dbg_0, led_dbg_1, led_dbg_2, led_dbg_3,
  debug_gpio_0, debug_gpio_1,
  gate_0, gate_1,
  enable_gates,
  relay_precharge, relay_main,
  fpga_fault
);

// Inputs
input   sysclk, rst, ov_cmp;
input   dsp_gpio_0, dsp_gpio_1;
input   dsp_gpio_2, dsp_gpio_3;
input   v_cmp_0, v_cmp_1;
input   i_cmp_0, i_cmp_1;

// Outputs
output  gate_0, gate_1, fpga_fault;
output enable_gates;
output led_dbg_0, led_dbg_1, led_dbg_2, led_dbg_3;
output debug_gpio_0, debug_gpio_1;
output relay_precharge, relay_main;

// Interconnection wires
wire        sysclk, clk, rst, clk_core, pll_lock;
wire        v_cmp_0, v_cmp_1;
wire        i_cmp_0, i_cmp_1;
wire        fault_0, fault_1;
reg        rst_mppt_0, rst_mppt_1;
wire [2:0]  state_0, state_1;

reg [31:0] counter;
reg led_state;

// Output registers
reg led_dbg_0, led_dbg_1, led_dbg_2, led_dbg_3;
reg enable_gates;
reg relay_precharge, relay_main;
reg fpga_fault;
reg debug_gpio_0, debug_gpio_1;

// register dsp_gpio inputs
dff_deglitch d0 (clk, rst, dsp_gpio_0, dsp_gpio_0_d);
dff_deglitch d1 (clk, rst, dsp_gpio_1, dsp_gpio_1_d);
dff_deglitch d2 (clk, rst, dsp_gpio_2, dsp_gpio_2_d);
dff_deglitch d3 (clk, rst, dsp_gpio_3, dsp_gpio_3_d);

// 40MHz PLL
top_pll pll_40 (sysclk, clk_core, clk, !rst, pll_lock);

// Set up the MPPT modules
mppt_fsm mppt_0 (clk, rst_mppt_0, !ov_cmp, v_cmp_0, i_cmp_0,
                 gate_0, fault_0, state_0);
mppt_fsm mppt_1 (clk, rst_mppt_1, !ov_cmp, v_cmp_1, i_cmp_1,
                 gate_1, fault_1, state_1);


always @ (posedge clk) begin
  if(rst == 1'b1) begin
    counter <= 0;
    led_state <= 1'b0;
    end
  else begin
    counter <= counter + 1;
    led_state <= counter[24];
  end
end


// Output logic
always @ (posedge clk) begin
  // The reset signals are active high
  rst_mppt_0 <= rst | dsp_gpio_0_d | !pll_lock;
  rst_mppt_1 <= rst | dsp_gpio_1_d | !pll_lock;

  enable_gates <= pll_lock & ov_cmp;

  // Set the LED outputs
  led_dbg_0 <= i_cmp_1 | !pll_lock;
  led_dbg_1 <= v_cmp_1 | !pll_lock;
  led_dbg_2 <= !ov_cmp | !pll_lock;
  led_dbg_3 <= led_state | !pll_lock;

  // Pass thru relay controls
  relay_precharge <= dsp_gpio_2;
  relay_main <= dsp_gpio_3;

  // OR together the faults into a single pin
  fpga_fault <= fault_0 | fault_1 | !pll_lock;

  // Set the debug signals
  debug_gpio_0 <= i_cmp_0;
  debug_gpio_1 <= v_cmp_0;



end


endmodule // top
