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
	status->clock_plan = "UC3A3_AD_MCLK_12M288_DA_MCLK_24M576_direction_tbd";
	status->sample_frame = "48k24_or_44k16_usb_modes";
	status->dma_plan = "XDMAC_double_buffer_tbd";
	status->voltage_plan = "3v3_only_no_5v_logic";
	status->wiring_doc = "ports/same70-xplained/external-codec-original-uc3a3-map.md ports/same70-xplained/external-codec-pin-map.md";
	status->required_signals = "ad_mclk,da_mclk,bclk,lrck,adc_sdata,dac_sdata,codec_reset,adc_control";
	status->original_codec = "UC3A3_AK5394_ES9023";
	status->original_transport = "AVR32_SSC_PDCA";
	status->boundary = "needs_external_codec_board";
	status->next_step = "confirm_pin_map_then_wire_external_codec_before_enable";
}
