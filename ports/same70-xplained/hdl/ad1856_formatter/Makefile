OSS_CAD_SUITE ?= /Users/cozy/cozycactus/oss-cad-suite
OSS_BIN := $(OSS_CAD_SUITE)/bin

IVERILOG ?= $(shell if [ -x "$(OSS_BIN)/iverilog" ]; then printf '%s\n' "$(OSS_BIN)/iverilog"; else command -v iverilog 2>/dev/null || printf '%s\n' iverilog; fi)
VVP ?= $(shell if [ -x "$(OSS_BIN)/vvp" ]; then printf '%s\n' "$(OSS_BIN)/vvp"; else command -v vvp 2>/dev/null || printf '%s\n' vvp; fi)
YOSYS ?= $(shell if [ -x "$(OSS_BIN)/yosys" ]; then printf '%s\n' "$(OSS_BIN)/yosys"; else command -v yosys 2>/dev/null || printf '%s\n' yosys; fi)

SIM_OUT := build/tb_ad1856_formatter.vvp
SYNTH_JSON := build/ad1856_formatter-ice40.json
SYNTH_RPT := build/ad1856_formatter-ice40.rpt

.PHONY: sim synth-ice40 tool-versions clean

sim: $(SIM_OUT)
	$(VVP) $(SIM_OUT)

synth-ice40: $(SYNTH_JSON)
	@awk '/^=== ad1856_formatter ===/{show=1} /^=== design hierarchy ===/{if (show) exit} show && /SB_CARRY|SB_DFF|SB_DFFE|SB_LUT4/ {print}' $(SYNTH_RPT)

tool-versions:
	$(IVERILOG) -V 2>&1 | sed -n '1p'
	$(VVP) -V 2>&1 | sed -n '1p'
	$(YOSYS) --version

$(SIM_OUT): ad1856_formatter.v tb_ad1856_formatter.v
	mkdir -p build
	$(IVERILOG) -g2012 -Wall -o $@ tb_ad1856_formatter.v ad1856_formatter.v

$(SYNTH_JSON): ad1856_formatter.v
	mkdir -p build
	$(YOSYS) -p "read_verilog ad1856_formatter.v; synth_ice40 -top ad1856_formatter -json $(SYNTH_JSON); stat" > $(SYNTH_RPT)
	test -s $(SYNTH_JSON)
	grep -q "SB_LUT4" $(SYNTH_RPT)

clean:
	rm -rf build
