`timescale 1ns / 1ps

// Low-jitter mono formatter for AD1856.
//
// The formatter owns AD1856 DATA/CLK/LE timing. SAME70 is a slave-TX source:
//   - same_tk is the external serial clock into SAME70 TK.
//   - same_tf is the external frame signal into SAME70 TF.
//   - same_td is sampled from SAME70 TD.
//
// Default input format is left-justified stereo, 32 bits per channel:
//   same_tf=0: left slot, first 16 bits are the mono DAC sample.
//   same_tf=1: right slot, ignored for the first mono proof.
module ad1856_formatter #(
	parameter integer INPUT_FIRST_BIT = 0
) (
	input wire clk_xo,
	input wire reset_n,

	input wire same_td,
	output reg same_tk,
	output reg same_tf,

	output reg ad1856_data,
	output reg ad1856_clk,
	output reg ad1856_le,

	output reg [5:0] same_bit_phase_dbg,
	output reg sample_valid_dbg,
	output reg overrun_dbg
);
	localparam integer INPUT_LAST_BIT = INPUT_FIRST_BIT + 15;

	localparam [1:0] AD_IDLE = 2'd0;
	localparam [1:0] AD_CLK_HIGH = 2'd1;
	localparam [1:0] AD_CLK_LOW = 2'd2;
	localparam [1:0] AD_LE_LOW = 2'd3;

	reg [1:0] div4;
	reg [5:0] bit_phase;
	reg [15:0] capture_shift;
	reg [15:0] sample_latch;
	reg sample_valid;

	reg [1:0] ad_state;
	reg [15:0] ad_shift;
	reg [4:0] ad_bits_left;
	reg [1:0] le_low_cycles;

	wire [1:0] div4_next = div4 + 2'd1;
	wire bclk_rise = (div4 == 2'd1);
	wire bclk_fall = (div4 == 2'd3);
	wire left_sample_bit =
		(bit_phase >= INPUT_FIRST_BIT[5:0]) &&
		(bit_phase <= INPUT_LAST_BIT[5:0]) &&
		(bit_phase < 6'd32);
	wire left_sample_last_bit = (bit_phase == INPUT_LAST_BIT[5:0]);
	wire frame_wrap = bclk_fall && (bit_phase == 6'd63);
	wire [5:0] next_bit_phase = frame_wrap ? 6'd0 : (bit_phase + 6'd1);

	always @(posedge clk_xo or negedge reset_n) begin
		if (!reset_n) begin
			div4 <= 2'd0;
			bit_phase <= 6'd0;
			capture_shift <= 16'd0;
			sample_latch <= 16'd0;
			sample_valid <= 1'b0;

			same_tk <= 1'b0;
			same_tf <= 1'b0;

			ad1856_data <= 1'b0;
			ad1856_clk <= 1'b0;
			ad1856_le <= 1'b1;
			ad_state <= AD_IDLE;
			ad_shift <= 16'd0;
			ad_bits_left <= 5'd0;
			le_low_cycles <= 2'd0;

			same_bit_phase_dbg <= 6'd0;
			sample_valid_dbg <= 1'b0;
			overrun_dbg <= 1'b0;
		end else begin
			div4 <= div4_next;
			same_tk <= div4_next[1];

			if (bclk_fall) begin
				bit_phase <= next_bit_phase;
				same_tf <= next_bit_phase[5];
			end

			if (bclk_rise && left_sample_bit) begin
				capture_shift <= {capture_shift[14:0], same_td};
				if (left_sample_last_bit) begin
					sample_latch <= {capture_shift[14:0], same_td};
					sample_valid <= 1'b1;
				end
			end

			if (frame_wrap && (ad_state != AD_IDLE)) begin
				overrun_dbg <= 1'b1;
			end

			case (ad_state)
			AD_IDLE: begin
				ad1856_clk <= 1'b0;
				ad1856_le <= 1'b1;
				if (frame_wrap) begin
					ad_shift <= sample_valid ? sample_latch : 16'd0;
					ad1856_data <= sample_valid ? sample_latch[15] : 1'b0;
					ad_bits_left <= 5'd16;
					ad_state <= AD_CLK_HIGH;
				end
			end

			AD_CLK_HIGH: begin
				ad1856_clk <= 1'b1;
				ad_state <= AD_CLK_LOW;
			end

			AD_CLK_LOW: begin
				ad1856_clk <= 1'b0;
				if (ad_bits_left == 5'd1) begin
					ad_bits_left <= 5'd0;
					ad1856_le <= 1'b0;
					le_low_cycles <= 2'd2;
					ad_state <= AD_LE_LOW;
				end else begin
					ad_bits_left <= ad_bits_left - 5'd1;
					ad_shift <= {ad_shift[14:0], 1'b0};
					ad1856_data <= ad_shift[14];
					ad_state <= AD_CLK_HIGH;
				end
			end

			AD_LE_LOW: begin
				ad1856_clk <= 1'b0;
				if (le_low_cycles == 2'd1) begin
					ad1856_le <= 1'b1;
					le_low_cycles <= 2'd0;
					ad_state <= AD_IDLE;
				end else begin
					le_low_cycles <= le_low_cycles - 2'd1;
				end
			end
			endcase

			same_bit_phase_dbg <= bit_phase;
			sample_valid_dbg <= sample_valid;
		end
	end
endmodule
