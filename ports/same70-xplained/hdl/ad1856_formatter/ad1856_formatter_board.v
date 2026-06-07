`timescale 1ns / 1ps

// Board bitstream wrapper for the AD1856 formatter.
//
// The core module exposes debug outputs for simulation and bring-up. This
// wrapper keeps the board-level pinout to only the signals needed for the
// first AD1856 mono formatter bitstream.
module ad1856_formatter_board #(
	parameter integer INPUT_FIRST_BIT = 0
) (
	input wire clk_xo,
	input wire reset_n,

	input wire same_td,
	output wire same_tk,
	output wire same_tf,

	output wire ad1856_data,
	output wire ad1856_clk,
	output wire ad1856_le
);
	wire [5:0] same_bit_phase_dbg;
	wire sample_valid_dbg;
	wire overrun_dbg;

	ad1856_formatter #(
		.INPUT_FIRST_BIT(INPUT_FIRST_BIT)
	) formatter (
		.clk_xo(clk_xo),
		.reset_n(reset_n),
		.same_td(same_td),
		.same_tk(same_tk),
		.same_tf(same_tf),
		.ad1856_data(ad1856_data),
		.ad1856_clk(ad1856_clk),
		.ad1856_le(ad1856_le),
		.same_bit_phase_dbg(same_bit_phase_dbg),
		.sample_valid_dbg(sample_valid_dbg),
		.overrun_dbg(overrun_dbg)
	);
endmodule
