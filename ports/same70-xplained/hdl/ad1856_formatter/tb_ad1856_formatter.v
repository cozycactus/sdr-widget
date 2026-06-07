`timescale 1ns / 1ps

module tb_ad1856_formatter;
	reg clk_xo = 1'b0;
	reg reset_n = 1'b0;
	reg same_td = 1'b0;

	wire same_tk;
	wire same_tf;
	wire ad1856_data;
	wire ad1856_clk;
	wire ad1856_le;
	wire [5:0] same_bit_phase_dbg;
	wire sample_valid_dbg;
	wire overrun_dbg;

	reg [15:0] samples [0:7];
	reg [15:0] ad_word;
	reg [1023:0] vcdfile;
	time last_data_change_time;
	time last_ad1856_clk_fall_time;
	integer tx_sample;
	integer tx_phase;
	integer out_sample;
	integer ad_bit_count;
	integer xo_edges;
	integer last_same_tk_rise_xo;
	integer last_same_tf_toggle_xo;
	integer le_low_start_xo;
	integer i;

	ad1856_formatter dut (
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

	always #44 clk_xo = ~clk_xo;

	initial begin
		vcdfile = "build/ad1856_formatter.vcd";
		if ($test$plusargs("vcd")) begin
			if (!$value$plusargs("vcdfile=%s", vcdfile)) begin
				vcdfile = "build/ad1856_formatter.vcd";
			end
			$dumpfile(vcdfile);
			$dumpvars(0, tb_ad1856_formatter);
		end
	end

	initial begin
		samples[0] = 16'h1234;
		samples[1] = 16'ha55a;
		samples[2] = 16'h0000;
		samples[3] = 16'h7fff;
		samples[4] = 16'h8000;
		samples[5] = 16'h55aa;
		samples[6] = 16'h0f0f;
		samples[7] = 16'hf00f;

		tx_sample = 0;
		tx_phase = 0;
		out_sample = 0;
		ad_bit_count = 0;
		ad_word = 16'd0;
		xo_edges = 0;
		last_same_tk_rise_xo = -1;
		last_same_tf_toggle_xo = -1;
		le_low_start_xo = -1;
		last_data_change_time = 0;
		last_ad1856_clk_fall_time = 0;
		same_td = samples[0][15];

		repeat (8) @(posedge clk_xo);
		reset_n = 1'b1;

		for (i = 0; i < 20000; i = i + 1) begin
			@(posedge clk_xo);
		end

		$display("timeout");
		$finish;
	end

	always @(posedge clk_xo) begin
		xo_edges = xo_edges + 1;
		if (reset_n && (ad1856_le === 1'b0) && (ad1856_clk !== 1'b0)) begin
			$display("FAIL: AD1856 CLK high while LE is low at %0t", $time);
			$finish;
		end
	end

	always @(posedge same_tk) begin
		if (reset_n) begin
			if ((last_same_tk_rise_xo >= 0) &&
			    ((xo_edges - last_same_tk_rise_xo) != 4)) begin
				$display("FAIL: same_tk period %0d XO edges, expected 4",
					xo_edges - last_same_tk_rise_xo);
				$finish;
			end
			last_same_tk_rise_xo = xo_edges;
		end
	end

	always @(same_tf) begin
		if (reset_n) begin
			if ((last_same_tf_toggle_xo >= 0) &&
			    ((xo_edges - last_same_tf_toggle_xo) != 128)) begin
				$display("FAIL: same_tf half-frame %0d XO edges, expected 128",
					xo_edges - last_same_tf_toggle_xo);
				$finish;
			end
			last_same_tf_toggle_xo = xo_edges;
		end
	end

	always @(negedge same_tk) begin
		if (reset_n) begin
			#1;
			if (tx_phase == 63) begin
				tx_phase = 0;
				tx_sample = tx_sample + 1;
				if (tx_sample > 7) begin
					tx_sample = 7;
				end
			end else begin
				tx_phase = tx_phase + 1;
			end

			if (tx_phase < 16) begin
				same_td <= samples[tx_sample][15 - tx_phase];
			end else begin
				same_td <= 1'b0;
			end
		end
	end

	always @(ad1856_data) begin
		if (reset_n) begin
			last_data_change_time = $time;
		end
	end

	always @(posedge ad1856_clk) begin
		if (!reset_n) begin
			ad_word = 16'd0;
			ad_bit_count = 0;
		end else begin
			if (ad1856_le !== 1'b1) begin
				$display("FAIL: AD1856 CLK rose while LE was low at %0t", $time);
				$finish;
			end
			if (($time - last_data_change_time) < 80) begin
				$display("FAIL: AD1856 DATA setup %0t ns before CLK, expected >=80 ns",
					$time - last_data_change_time);
				$finish;
			end
			if (ad_bit_count >= 16) begin
				$display("FAIL: extra AD1856 CLK before LE pulse");
				$finish;
			end
			ad_word = {ad_word[14:0], ad1856_data};
			ad_bit_count = ad_bit_count + 1;
		end
	end

	always @(negedge ad1856_clk) begin
		if (reset_n) begin
			last_ad1856_clk_fall_time = $time;
		end
	end

	always @(negedge ad1856_le) begin
		#1;
		if (ad1856_clk !== 1'b0) begin
			$display("FAIL: AD1856 LE fell while CLK was not low");
			$finish;
		end
		if (($time - last_ad1856_clk_fall_time) > 1) begin
			$display("FAIL: AD1856 LE did not follow the final CLK falling edge");
			$finish;
		end
		if (ad_bit_count != 16) begin
			$display("FAIL: AD1856 clock count %0d, expected 16", ad_bit_count);
			$finish;
		end
		if (ad_word !== samples[out_sample]) begin
			$display("FAIL: sample %0d expected %04x got %04x",
				out_sample, samples[out_sample], ad_word);
			$finish;
		end
		$display("PASS sample %0d word=%04x", out_sample, ad_word);
		le_low_start_xo = xo_edges;
		out_sample = out_sample + 1;
		ad_word = 16'd0;
		ad_bit_count = 0;
		if (out_sample == 6) begin
			if (overrun_dbg) begin
				$display("FAIL: formatter overrun");
				$finish;
			end
			$display("ad1856_formatter_tb: pass");
			$finish;
		end
	end

	always @(posedge ad1856_le) begin
		if (reset_n && (le_low_start_xo >= 0)) begin
			if ((xo_edges - le_low_start_xo) != 2) begin
				$display("FAIL: AD1856 LE low width %0d XO edges, expected 2",
					xo_edges - le_low_start_xo);
				$finish;
			end
			le_low_start_xo = -1;
		end
	end
endmodule
