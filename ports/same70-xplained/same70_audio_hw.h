#ifndef SAME70_XPLAINED_AUDIO_HW_H
#define SAME70_XPLAINED_AUDIO_HW_H

#include "types.h"

typedef struct {
	uint32_t usb_path;
	uint32_t external_codec;
	uint32_t i2sc_configured;
	const char *current_path;
	const char *adc_path;
	const char *dac_path;
	const char *required_signals;
	const char *original_codec;
	const char *original_transport;
	const char *boundary;
} same70_audio_hw_status_t;

void same70_audio_hw_get_status(same70_audio_hw_status_t *status);

#endif
