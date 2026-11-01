
module dff (
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
reg     out, mid;


always @(posedge clk) begin
  if (rst == 1'b1) begin
    mid <= 1'b0;
    out <= 1'b0;
  end
  else begin
    mid <= in;
    out <= mid;
  end
end
endmodule
