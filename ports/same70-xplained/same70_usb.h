#ifndef SAME70_XPLAINED_USB_H
#define SAME70_XPLAINED_USB_H

#include "types.h"

typedef struct {
	uint32_t initialized;
	uint32_t attached;
	uint32_t pmc_sr;
	uint32_t pmc_usb;
	uint32_t pmc_pcsr1;
	uint32_t usbhs_ctrl;
	uint32_t usbhs_sr;
	uint32_t devctrl;
	uint32_t devisr;
	uint32_t devept;
	uint32_t ep0cfg;
	uint32_t ep0isr;
	uint32_t ep0imr;
	uint32_t reset_count;
	uint32_t setup_count;
	uint32_t tx_count;
	uint32_t rxout_count;
	uint32_t stall_count;
	uint32_t descriptor_count;
	uint32_t set_address_count;
	uint32_t set_configuration_count;
	uint32_t address;
	uint32_t configuration;
	uint32_t ep0_state;
	uint32_t last_setup0;
	uint32_t last_wvalue;
	uint32_t last_windex;
	uint32_t last_wlength;
} same70_usb_status_t;

void same70_usb_init(void);
void same70_usb_attach(void);
void same70_usb_detach(void);
void same70_usb_poll(void);
void same70_usb_get_status(same70_usb_status_t *status);

#endif
