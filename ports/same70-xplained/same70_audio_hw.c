#include "same70_audio_hw.h"

void same70_audio_hw_get_status(same70_audio_hw_status_t *status)
{
	status->usb_path = 1u;
	status->external_codec = 0u;
	status->i2sc_configured = 0u;
	status->current_path = "usb_loopback_or_generated";
	status->adc_path = "no_adc";
	status->dac_path = "no_dac";
	status->candidate_bus = "header_digital_audio_ssc_style";
	status->candidate_pins = "TD=PD26/J502.1 RD=PA10/J504.2 RF=PD24/J504.1 RK=PA22/J504.3 TK=PB1/J505.8 TF=PB0/J505.7 PCK0=PB13/J504.5";
	status->candidate_headers = "J502,J504,J505,J507,EXT1";
	status->clock_plan = "PCK0_MCLK_or_codec_master_tbd";
	status->sample_frame = "48k_24in32_stereo";
	status->dma_plan = "XDMAC_double_buffer_tbd";
	status->voltage_plan = "3v3_only_no_5v_logic";
	status->wiring_doc = "ports/same70-xplained/external-codec-pin-map.md";
	status->required_signals = "mclk,bclk,lrck,adc_sdata,dac_sdata,codec_reset,codec_control";
	status->original_codec = "AK5394A_CS4344";
	status->original_transport = "AVR32_SSC_PDCA";
	status->boundary = "needs_external_codec_board";
	status->next_step = "confirm_pin_map_then_wire_external_codec_before_enable";
}
