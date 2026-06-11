##
## make both the widget
## and the widget-control that matches
## the features in the widget
##
## uses avr32-gcc from PATH, or from AVR32BIN when it is set
HOST_CC ?= cc
PKG_CONFIG ?= pkg-config
LIBUSB_CFLAGS ?= $(shell $(PKG_CONFIG) --cflags libusb-1.0 2>/dev/null)
LIBUSB_LIBS ?= $(shell $(PKG_CONFIG) --libs libusb-1.0 2>/dev/null || echo -lusb-1.0)

all:: Release/widget.elf widget-control

Release/widget.elf::
	./make-widget

audio-widget::
	rm -f Release/widget.elf Release/src/features.o
	CFLAGS=-DFEATURE_DEFAULT_BOARD=feature_board_dib ./make-widget

sdr-widget::
	rm -f Release/widget.elf Release/src/features.o
	CFLAGS=-DFEATURE_DEFAULT_BOARD=feature_board_widget ./make-widget

widget-control: widget-control.c src/features.h
	$(HOST_CC) $(LIBUSB_CFLAGS) -o $@ widget-control.c $(LIBUSB_LIBS)

clean::
	cd Release && make clean
	rm -f widget-control
