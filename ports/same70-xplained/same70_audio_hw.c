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
	status->candidate_pins = "CS4272=I2S_TD_RD_BCLK_LRCK TD=PD26/J502.1 RD=PA10/J504.2 RF=PD24/J504.1 RK=PA22/J504.3 TK=PB1/J505.8 TF=PB0/J505.7 MCLK=external_to_codecs AD1856=mono_DATA_TD_CLK_TK_LE_TF formatter_owns_ad1856_data_clk_le";
	status->candidate_headers = "J502,J504,J505,J507,EXT1";
	status->clock_plan = "cs4272_master_256fs_divides_mclk_to_bclk_lrck_same70_slave_ssc_explicit_fb_external_low_jitter_ad1856_formatter_alt";
	status->sample_frame = "44k16_mclk=11289600_bclk=2822400_feedback_44k1=0x00058333 48k24_mclk=12288000_bclk=3072000_feedback_48k=0x00060000";
	status->dma_plan = "XDMAC_double_buffer_tbd";
	status->voltage_plan = "3v3_only_no_5v_logic";
	status->wiring_doc = "ports/same70-xplained/external-codec-i2s-cs4272-bitperfect.md ports/same70-xplained/external-codec-clock-model.py ports/same70-xplained/external-codec-original-uc3a3-map.md ports/same70-xplained/external-codec-low-jitter-clock-plan.md ports/same70-xplained/external-codec-pin-map.md ports/same70-xplained/external-dac-ad1856-mono-test.md ports/same70-xplained/external-dac-ad1856-low-jitter-formatter.md";
	status->required_signals = "cs4272_mclk_11m2896_or_12m288,bclk_to_rk_tk,lrck_to_rf_tf,dac_sdata_td,adc_sdata_rd,codec_reset,codec_control,td_to_formatter,formatter_to_ad1856_data_clk_le,ad1856_bipolar_supply";
	status->original_codec = "UC3A3_AK5394_ADC_AD1856_MONO_DAC_TEST_ES9023_REFERENCE CS4272_PRACTICAL_I2S_ADC_DAC_PLAN";
	status->original_transport = "AVR32_SSC_PDCA";
	status->boundary = "needs_external_codec_board";
	status->next_step = "cs4272_status_only_then_clock_only_probe_external_bclk_lrck_slave_ssc_gate";
}
