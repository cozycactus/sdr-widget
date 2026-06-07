#include "same70_audio_hw.h"

void same70_audio_hw_get_status(same70_audio_hw_status_t *status)
{
	status->usb_path = 1u;
	status->external_codec = 0u;
	status->i2sc_configured = 0u;
	status->current_path = "usb_loopback_or_generated";
	status->adc_path = "no_adc";
	status->dac_path = "no_dac";
	status->required_signals = "mclk,bclk,lrck,adc_sdata,dac_sdata,codec_reset,codec_control";
	status->original_codec = "AK5394A_CS4344";
	status->original_transport = "AVR32_SSC_PDCA";
	status->boundary = "needs_external_codec_board";
}
