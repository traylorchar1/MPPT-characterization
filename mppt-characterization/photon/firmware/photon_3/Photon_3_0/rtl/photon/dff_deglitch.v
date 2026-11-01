
// 75-100 ns deglitch filter

module dff_deglitch (
  clk,
  rst,
  in,
  out
);

//-------------Input Ports-----------------------------
input   clk, rst, in;
 //-------------Output Ports----------------------------
output  out;
//-------------Input ports Data Type-------------------
wire    clk, in;
//-------------Output Ports Data Type------------------
reg     out;
reg     mid0, mid1, mid2;


always @(posedge clk) begin
  if (rst == 1'b1) begin
    mid0 <= 1'b0;
    mid1 <= 1'b0;
    mid2 <= 1'b0;
    out <= 1'b0;
  end
  else begin
    mid0 <= in;
    mid1 <= mid0;
    mid2 <= mid1;
    if (in == mid0  & mid0 == mid1 & mid1 == mid2) begin
      out <= in;
    end
  end
end
endmodule
