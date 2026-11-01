module top (
  sysclk,
  rst,
  led_dbg_0,
  led_dbg_1,
  led_dbg_2,
  led_dbg_3,
  debug_gpio_0,
  debug_gpio_1,
);

input sysclk;
input rst;
output led_dbg_0;
output led_dbg_1;
output led_dbg_2;
output led_dbg_3;
output debug_gpio_0;
output debug_gpio_1;

reg[31:0] cnt = 0;
reg[31:0] factor = 40000000; // divide clock down to 1 Hz

reg divout = 1'b0;

wire sysclk, clk, rst, clk_g, pll_lock;

// 40MHz PLL
top_pll pll_40 (sysclk, clk, clk_g, rst, pll_lock);

// 48MHz HFOSC
//SB_HFOSC OSCInst0 (
//  .CLKHFEN(1'b1),
//  .CLKHFPU(1'b1),
//  .CLKHF(clk)
//) /* synthesis ROUTE_THROUGH_FABRIC= 0 */;
//defparam OSCInst0.CLKHF_DIV = "0b00";


assign led_dbg_0 = divout && rst;
assign led_dbg_1 = ~divout && rst;
assign led_dbg_2 = divout && pll_lock;
assign led_dbg_3 = ~divout;

assign debug_gpio_0 = divout && rst;
assign debug_gpio_1 = ~divout && rst;

always @(posedge clk)
begin
  if (!rst) begin
  end
  else begin
    if(factor != 0) begin
      cnt = cnt + 1;
    end
    if (cnt == factor) begin
      cnt = 0;
      divout = ~divout;
    end
  end
end

endmodule
