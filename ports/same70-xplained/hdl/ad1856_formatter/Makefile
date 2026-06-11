OSS_CAD_SUITE ?= /Users/cozy/cozycactus/oss-cad-suite
OSS_BIN := $(OSS_CAD_SUITE)/bin

IVERILOG ?= $(shell if [ -x "$(OSS_BIN)/iverilog" ]; then printf '%s\n' "$(OSS_BIN)/iverilog"; else command -v iverilog 2>/dev/null || printf '%s\n' iverilog; fi)
VVP ?= $(shell if [ -x "$(OSS_BIN)/vvp" ]; then printf '%s\n' "$(OSS_BIN)/vvp"; else command -v vvp 2>/dev/null || printf '%s\n' vvp; fi)
YOSYS ?= $(shell if [ -x "$(OSS_BIN)/yosys" ]; then printf '%s\n' "$(OSS_BIN)/yosys"; else command -v yosys 2>/dev/null || printf '%s\n' yosys; fi)
NEXTPNR_ICE40 ?= $(shell if [ -x "$(OSS_BIN)/nextpnr-ice40" ]; then printf '%s\n' "$(OSS_BIN)/nextpnr-ice40"; else command -v nextpnr-ice40 2>/dev/null || printf '%s\n' nextpnr-ice40; fi)
ICEPACK ?= $(shell if [ -x "$(OSS_BIN)/icepack" ]; then printf '%s\n' "$(OSS_BIN)/icepack"; else command -v icepack 2>/dev/null || printf '%s\n' icepack; fi)

SIM_OUT := build/tb_ad1856_formatter.vvp
VCD ?= build/ad1856_formatter.vcd
SYNTH_JSON := build/ad1856_formatter-ice40.json
SYNTH_RPT := build/ad1856_formatter-ice40.rpt
BOARD_SYNTH_JSON := build/ad1856_formatter-board-ice40.json
BOARD_SYNTH_RPT := build/ad1856_formatter-board-ice40.rpt
ICE40_ASC := build/ad1856_formatter-ice40.asc
ICE40_BIN := build/ad1856_formatter-ice40.bin
BOARD_SRCS := ad1856_formatter.v ad1856_formatter_board.v
ICE40_DEVICE ?=
ICE40_PACKAGE ?=
PCF ?=
NEXTPNR_ARGS ?=
UPDUINO_V31_DEVICE ?= up5k
UPDUINO_V31_PACKAGE ?= sg48
UPDUINO_V31_PCF ?= constraints/upduino-v3.1-ad1856.pcf.example
UPDUINO_V31_NEXTPNR_ARGS ?=

.PHONY: sim sim-vcd waves synth-ice40 synth-ice40-board bitstream-help bitstream-ice40 upduino-v31-bitstream check-bitstream-config tool-versions clean

sim: $(SIM_OUT)
	$(VVP) $(SIM_OUT)

sim-vcd waves: $(SIM_OUT)
	$(VVP) $(SIM_OUT) +vcd +vcdfile=$(VCD)
	test -s $(VCD)

synth-ice40: $(SYNTH_JSON)
	@awk '/^=== ad1856_formatter ===/{show=1} /^=== design hierarchy ===/{if (show) exit} show && /SB_CARRY|SB_DFF|SB_DFFE|SB_LUT4/ {print}' $(SYNTH_RPT)

synth-ice40-board: $(BOARD_SYNTH_JSON)
	@awk '/^=== ad1856_formatter_board ===/{show=1} /^=== design hierarchy ===/{if (show) exit} show && /SB_CARRY|SB_DFF|SB_DFFE|SB_LUT4/ {print}' $(BOARD_SYNTH_RPT)

bitstream-help:
	@printf '%s\n' "Copy constraints/ad1856_formatter-ice40.pcf.example to a board-specific .pcf, then run:"
	@printf '%s\n' "  make bitstream-ice40 ICE40_DEVICE=up5k ICE40_PACKAGE=sg48 PCF=path/to/board.pcf"
	@printf '%s\n' "For the selected UPduino v3.1 starter map, run:"
	@printf '%s\n' "  make upduino-v31-bitstream"
	@printf '%s\n' "Use the exact ICE40_DEVICE/ICE40_PACKAGE values for the selected FPGA board."

bitstream-ice40: check-bitstream-config $(ICE40_BIN)

upduino-v31-bitstream:
	$(MAKE) bitstream-ice40 \
		ICE40_DEVICE=$(UPDUINO_V31_DEVICE) \
		ICE40_PACKAGE=$(UPDUINO_V31_PACKAGE) \
		PCF=$(UPDUINO_V31_PCF) \
		NEXTPNR_ARGS="$(UPDUINO_V31_NEXTPNR_ARGS)"

check-bitstream-config:
	@if [ -z "$(ICE40_DEVICE)" ] || [ -z "$(ICE40_PACKAGE)" ] || [ -z "$(PCF)" ]; then \
		$(MAKE) --no-print-directory bitstream-help; \
		exit 2; \
	fi

tool-versions:
	$(IVERILOG) -V 2>&1 | sed -n '1p'
	$(VVP) -V 2>&1 | sed -n '1p'
	$(YOSYS) --version
	$(NEXTPNR_ICE40) --version
	$(ICEPACK) --help >/dev/null 2>&1 || true

$(SIM_OUT): ad1856_formatter.v tb_ad1856_formatter.v
	mkdir -p build
	$(IVERILOG) -g2012 -Wall -o $@ tb_ad1856_formatter.v ad1856_formatter.v

$(SYNTH_JSON): ad1856_formatter.v
	mkdir -p build
	$(YOSYS) -p "read_verilog ad1856_formatter.v; synth_ice40 -top ad1856_formatter -json $(SYNTH_JSON); stat" > $(SYNTH_RPT)
	test -s $(SYNTH_JSON)
	grep -q "SB_LUT4" $(SYNTH_RPT)

$(BOARD_SYNTH_JSON): $(BOARD_SRCS)
	mkdir -p build
	$(YOSYS) -p "read_verilog $(BOARD_SRCS); synth_ice40 -top ad1856_formatter_board -json $(BOARD_SYNTH_JSON); stat" > $(BOARD_SYNTH_RPT)
	test -s $(BOARD_SYNTH_JSON)
	grep -q "SB_LUT4" $(BOARD_SYNTH_RPT)

$(ICE40_ASC): check-bitstream-config $(BOARD_SYNTH_JSON) $(PCF)
	$(NEXTPNR_ICE40) --$(ICE40_DEVICE) --package $(ICE40_PACKAGE) --json $(BOARD_SYNTH_JSON) --pcf $(PCF) --asc $@ $(NEXTPNR_ARGS)

$(ICE40_BIN): $(ICE40_ASC)
	$(ICEPACK) $< $@
	test -s $@

clean:
	rm -rf build
