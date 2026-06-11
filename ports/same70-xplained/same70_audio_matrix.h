#ifndef SAME70_XPLAINED_AUDIO_MATRIX_H
#define SAME70_XPLAINED_AUDIO_MATRIX_H

#include "types.h"

#define SAME70_AUDIO_MATRIX_RATE_COUNT 13u
#define SAME70_AUDIO_MATRIX_CURRENT_DESCRIPTOR_COUNT 2u
#define SAME70_AUDIO_MATRIX_TARGET_BITS_PER_RATE 4u
#define SAME70_AUDIO_MATRIX_TARGET_FORMAT_COUNT \
	(SAME70_AUDIO_MATRIX_RATE_COUNT * SAME70_AUDIO_MATRIX_TARGET_BITS_PER_RATE)

typedef struct {
	uint32_t sample_rate_hz;
	uint32_t mclk_hz;
	uint32_t bclk_hz;
	uint32_t feedback_hs_16_16;
	uint8_t mode1;
	const char *rate_label;
	const char *family;
	const char *speed;
	const char *clock_source;
	const char *current_descriptor;
} same70_audio_matrix_rate_t;

typedef struct {
	uint32_t external_codec;
	uint32_t descriptor_enabled;
	uint32_t current_descriptor_count;
	uint32_t target_rate_count;
	uint32_t target_bits_per_rate;
	uint32_t target_format_count;
	const char *current_descriptors;
	const char *target_bits;
	const char *target_rates;
	const char *descriptor_policy;
	const char *transport_policy;
	const char *feedback_policy;
	const char *next_step;
} same70_audio_matrix_status_t;

void same70_audio_matrix_get_status(same70_audio_matrix_status_t *status);
const same70_audio_matrix_rate_t *same70_audio_matrix_rates(void);

#endif
