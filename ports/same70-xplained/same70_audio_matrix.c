#include "same70_audio_matrix.h"
#include "same70_cs4272_control.h"

static const same70_audio_matrix_rate_t rate_matrix[SAME70_AUDIO_MATRIX_RATE_COUNT] = {
	{ 8000u, 6144000u, 512000u, 0x00010000u, 0x39u, "8", "48k", "single", "family_programmable", "none" },
	{ 11025u, 8467200u, 705600u, 0x000160ccu, 0x39u, "11k025", "44k1", "single", "family_programmable", "none" },
	{ 12000u, 9216000u, 768000u, 0x00018000u, 0x39u, "12", "48k", "single", "family_programmable", "none" },
	{ 16000u, 12288000u, 1024000u, 0x00020000u, 0x39u, "16", "48k", "single", "fixed_family_xo", "none" },
	{ 22050u, 11289600u, 1411200u, 0x0002c199u, 0x29u, "22k05", "44k1", "single", "fixed_family_xo", "none" },
	{ 24000u, 12288000u, 1536000u, 0x00030000u, 0x29u, "24", "48k", "single", "fixed_family_xo", "none" },
	{ 32000u, 12288000u, 2048000u, 0x00040000u, 0x19u, "32", "48k", "single", "fixed_family_xo", "none" },
	{ 44100u, 11289600u, 2822400u, 0x00058333u, 0x09u, "44k1", "44k1", "single", "fixed_family_xo", "44k16" },
	{ 48000u, 12288000u, 3072000u, 0x00060000u, 0x09u, "48", "48k", "single", "fixed_family_xo", "48k24" },
	{ 88200u, 11289600u, 5644800u, 0x000b0666u, 0x89u, "88k2", "44k1", "double", "fixed_family_xo", "none" },
	{ 96000u, 12288000u, 6144000u, 0x000c0000u, 0x89u, "96", "48k", "double", "fixed_family_xo", "none" },
	{ 176400u, 22579200u, 11289600u, 0x00160cccu, 0xe9u, "176k4", "44k1", "quad", "fixed_family_xo", "none" },
	{ 192000u, 24576000u, 12288000u, 0x00180000u, 0xe9u, "192", "48k", "quad", "fixed_family_xo", "none" },
};

void same70_audio_matrix_get_status(same70_audio_matrix_status_t *status)
{
	status->external_codec = 0u;
	status->descriptor_enabled = 0u;
	status->current_descriptor_count = SAME70_AUDIO_MATRIX_CURRENT_DESCRIPTOR_COUNT;
	status->target_rate_count = SAME70_AUDIO_MATRIX_RATE_COUNT;
	status->target_bits_per_rate = SAME70_AUDIO_MATRIX_TARGET_BITS_PER_RATE;
	status->target_format_count = SAME70_AUDIO_MATRIX_TARGET_FORMAT_COUNT;
	status->current_descriptors = "current_usb_descriptors=44k16_48k24_only";
	status->target_bits = "target_bits=16_18_20_24";
	status->target_rates = "target_rates=8_11k025_12_16_22k05_24_32_44k1_48_88k2_96_176k4_192";
	status->descriptor_policy = "status_only_no_new_usb_altsettings_until_ssc_xdmac_and_external_gate";
	status->transport_policy = "future_full_duplex_same70_ssc_slave_xdmac_i2s_to_cs4272";
	status->feedback_policy = "explicit_feedback_endpoint_4_rate_family_selected_from_usb_rate";
	status->next_step = "descriptor_generator_and_host_verifiers_before_advertising_new_modes";
}

const same70_audio_matrix_rate_t *same70_audio_matrix_rates(void)
{
	return rate_matrix;
}
