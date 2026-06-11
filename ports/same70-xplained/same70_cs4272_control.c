#include "same70_cs4272_control.h"

static const same70_cs4272_register_write_t baseline_writes[SAME70_CS4272_BASELINE_WRITE_COUNT] = {
	{ 0x07u, 0x03u, 0u },
	{ 0x01u, 0x09u, 1u },
	{ 0x02u, 0x80u, 0u },
	{ 0x03u, 0x50u, 0u },
	{ 0x04u, 0x00u, 0u },
	{ 0x05u, 0x00u, 0u },
	{ 0x06u, 0x10u, 0u },
	{ 0x07u, 0x02u, 0u },
};

void same70_cs4272_control_get_status(same70_cs4272_control_status_t *status)
{
	status->control_enabled = 0u;
	status->bus_configured = 0u;
	status->reset_configured = 0u;
	status->external_codec = 0u;
	status->address_7bit = SAME70_CS4272_I2C_ADDRESS_7BIT;
	status->twi_hz = SAME70_CS4272_TWI_HZ;
	status->baseline_write_count = SAME70_CS4272_BASELINE_WRITE_COUNT;
	status->bus_plan = "TWIHS0_status_only SDA=PA3/TWD0 SCL=PA4/TWCK0 pullups=3V3 addr7=0x10";
	status->reset_plan = "RST=PC17/EXT1.10_status_only_hold_low_until_mclk_stable";
	status->power_plan = "VL=3V3_no_5v_logic AD0/CS=strapped_low";
	status->startup_sequence = "write_0x07_0x03_program_registers_write_0x07_0x02_readback_before_external_codec_1";
	status->mode1_table = "mode1=8_11k025_12_16:0x39 22k05_24:0x29 32:0x19 44k1_48:0x09 88k2_96:0x89 176k4_192:0xe9";
	status->policy = "no_usb_mute_volume_to_codec_no_i2c_during_active_audio";
	status->boundary = "status_only_external_codec_0_no_pin_mux_no_twi_writes";
	status->next_step = "wire_safe_probe_then_twihs0_readback_gate";
}

const same70_cs4272_register_write_t *same70_cs4272_control_baseline_writes(void)
{
	return baseline_writes;
}

uint8_t same70_cs4272_control_mode1_for_rate(uint32_t sample_rate_hz)
{
	if ((sample_rate_hz == 176400u) || (sample_rate_hz == 192000u)) {
		return 0xe9u;
	}

	if ((sample_rate_hz == 88200u) || (sample_rate_hz == 96000u)) {
		return 0x89u;
	}

	if ((sample_rate_hz == 44100u) || (sample_rate_hz == 48000u)) {
		return 0x09u;
	}

	if (sample_rate_hz == 32000u) {
		return 0x19u;
	}

	if ((sample_rate_hz == 22050u) || (sample_rate_hz == 24000u)) {
		return 0x29u;
	}

	return 0x39u;
}
