IVERILOG ?= iverilog
VVP ?= vvp

SIM_OUT := build/tb_ad1856_formatter.vvp

.PHONY: sim clean

sim: $(SIM_OUT)
	$(VVP) $(SIM_OUT)

$(SIM_OUT): ad1856_formatter.v tb_ad1856_formatter.v
	mkdir -p build
	$(IVERILOG) -g2012 -Wall -o $@ tb_ad1856_formatter.v ad1856_formatter.v

clean:
	rm -rf build
