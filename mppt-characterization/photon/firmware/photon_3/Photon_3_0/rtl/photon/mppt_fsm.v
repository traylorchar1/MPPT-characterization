
module mppt_fsm (
  clk,        // clock
  rst,        // active high
  ov_cmp,     // overvoltage comparator
  volt_cmp,   // voltage comparator
  curr_cmp,   // current comparator
  gate_out,   // gate output
  fault_out,  // fault output
  state_out
);

//-------------Internal Constants--------------------------
parameter SZ = 3;
parameter IDLE= 0, GATE_ON= 1, GATE_OFF=2, FAULT=3;
parameter MIN_ON = 80;      // cycles - 2us @ 40MHz
parameter MIN_OFF = 80;     // cycles - 2us @ 40MHz
parameter MAX_ON = 6000;    // cycles - 150us @ 40MHz
parameter MAX_OFF = 40000;  // cycles - 1ms @ 40MHz
//-------------Input Ports-----------------------------
input   clk, rst, ov_cmp, volt_cmp, curr_cmp;
 //-------------Output Ports----------------------------
output  gate_out, fault_out;
output [SZ-1:0]   state_out;
//-------------Input ports Data Type-------------------
wire    clk, rst, ov_cmp, volt_cmp, curr_cmp;
//-------------Output Ports Data Type------------------
reg                     gate_out, fault_out;
reg   [SZ-1:0]          state_out;
//-------------Internal Variables---------------------------
reg   [SZ-1:0]          state;          // Seq part of the FSM
reg   [SZ-1:0]          next_state;     // combo part of FSM
reg   [15:0]            on_time_count;  // counter for minimum on time
reg   [15:0]            off_time_count; // counter for minimum off time
reg                     on_deglitch;    // Turn on deglitching
reg                     off_deglitch;   // Turn off deglitching
reg                     on_too_long;    // Stuck on
reg                     off_too_long;   // Stuck off
//----------Code startes Here------------------------

// Registering inputs
dff_deglitch d0 (clk, rst, ov_cmp, ov_cmp_d);
dff_deglitch d1 (clk, rst, volt_cmp, volt_cmp_d);
dff_deglitch d2 (clk, rst, curr_cmp, curr_cmp_d);


always @*
  begin : FSM_COMBO
    next_state = IDLE;
    case(state)
      IDLE: 
        if (curr_cmp_d == 1'b0 && ov_cmp_d == 1'b0 && rst == 1'b0) begin
          next_state = GATE_ON;
        end else begin
          next_state = IDLE;
        end
      GATE_ON: 
        if (curr_cmp_d == 1'b1 && on_deglitch) begin
          next_state = GATE_OFF;
        end else begin
          if (on_too_long) begin
            next_state = GATE_OFF;
          end else begin
            next_state = GATE_ON;
          end
        end
      GATE_OFF: 
        if (volt_cmp_d == 1'b0 && curr_cmp_d == 1'b0 && off_deglitch) begin
          next_state = GATE_ON;
        end else begin
          if (off_too_long) begin
            next_state = IDLE;
          end else begin
            next_state = GATE_OFF;
          end
        end
      FAULT:
        if (rst == 1'b1) begin
          next_state = IDLE;
        end else begin
          next_state = FAULT;
        end
      default:
        next_state = FAULT;
  endcase
end
//----------Seq Logic-----------------------------
always @ (posedge clk)
  begin : FSM_SEQ
    if (rst == 1'b1)  begin
      state <= IDLE;
      on_time_count <= 0;
      off_time_count <= 0;
      on_deglitch <= 0;
      off_deglitch <= 0;
      on_too_long <= 0;
      off_too_long <= 0;
    end else begin
      if (ov_cmp_d == 1'b1)  begin
        state <= FAULT;
      end else begin
        state <= next_state;
      end

      if (state == GATE_ON) begin
        on_time_count <= on_time_count + 1;
      end else begin
        on_time_count <= 0;
      end

      if (state == GATE_OFF) begin
        off_time_count <= off_time_count + 1;
      end else begin
        off_time_count <= 0;
      end
      
      if (on_time_count >= MIN_ON)    on_deglitch <= 1'b1;
      else                            on_deglitch <= 1'b0;
      
      if (on_time_count >= MAX_ON)    on_too_long <= 1'b1;
      else                            on_too_long <= 1'b0;
      
      if (off_time_count >= MIN_OFF)  off_deglitch <= 1'b1;
      else                            off_deglitch <= 1'b0;

      if (off_time_count >= MAX_OFF)  off_too_long <= 1'b1;
      else                            off_too_long <= 1'b0;

    end
end
//----------Output Logic-----------------------------
always @ (posedge clk)
begin : OUTPUT_LOGIC
if (rst == 1'b1) begin
  gate_out <= 1'b0;
  fault_out <= 1'b0;
  state_out <= state;
end
else begin
  state_out <= state;
  case(state)
    IDLE: 
      begin
        gate_out <= 1'b0;
        fault_out <= 1'b0;
      end
    GATE_ON:
      begin
        gate_out <= 1'b1;
        fault_out <= 1'b0;
      end
    GATE_OFF:
      begin
        gate_out <= 1'b0;
        fault_out <= 1'b0;
      end
    FAULT:
      begin
        gate_out <= 1'b0;
        fault_out <= 1'b1;
      end
    default :
      begin
        gate_out <= 1'b0;
        fault_out <= 1'b0;
      end
  endcase
end
end // End Of Block OUTPUT_LOGIC

endmodule // End of Module arbiter
