#ifndef SAME70_XPLAINED_USB_H
#define SAME70_XPLAINED_USB_H

#include "types.h"

#define SAME70_USB_AUDIO_SOURCE_LOOPBACK 0u
#define SAME70_USB_AUDIO_SOURCE_PATTERN  1u
#define SAME70_USB_AUDIO_SOURCE_SILENCE  2u
#define SAME70_USB_AUDIO_SOURCE_TONE     3u
#define SAME70_USB_AUDIO_SOURCE_SINE     4u
#define SAME70_USB_AUDIO_FORMAT_48K24    0u
#define SAME70_USB_AUDIO_FORMAT_44K16    1u

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
	uint32_t ep3cfg;
	uint32_t ep3isr;
	uint32_t ep4cfg;
	uint32_t ep4isr;
	uint32_t ep5cfg;
	uint32_t ep5isr;
	uint32_t reset_count;
	uint32_t setup_count;
	uint32_t tx_count;
	uint32_t rxout_count;
	uint32_t stall_count;
	uint32_t audio_config_count;
	uint32_t audio_cfgok_mask;
	uint32_t audio_out_count;
	uint32_t audio_out_bytes;
	uint32_t audio_out_last_bytes;
	uint32_t audio_out_max_bytes;
	uint32_t audio_feedback_count;
	uint32_t audio_feedback_bytes;
	uint32_t audio_feedback_busy_last;
	uint32_t audio_feedback_busy_max;
	uint32_t audio_in_count;
	uint32_t audio_in_bytes;
	uint32_t audio_in_busy_last;
	uint32_t audio_in_busy_max;
	uint32_t audio_error_count;
	uint32_t audio_short_count;
	uint32_t audio_crc_count;
	uint32_t audio_overflow_count;
	uint32_t audio_underflow_count;
	uint32_t audio_source_mode;
	uint32_t audio_out_format;
	uint32_t audio_in_format;
	uint32_t audio_loopback_level;
	uint32_t audio_loopback_peak;
	uint32_t audio_loopback_drop_bytes;
	uint32_t audio_loopback_silence_bytes;
	uint32_t descriptor_count;
	uint32_t set_address_count;
	uint32_t set_configuration_count;
	uint32_t set_interface_count;
	uint32_t audio_set_interface_count;
	uint32_t interface_alternate_mask;
	uint32_t interface_alternate_peak_mask;
	uint32_t last_set_interface_index;
	uint32_t last_set_interface_value;
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
uint32_t same70_usb_set_audio_source(uint32_t source);

#endif
