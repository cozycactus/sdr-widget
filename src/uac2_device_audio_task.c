/* -*- mode: c++; tab-width: 4; c-basic-offset: 4 -*- */
/* This source file is part of the ATMEL AVR32-SoftwareFramework-AT32UC3-1.5.0 Release */

/*This file is prepared for Doxygen automatic documentation generation.*/
/*! \file ******************************************************************
 *
 * \brief Management of the USB device Audio task.
 *
 * This file manages the USB device Audio task.
 *
 * - Compiler:           IAR EWAVR32 and GNU GCC for AVR32
 * - Supported devices:  All AVR32 devices with a USB module can be used.
 * - AppNote:
 *
 * \author               Atmel Corporation: http://www.atmel.com \n
 *                       Support and FAQ: http://support.atmel.no/
 *
 ***************************************************************************/

/* Copyright (c) 2009 Atmel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. The name of Atmel may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 *
 * 4. This software may only be redistributed and used in connection with an Atmel
 * AVR product.
 *
 * THIS SOFTWARE IS PROVIDED BY ATMEL "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT ARE
 * EXPRESSLY AND SPECIFICALLY DISCLAIMED. IN NO EVENT SHALL ATMEL BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE
 *
 * Modified by Alex Lee and SDR-Widget team for the sdr-widget project.
 * See http://code.google.com/p/sdr-widget/
 * Copyright under GNU General Public License v2
 */

//_____  I N C L U D E S ___________________________________________________

#include <stdio.h>
#include "usart.h"     // Shall be included before FreeRTOS header files, since 'inline' is defined to ''; leading to
                       // link errors
#include "conf_usb.h"


#if USB_DEVICE_FEATURE == ENABLED

#include "board.h"
#include "features.h"
#ifdef FREERTOS_USED
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#endif
#include "pdca.h"
#include "gpio.h"
#include "usb_drv.h"
#include "usb_descriptors.h"
#include "uac2_usb_descriptors.h"
#include "usb_standard_request.h"
#include "usb_specific_request.h"
#include "uac2_usb_specific_request.h"
#include "device_audio_task.h"
#include "uac2_device_audio_task.h"


#include "composite_widget.h"
#include "taskAK5394A.h"

// To access input select constants
#include "Mobo_config.h"


//_____ M A C R O S ________________________________________________________


//_____ D E F I N I T I O N S ______________________________________________

// For adaptive USB fallback, set this to 0 and experiment with MCU_CHAR_RATEUP and MCU_CHAR_RATEDOWN
#define FB_RATE_DELTA   64 // 0 for UAC2 adaptive testing


//_____ D E C L A R A T I O N S ____________________________________________

static U32 index = 0;
static U32 spk_index = 0;
static S16 old_gap = DAC_BUFFER_UNI / 2; // Assumed to be OK... 

// static U8 ADC_buf_USB_IN, DAC_buf_OUT;		// These are now global the ID number of the buffer used for sending out
												// to the USB and reading from USB

static U8 ep_audio_in, ep_audio_out, ep_audio_out_fb;

//!
//! @brief This function initializes the hardware/software resources
//! required for device Audio task.
//!
void uac2_device_audio_task_init(U8 ep_in, U8 ep_out, U8 ep_out_fb)
{
	index     =0;
	ADC_buf_USB_IN = INIT_ADC_USB;				// Must initialize before it can be used for any good!
	spk_index = 0;

	mute = FALSE; // applies to ADC OUT endpoint
	spk_mute = FALSE;
	ep_audio_in = ep_in;
	ep_audio_out = ep_out;
	ep_audio_out_fb = ep_out_fb;

	// With working volume flash
	// spk_vol_usb_L = usb_volume_flash(CH_LEFT, 0, VOL_READ);		// Fetch stored or default volume setting
	// spk_vol_usb_R = usb_volume_flash(CH_RIGHT, 0, VOL_READ);
	// Without working volume flash, spk_vol_usb_? = VOL_DEFAULT is set in device_audio_task.c
	spk_vol_mult_L = usb_volume_format(spk_vol_usb_L);
	spk_vol_mult_R = usb_volume_format(spk_vol_usb_R);

	xTaskCreate(uac2_device_audio_task,
				configTSK_USB_DAUDIO_NAME,
				configTSK_USB_DAUDIO_STACK_SIZE,
				NULL,
				configTSK_USB_DAUDIO_PRIORITY,
				NULL);
}


//!
//! @brief Entry point of the device Audio task management
//!


void uac2_device_audio_task(void *pvParameters)
{
//	static U32  time=0;
//	static Bool startup=TRUE;
	Bool playerStarted = FALSE; // BSB 20150516: changed into global variable
	int i = 0;
	S32 num_samples = 0;	// Used only to validate cache contents!
	S32 temp_num_samples = 0;
	S32 gap = 0;

	#ifdef FEATURE_ADC_EXPERIMENTAL
		S32 num_samples_adc = 0;
		U8 counter_44k = 0;
		U8 limit_44k = 11;	// Default setting for 44.1 rounding off into average packet length
		uint32_t sample_left = 0; 					// Must be unsigned for zeros to be right-shifted into MSBs ??
		uint32_t sample_right = 0;					// ���� convert to signed to merge with sample_R and spdif code
		const U8 EP_AUDIO_IN = ep_audio_in;
	#endif
	
	S16 time_to_calculate_gap = 0; // BSB 20131101 New variables for skip/insert
	S32 FB_error_acc = 0;	// BSB 20131102 Accumulated error for skip/insert
	U8 sample_HSB;
	U8 sample_MSB;
	U8 sample_SB;
	U8 sample_LSB;
	S32 sample_L = 0;
	S32 sample_R = 0; // BSB 20131102 Expanded for skip/insert, 20160322 changed to S32
	
	const U8 EP_AUDIO_OUT = ep_audio_out;
	const U8 EP_AUDIO_OUT_FB = ep_audio_out_fb;
	uint32_t silence_USB = SILENCE_USB_LIMIT;	// BSB 20150621: detect silence in USB channel, initially assume silence
	Bool silence_det = 0;

// Start new code for skip/insert
	static bool return_to_nominal = FALSE;		// Tweak frequency feedback system
	
	static S32 prev_sample_L = 0;	// Enable delayed writing to cache, initiated to 0, new value survives to next iteration
	static S32 prev_sample_R = 0;
	S32 diff_value = 0;
	S32 diff_sum = 0;
	S32 si_score_low = 0x7FFFFFFF;
	U32 si_index_low = 0;
	S32 si_score_high = 0;
	static S32 prev_si_score_high = 0;
	U32 si_index_high = 0;
	static S32 prev_diff_value = 0;	// Initiated to 0, new value survives to next iteration
	Bool cache_holds_silence = TRUE;
	uint16_t cache_silence_counter = 0;

	// New code for adaptive USB fallback using skip / insert s/i
	#define SI_PKG_RESOLUTION	2000 // 1000			// Apply 1/2 IIR filter. Resolution: once every 2000 packets at 250�s packet rate
	#define SI_PKG_RESOLUTION_H	2200 					// Apply 1/1 IIR filter
	#define SI_PKG_RESOLUTION_F	2400 // 1200			// Force override
	int8_t si_action = SI_NORMAL;
	int32_t si_pkg_counter = 0;
	int8_t si_pkg_increment = 0;				// Reset at sample rate change
	int8_t si_pkg_direction = SI_NORMAL;		// Reset at sample rate change

	
	// The Henry Audio and QNKTC series of hardware only use NORMAL I2S with left before right
	/* The use of this code is disregarded to try to save execution time
	#if (defined HW_GEN_AB1X) || (defined HW_GEN_SPRX) || (defined HW_GEN_FMADC)
		#define IN_LEFT 0
		#define IN_RIGHT 1
		#define OUT_LEFT 0
		#define OUT_RIGHT 1
	#else
		const U8 IN_LEFT = FEATURE_IN_NORMAL ? 0 : 1;
		const U8 IN_RIGHT = FEATURE_IN_NORMAL ? 1 : 0;
		const U8 OUT_LEFT = FEATURE_OUT_NORMAL ? 0 : 1;
		const U8 OUT_RIGHT = FEATURE_OUT_NORMAL ? 1 : 0;
	#endif
    */


	portTickType xLastWakeTime;
	xLastWakeTime = xTaskGetTickCount();

	while (TRUE) {
		vTaskDelayUntil(&xLastWakeTime, UAC2_configTSK_USB_DAUDIO_PERIOD);

//		gpio_set_gpio_pin(AVR32_PIN_PX31); // Start of task execution
		
		// Introduced into UAC2 code with mobodebug
		// Must we clear the DAC buffer contents? 
		if (dac_must_clear == DAC_MUST_CLEAR) {
//			print_dbg_char('7');
			mobo_clear_dac_channel(); 
			
			dac_must_clear = DAC_CLEARED;
		}


		#ifdef FEATURE_ADC_EXPERIMENTAL
			if (usb_alternate_setting == 0) {								// ADC interface is permanently off or about to be turned off
				if (ADC_buf_USB_IN == INIT_ADC_USB)	{						// Already in initial state. Do nothing
				}
				else {
					LED_Off( LED1 );										// Green LED turning off
					I2S_consumer &= ~I2S_CONSUMER_USB;						// USB is no longer subscribing to I2S data
	
					if (I2S_consumer == I2S_CONSUMER_NONE) {				// No other consumers? Disable DMA
						pdca_disable(PDCA_CHANNEL_SSC_RX);					// Disable I2S reception at MCU's ADC port
						pdca_disable_interrupt_reload_counter_zero(PDCA_CHANNEL_SSC_RX);
					}
					
					ADC_buf_USB_IN = INIT_ADC_USB;							// Done initializing. Wait for alt > 0 to enable it
				}
			} // alt == 0


			else if (usb_alternate_setting >= 1) { // For IN endpoint / ADC bBitResolution
				
				if (ADC_buf_USB_IN == INIT_ADC_USB)	{						// In initial state. Do something to fire up data collection!
					if (I2S_consumer == I2S_CONSUMER_NONE) {				// No other consumers? Enable DMA - ADC_site with what sample rate??
	
						// Using a large ADC buffer to serve data packets picked up by USB timing. A small buffer would be more convenient for SPDIF playback, but we'd put USB data integrity at risk with low latency						
						
						mobo_clear_adc_channel();							// Clear buffer before starting to fill it. Old comment said it might be redundant here. True?
						AK5394A_pdca_rx_enable(mobo_srd());					// ADC_site FMADC_site enable according to detected rate
						// mobo_start_spdif_tc();							// Probably no need to start spdif timer/counter since data consumption is given by USB packet rate, not timer
						
					} // Init DMA for USB IN consumer

					ADC_buf_USB_IN = INIT_ADC_USB_st2;						// Prepare for 2nd init step during first USB data transfer
					
					I2S_consumer |= I2S_CONSUMER_USB;						// USB subscribes to I2S data
					LED_On( LED1 );											// Green LED turning on to indicate recording
					
				} // Init synching up USB IN consumer's pointers to I2S RX data producer
				
				
				
				if (Is_usb_in_ready(EP_AUDIO_IN)) {	// Endpoint ready for data transfer? If so, be quick about it!
					Usb_ack_in_ready(EP_AUDIO_IN);	// acknowledge in ready

				// Must ADC consumer pointers be set up for 1st transfer?
					// Rewrite init code!
					if (ADC_buf_USB_IN == INIT_ADC_USB_st2) {
						index = ADC_BUFFER_SIZE - (pdca_channel->tcr) + ADC_BUFFER_SIZE / 2;	// Starting half a unified buffer away from DMA's write head
						index = index & ~((U32)1); 								// Clear LSB in order to start with L sample
						if (index >= ADC_BUFFER_SIZE) {						// Stay within bounds
							index -= ADC_BUFFER_SIZE;
						}
						ADC_buf_USB_IN = 0;										// Done with init, continue ordinary operation where this variable probably isn't touched
					}

					// How many stereo samples are present in a 1/4ms USB period on UAC2? 
					// 192   / 4 = 48
					// 176.4 / 4 = 44.1
					//  96   / 4 = 24
					//  88.2 / 4 = 22.05
					//  48   / 4 = 12
					//  44.1 / 4 = 11.025
					// We can use basic values but must turn 440 -> 441 on average. I.e. add one every 440/11 or 440/22 or 440/44 = 40, 20, 10 respectively

					if (spk_current_freq.frequency == FREQ_44) {
						num_samples_adc = 11;
						limit_44k = 40;
					}
					else if (spk_current_freq.frequency == FREQ_48) {
						num_samples_adc = 12;
					}
					else if (spk_current_freq.frequency == FREQ_88) {
						num_samples_adc = 22;
						limit_44k = 20;
					}
					else if (spk_current_freq.frequency == FREQ_96) {
						num_samples_adc = 24;
					}
					else if (spk_current_freq.frequency == FREQ_176) {
						num_samples_adc = 44;
						limit_44k = 10;
					}
					else if (spk_current_freq.frequency == FREQ_192) {
						num_samples_adc = 48;
					}
				
					if ( (spk_current_freq.frequency == FREQ_44) || (spk_current_freq.frequency == FREQ_88) || (spk_current_freq.frequency == FREQ_176) ) {
						counter_44k++;
						if (counter_44k == limit_44k) { 
							counter_44k = 0;
							num_samples_adc++;
						}
					}

											
					// Sync AK data stream with USB data stream
					// AK data is being filled into ~ADC_buf_DMA_write, ie if ADC_buf_DMA_write is 0
					// buffer 0 is set in the reload register of the pdca
					// So the actual loading is occurring in buffer 1
					// USB data is being taken from ADC_buf_USB_IN

					// find out the current status of PDCA transfer
					// gap is how far the ADC_buf_USB_IN is from overlapping ADC_buf_DMA_write


// Adoption of DAC side's buffered gap calculation

					// Simulated in debug03_gap.c - not verified or thoroughly analyzed
					gap = ADC_BUFFER_SIZE - index - (pdca_channel->tcr);
					if (gap < 0) {
						gap += ADC_BUFFER_SIZE;
					}
					if ( gap < ADC_BUFFER_SIZE/4 ) {
						// throttle back, transfer less
						num_samples_adc--;
						print_dbg_char('-');
					}
					else if (gap > (ADC_BUFFER_SIZE/2 + ADC_BUFFER_SIZE/4)) {
						// transfer more
						num_samples_adc++;
						print_dbg_char('+');
					}
// End of gap calculation


					Usb_reset_endpoint_fifo_access(EP_AUDIO_IN);

					for( i=0 ; i < num_samples_adc ; i++ ) {
						
						if (mute) {
							sample_left  = 0;
							sample_right = 0;
						}
						else {
							sample_left  = audio_buffer[index++];
							sample_right = audio_buffer[index++];
							
							if (index >= ADC_BUFFER_SIZE) {
								index = 0;
								#ifdef USB_STATE_MACHINE_GPIO
//										gpio_tgl_gpio_pin(AVR32_PIN_PA22);		// Perfect operation: This signal is +-90 degrees out of phase with ADC int. code's producer indicator!
								#endif
							} // index rolled over
						} // not muted

						// 3x 16-bit USB accesses are faster than 6x 8-bit accesses. Odd/even sequences of 32-bit plus 16-bit accesses are more complex and slower. See commit history
						// Always LSB first, MSB last. Either as sequences of 8-bit transfers or as 16-bit transfers read left-to-right
						// Left LSB is first right-shifted 8 bits, then AND'ed with 0x00FF, then left-shifted 8 bits. It is less costly to just AND out the bits without shifting
						if (usb_alternate_setting == ALT1_AS_INTERFACE_INDEX) {				// Stereo 24-bit data, least significant byte first, left before right
							Usb_write_endpoint_data(EP_AUDIO_IN, 16, (uint16_t) ( ( ((sample_left       ) & 0xFF00) ) | ((sample_left  >> 16) & 0x00FF) )); // left  LSB (>> 8) into MSB of 16-bit word (<<8), left   SB (>>16) into LSB of 16-bit word
							Usb_write_endpoint_data(EP_AUDIO_IN, 16, (uint16_t) ( ( ((sample_left  >> 16) & 0xFF00) ) | ((sample_right >>  8) & 0x00FF) )); // left  MSB (>>24) into MSB of 16-bit word (<<8), right LSB (>> 8) into LSB of 16-bit word
							Usb_write_endpoint_data(EP_AUDIO_IN, 16, (uint16_t) ( ( ((sample_right >>  8) & 0xFF00) ) | ((sample_right >> 24) & 0x00FF) )); // right  SB (>>16) into MSB of 16-bit word (<<8), right MSB (>>24) into LSB of 16-bit word
						}
						#ifdef FEATURE_ALT2_16BIT // UAC2 ALT 2 for 16-bit audio, MUST VERIFY!
							else if (usb_alternate_setting == ALT2_AS_INTERFACE_INDEX) {	// Stereo 16-bit data
								Usb_write_endpoint_data(EP_AUDIO_IN, 16, (uint16_t) ( ( ((sample_left  >>  8) & 0xFF00) ) | ( ((sample_left  >> 24) & 0x00FF) );
								Usb_write_endpoint_data(EP_AUDIO_IN, 16, (uint16_t) ( ( ((sample_right >>  8) & 0xFF00) ) | ( ((sample_right >> 24) & 0x00FF) );
							}
						#endif

					} // Data insertion for loop
					
					Usb_send_in(EP_AUDIO_IN);		// send the current bank
				} // end Is_usb_in_ready(EP_AUDIO_IN)
			} 	// end alt setting 1 / 2
		#endif // FEATURE_ADC_EXPERIMENTAL
		

#ifdef HW_GEN_SPRX 
		if ( (usb_alternate_setting_out >= 1) && (usb_ch_swap == USB_CH_NOSWAP) ) { // bBitResolution
#else
		if (usb_alternate_setting_out >= 1) { // bBitResolution
#endif

			/* SPDIF reduced OK */
			if (Is_usb_in_ready(EP_AUDIO_OUT_FB)) {	// Endpoint buffer free ?
				Usb_ack_in_ready(EP_AUDIO_OUT_FB);	// acknowledge in ready
				Usb_reset_endpoint_fifo_access(EP_AUDIO_OUT_FB);

				if (Is_usb_full_speed_mode()) {
					// FB rate is 3 bytes in 10.14 format

	// #ifdef HW_GEN_SPRX 	// With WM8805/WM8804 input, USB subsystem will be running off a completely wacko MCLK!
	#if ( (defined HW_GEN_SPRX) || (defined HW_GEN_AB1X) ) // For USB playback, handle semaphores
					if (input_select != MOBO_SRC_UAC2) {
	#else // Probably redundant now
					if (0) {
	#endif
						sample_LSB = FB_rate_nominal;		// but not consider it. Emulate this by sending the nominal
						sample_SB = FB_rate_nominal >> 8;	// FB rate and not the one generated by the firmware's feedback
						sample_MSB = FB_rate_nominal >> 16;
					}
					else {									// Send firmware's feedback
						sample_LSB = FB_rate;
						sample_SB = FB_rate >> 8;
						sample_MSB = FB_rate >> 16;
					}
					Usb_write_endpoint_data(EP_AUDIO_OUT_FB, 8, sample_LSB);
					Usb_write_endpoint_data(EP_AUDIO_OUT_FB, 8, sample_SB);
					Usb_write_endpoint_data(EP_AUDIO_OUT_FB, 8, sample_MSB);
				}
				else {
					// HS mode
					// HS mode, FB rate is 4 bytes in 16.16 format per 125�s.
					// Internal format is 18.14 samples per 1�s = 16.16 per 250�s
					// i.e. must right-shift once for 16.16 per 125�s.
					// So for 250us microframes it is same amount of shifting as 10.14 for 1ms frames


	//#ifdef HW_GEN_SPRX 	// With WM8805/WM8804 input, USB subsystem will be running off a completely wacko MCLK!
	#if ( (defined HW_GEN_SPRX) || (defined HW_GEN_AB1X) ) // For USB playback, handle semaphores
					if (input_select != MOBO_SRC_UAC2) {
	#else // Probably redundant now
					if (0) {
	#endif
						sample_LSB = FB_rate_nominal;
						sample_SB = FB_rate_nominal >> 8;
						sample_MSB = FB_rate_nominal >> 16;
						sample_HSB = FB_rate_nominal >> 24;
					}
					else {
						sample_LSB = FB_rate;
						sample_SB = FB_rate >> 8;
						sample_MSB = FB_rate >> 16;
						sample_HSB = FB_rate >> 24;
					}
					Usb_write_endpoint_data(EP_AUDIO_OUT_FB, 8, sample_LSB);
					Usb_write_endpoint_data(EP_AUDIO_OUT_FB, 8, sample_SB);
					Usb_write_endpoint_data(EP_AUDIO_OUT_FB, 8, sample_MSB);
					Usb_write_endpoint_data(EP_AUDIO_OUT_FB, 8, sample_HSB);
				}


				if (playerStarted) {
// 					Original Linux quirk replacement code
//					if (((spk_current_freq.frequency == FREQ_88) && (FB_rate > ((88 << 14) + (7 << 14)/10))) ||
//						((spk_current_freq.frequency == FREQ_96) && (FB_rate > ((96 << 14) + (6 << 14)/10))))
//						FB_rate -= FB_RATE_DELTA * 512;

//					Alternative Linux quirk replacement code, insert nominal FB_rate after a short interlude of requesting 99ksps (see uac2_usb_specific_request.c)

					// FIX: Will Linux quirk work with WM8805 input??
					if ( (spk_current_freq.frequency == FREQ_88) && (FB_rate > (98 << 14) ) ) {
						FB_rate = FB_rate_nominal;			// BSB 20131115 restore saved nominal feedback rate
					}
					else if ( (spk_current_freq.frequency == FREQ_96) && (FB_rate > (98 << 14) ) ) {
						FB_rate = FB_rate_nominal;			// BSB 20131115 restore saved nominal feedback rate
					}
				}

				Usb_send_in(EP_AUDIO_OUT_FB);
			} // end if (Is_usb_in_ready(EP_AUDIO_OUT_FB)) // Endpoint buffer free ?


				/* SPDIF reduced */
#ifdef HW_GEN_SPRX 	// With WM8805/WM8804 input, USB subsystem will be running off a completely wacko MCLK!
			if ( (input_select == MOBO_SRC_SPDIF0) || (input_select == MOBO_SRC_SPDIF1) || (input_select == MOBO_SRC_TOSLINK0) || (input_select == MOBO_SRC_TOSLINK1) ) {

				// Do minimal USB action to make Host believe Device is actually receiving
				if (Is_usb_out_received(EP_AUDIO_OUT)) {
					Usb_reset_endpoint_fifo_access(EP_AUDIO_OUT);

					/*
					// Needed in minimal USB functionality?
					temp_num_samples = Usb_byte_count(EP_AUDIO_OUT);
					for (i = 0; i < temp_num_samples; i++) {
						Usb_read_endpoint_data(EP_AUDIO_OUT, 8);
					}
					*/

					Usb_ack_out_received_free(EP_AUDIO_OUT);
				}
			}
			else {
#else
			if (1) {
#endif

				if (Is_usb_out_received(EP_AUDIO_OUT)) {

					Usb_reset_endpoint_fifo_access(EP_AUDIO_OUT);
					temp_num_samples = Usb_byte_count(EP_AUDIO_OUT);
					
					// temp_num_samples != 0 is needed for non-silence detection of first USB packet. But unless we own the output DMA channel, reset temp_num_samples to 0 and don't write to cache!

					// bBitResolution
					if (usb_alternate_setting_out == ALT1_AS_INTERFACE_INDEX) {		// Alternate 1 24 bits/sample, 8 bytes per stereo sample with FORMAT_SUBSLOT_SIZE_1 = 4. Must use /6 with FORMAT_SUBSLOT_SIZE_1 = 3
						temp_num_samples = temp_num_samples / 6;
					}
					#ifdef FEATURE_ALT2_16BIT // UAC2 ALT 2 for 16-bit audio
						else if (usb_alternate_setting_out == ALT2_AS_INTERFACE_INDEX) { // Alternate 2 16 bits/sample, 4 bytes per stereo sample
							temp_num_samples = temp_num_samples / 4;
						}
					#endif
					else
						temp_num_samples = 0;											// Should never get here...

					xSemaphoreTake( mutexSpkUSB, portMAX_DELAY ); // Isn't this horribly time consuming? 
					spk_usb_heart_beat++;					// indicates EP_AUDIO_OUT receiving data from host
					spk_usb_sample_counter += temp_num_samples; 	// track the num of samples received
					xSemaphoreGive(mutexSpkUSB);
					
					// ��� how much of this must be done each time this loop detect input_select == idle?

					if (!playerStarted) {	
						time_to_calculate_gap = 0;			// BSB 20131031 moved gap calculation for DAC use
						FB_error_acc = 0;					// BSB 20131102 reset feedback error
						FB_rate = FB_rate_initial;			// BSB 20131113 reset feedback rate
//						old_gap = DAC_BUFFER_UNI / 2;		// Assumed and tested OK
						usb_buffer_toggle = 0;				// BSB 20131201 Attempting improved playerstarted detection
						dac_must_clear = DAC_READY;			// Prepare to send actual data to DAC interface

// Moved to spk_index normalization after gap calculation
// Updated skip/insert system init apply to spdif playback as well! That happens without Is_usb_out_received()
//						return_to_nominal = FALSE;			// Restart feedback system
//						prev_sample_L = 0;
//						prev_sample_R = 0;
//						diff_value = 0;
//						diff_sum = 0;
//						
//						si_action = SI_NORMAL;				// No skip/insert yet
//						si_pkg_counter = 0;					// Count to when we must s/i
//						si_pkg_increment = 0;				// Not yet waiting for s/i
//						si_pkg_direction = SI_NORMAL;		// No rate mismatch detected yet		
					} // end if (!playerStarted)

					// Received samples in 10.14 is num_samples * 1<<14. In 16.16 it is num_samples * 1<<16 ???????
					// Error increases when Host (in average) sends too much data compared to FB_rate
					// A high error means we must skip.


					// Default:1 Skip:0 Insert:2 Only one skip or insert per USB package
					// .. prior to for(num_samples) Hence 1st sample in a package is skipped or inserted
					FB_error_acc = 0;

					// Site of old USB skip/insert code

					uint16_t usb_16_0;
					uint16_t usb_16_1;
					uint16_t usb_16_2;
					

//					gpio_set_gpio_pin(AVR32_PIN_PX31);		// Start copying DAC data from USB OUT to cache 

					si_score_low = 0x7FFFFFFF;		// Highest positive number, reset for each iteration
					si_index_low = 0;				// Location of "lowest energy", reset for each iteration
					si_score_high = 0;				// Lowest positive number, reset for each iteration
					si_index_high = 0;				// Location of "highest energy", reset for each iteration
					silence_det = TRUE;				// We're looking for first non-zero audio-data

					if (usb_alternate_setting_out == ALT1_AS_INTERFACE_INDEX) {		// Alternate 1 24 bits/sample, 8 bytes per stereo sample
						temp_num_samples = min(temp_num_samples, SPK_CACHE_MAX_SAMPLES);			// prevent overshoot of cache_L and cache_R
						for (i = 0; i < temp_num_samples; i++) {
							usb_16_0 = Usb_read_endpoint_data(EP_AUDIO_OUT, 16);	// L LSB, L SB. Watch carefully as they are inserted into 32-bit word below!
							usb_16_1 = Usb_read_endpoint_data(EP_AUDIO_OUT, 16);	// L MSB, R LSB
							usb_16_2 = Usb_read_endpoint_data(EP_AUDIO_OUT, 16);	// R SB,  R MSB
							
							sample_L = (((U32) (uint8_t)(usb_16_1 >> 8) ) << 24) + (((U32) (uint8_t)(usb_16_0) ) << 16) + (((U32) (uint8_t)(usb_16_0 >> 8) ) << 8); //  + sample_HSB; // bBitResolution
							sample_R = (((U32) (uint8_t)(usb_16_2) ) << 24) + (((U32) (uint8_t)(usb_16_2 >> 8) ) << 16) + (((U32) (uint8_t)(usb_16_1)) << 8); // + sample_HSB; // bBitResolution

							// Finding packet's point of lowest and highest "energy"
							diff_value = abs( (sample_L >> 8) - (prev_sample_L >> 8) ) + abs( (sample_R >> 8) - (prev_sample_R >> 8) ); // The "energy" going from prev_sample to sample
							diff_sum = diff_value + prev_diff_value; // Add the energy going from prev_prev_sample to prev_sample. 
							
							if (diff_sum < si_score_low) {
								si_score_low = diff_sum;
								si_index_low = i;
							}

							if (diff_sum > si_score_high) {
								si_score_high = diff_sum;
								si_index_high = i;
							}

							// Applying volume control to stored sample
							#ifdef FEATURE_VOLUME_CTRL
							if (usb_spk_mute != 0) {	// usb_spk_mute is heeded as part of volume control subsystem
								prev_sample_L = 0;
								prev_sample_R = 0;
							}
							else {
								if (spk_vol_mult_L != VOL_MULT_UNITY) {	// Only touch gain-controlled samples
									// 32-bit data words volume control
									prev_sample_L = (S32)( (int64_t)( (int64_t)(prev_sample_L) * (int64_t)spk_vol_mult_L ) >> VOL_MULT_SHIFT) ;
									// rand8() too expensive at 192ksps
									// sample_L += rand8(); // dither in bits 7:0
								}

								if (spk_vol_mult_R != VOL_MULT_UNITY) {	// Only touch gain-controlled samples
									// 32-bit data words volume control
									prev_sample_R = (S32)( (int64_t)( (int64_t)(prev_sample_R) * (int64_t)spk_vol_mult_R ) >> VOL_MULT_SHIFT) ;
									// rand8() too expensive at 192ksps
									// sample_R += rand8(); // dither in bits 7:0
								}
							}
							#endif
							
							// It is time consuming to test for each stereo sample!
							if (input_select == MOBO_SRC_UAC2) {					// Only write to cache with the right permissions! Double check permission and num_samples
								cache_L[i] = prev_sample_L; 
								cache_R[i] = prev_sample_R;
							} // end input_select == MOBO_SRC_UAC2
							
							// Establish history
							prev_sample_L = sample_L;
							prev_sample_R = sample_R;
							prev_diff_value = diff_value;
						} // end for temp_num_samples
					} // end if alt setting 1

					#ifdef FEATURE_ALT2_16BIT // UAC2 ALT 2 for 16-bit audio
										
						else if (usb_alternate_setting_out == ALT2_AS_INTERFACE_INDEX) {	// Alternate 2 16 bits/sample, 4 bytes per stereo sample
							temp_num_samples = min(temp_num_samples, SPK_CACHE_MAX_SAMPLES);			// prevent overshoot of cache_L and cache_R
							for (i = 0; i < temp_num_samples; i++) {
								usb_16_0 = Usb_read_endpoint_data(EP_AUDIO_OUT, 16);	// L LSB, L MSB. Watch carefully as they are inserted into 32-bit word below!
								usb_16_1 = Usb_read_endpoint_data(EP_AUDIO_OUT, 16);	// L LSB, R MSB

								sample_L = (((U32) (uint8_t)(usb_16_0) ) << 24) + (((U32) (uint8_t)(usb_16_0 >> 8) ) << 16);
								sample_R = (((U32) (uint8_t)(usb_16_1)) << 24) + (((U32) (uint8_t)(usb_16_1 >> 8)) << 16);
								
								// Finding packet's point of lowest and highest "energy"
								diff_value = abs( (sample_L >> 8) - (prev_sample_L >> 8) ) + abs( (sample_R >> 8) - (prev_sample_R >> 8) ); // The "energy" going from prev_sample to sample
								diff_sum = diff_value + prev_diff_value; // Add the energy going from prev_prev_sample to prev_sample. 
							
								if (diff_sum < si_score_low) {
									si_score_low = diff_sum;
									si_index_low = i;
								}
								
								if (diff_sum > si_score_high) {
									si_score_high = diff_sum;
									si_index_high = i;
								}
								
								// Applying volume control to stored sample
								#ifdef FEATURE_VOLUME_CTRL
								if (usb_spk_mute != 0) {	// usb_spk_mute is heeded as part of volume control subsystem
									prev_sample_L = 0;
									prev_sample_R = 0;
								}
								else {
									if (spk_vol_mult_L != VOL_MULT_UNITY) {	// Only touch gain-controlled samples
										// 32-bit data words volume control
										prev_sample_L = (S32)( (int64_t)( (int64_t)(prev_sample_L) * (int64_t)spk_vol_mult_L ) >> VOL_MULT_SHIFT) ;
										// rand8() too expensive at 192ksps
										// sample_L += rand8(); // dither in bits 7:0
									}

									if (spk_vol_mult_R != VOL_MULT_UNITY) {	// Only touch gain-controlled samples
										// 32-bit data words volume control
										prev_sample_R = (S32)( (int64_t)( (int64_t)(prev_sample_R) * (int64_t)spk_vol_mult_R ) >> VOL_MULT_SHIFT) ;
										// rand8() too expensive at 192ksps
										// sample_R += rand8(); // dither in bits 7:0
									}
								}
								#endif
								
								// It is time consuming to test for each stereo sample!
								if (input_select == MOBO_SRC_UAC2) {					// Only write to cache with the right permissions! Double check permission and num_samples
									cache_L[i] = prev_sample_L;
									cache_R[i] = prev_sample_R;
								} // End input_select == MOBO_SRC_UAC2
							
								// Establish history
								prev_sample_L = sample_L;
								prev_sample_R = sample_R;
								prev_diff_value = diff_value;
							} // end for temp_num_samples
						} // end if alt setting 2
					#endif // UAC2 ALT 2 for 16-bit audio						

//					gpio_clr_gpio_pin(AVR32_PIN_PX31);		// End copying DAC data from USB OUT to cache

					// Silence detector v.4 reuses energy detection code
					// L = 0 0 0 256 0 0 0
					// R = 0 0 0 256 0 0 0
					// -> si_score_high = 4
					// It is practically a right-shift by 6 bits
					if ( (si_score_high + (abs(sample_L) >> 8) + (abs(sample_R) >> 8) ) > IS_SILENT) {
						silence_det = FALSE;
					}

					// New site for setting playerStarted and aligning buffers
					if (!silence_det) {		// There is actual USB audio.
//						#ifdef HW_GEN_SPRX		// With WM8805/WM8804 present, handle semaphores
						#if ( (defined HW_GEN_SPRX) || (defined HW_GEN_AB1X) )	// For USB playback, handle semaphores
							if (input_select == MOBO_SRC_NONE) {				// Always directly preceding take for RT reasons
								if (xSemaphoreTake(input_select_semphr, 10) == pdTRUE) {		// Re-take of taken semaphore returns false
									input_select = MOBO_SRC_UAC2;				// Claim input_select ASAP so that WM won't take it
//									print_dbg_char('\n');						// USB takes
									print_dbg_char('[');						// USB takes
									playerStarted = TRUE;						// Is it better off here?
									
//									mobo_xo_select(spk_current_freq.frequency, input_select);
//									mobo_clock_division(spk_current_freq.frequency);
									samples_per_package_min = (spk_current_freq.frequency >> 12) - (spk_current_freq.frequency >> 14); // 250us worth of music +- some slack
									samples_per_package_max = (spk_current_freq.frequency >> 12) + (spk_current_freq.frequency >> 14);
									must_init_xo = TRUE;
//									must_init_spk_index = TRUE;					// New frequency setting means resync DAC DMA
//									print_dbg_char('R');

									#ifdef HW_GEN_SPRX 
										// Report to cpu and debug terminal
										if (usb_ch == USB_CH_B) {
											print_cpu_char(CPU_CHAR_UAC2_B);	// USB audio Class 2 on rear USB-B plug
										}
										else if (usb_ch == USB_CH_C) {
											print_cpu_char(CPU_CHAR_UAC2_C);	// USB audio Class 2 on front USB-C plug
										}

										mobo_led_select(spk_current_freq.frequency, input_select);
										// mobo_i2s_enable(MOBO_I2S_ENABLE);		// Hard-unmute of I2S pin
									#endif
								}												// Hopefully, this code won't be called repeatedly. Would there be time??
								else {
									print_dbg_char('*');
									print_dbg_char('a');
								}
							} // if (input_select == MOBO_SRC_NONE)
						#else // not ( (defined HW_GEN_SPRX) || (defined HW_GEN_AB1X) ) // For USB playback, handle semaphores
							input_select = MOBO_SRC_UAC2;
						#endif
					} // End silence_det == 0 & MOBO_SRC_NONE


					// Validate the data we just put into the cache so that it can be read from there
					if (input_select == MOBO_SRC_UAC2) {
						cache_holds_silence = silence_det;		// Use this to determine how to use contents of cache
						num_samples = temp_num_samples;
					}


					// End of writing USB OUT data to cache. Writing takes place at the end of this function


					// Detect USB silence. We're counting USB packets. UAC2: 250us, UAC1: 1ms
					if (silence_det == 1) {
						if (!USB_IS_SILENT())
							silence_USB ++;
					}
					else // stereo sample is non-zero
						silence_USB = SILENCE_USB_INIT;			// USB interface is not silent!

					Usb_ack_out_received_free(EP_AUDIO_OUT);

//					if ( (USB_IS_SILENT()) && (input_select == MOBO_SRC_UAC2) ) { // Oops, we just went silent, probably from pause
					// mobodebug untested fix
					if ( (USB_IS_SILENT()) && (input_select == MOBO_SRC_UAC2) && (playerStarted != FALSE) ) { // Oops, we just went silent, probably from pause
						playerStarted = FALSE;

						#ifdef HW_GEN_SPRX 					// Dedicated mute pin
							// mobo_i2s_enable(MOBO_I2S_DISABLE);	// Hard-mute of I2S pin
						#endif

						// Clear buffers before give
//						print_dbg_char('8');

						mobo_clear_dac_channel();
						// mobodebug Could this be the spot which sucks up CPU time with input_select == MOBO_SRC_UAC2

//						#ifdef HW_GEN_SPRX		// With WM8805/WM8804 present, handle semaphores
						#if ( (defined HW_GEN_SPRX) || (defined HW_GEN_AB1X) ) // For USB playback, handle semaphores
							if (input_select != MOBO_SRC_NONE) {		// Always directly preceding give for RT reasons
								if( xSemaphoreGive(input_select_semphr) == pdTRUE ) {
									mobo_clear_dac_channel();				// Leave the DAC buffer empty as we check out
									print_dbg_char(']');					// USB gives
									print_dbg_char('\n');					// USB gives

									// Report to cpu and debug terminal
									print_cpu_char(CPU_CHAR_IDLE);
									#ifdef HW_GEN_SPRX
										mobo_led_select(FREQ_NOCHANGE, MOBO_SRC_NONE);	// User interface NO-channel indicator 
									#endif
									input_select = MOBO_SRC_NONE;			// Do this LATE! Indicate WM may take over control
								}
								else {
									print_dbg_char('*');
									print_dbg_char('b');
								}
							} // if (input_select != MOBO_SRC_NONE)
						#endif
					}
				}	// end if (Is_usb_out_received(EP_AUDIO_OUT))
			} // end if(1) / input_select on RX chip
		} // end if (usb_alternate_setting_out >= 1)

		else { // ( (usb_alternate_setting_out >= 1) && (usb_ch_swap == USB_CH_NOSWAP) )
			/* SPDIF reduced */
#ifdef HW_GEN_SPRX	// With WM8805/WM8804 input, USB subsystem will be running off a completely wacko MCLK!
			if ( (input_select == MOBO_SRC_SPDIF0) || (input_select == MOBO_SRC_TOSLINK0) || (input_select == MOBO_SRC_TOSLINK1) ) {
				// Do nothing at this stage
			}
			else {
#else
			if (1) {
#endif
				// Execute from here with USB input or no input
				
//				playerStarted = FALSE;  // mobodebug, commented out here and included below
				silence_USB = SILENCE_USB_LIMIT;				// Indicate USB silence

				#ifdef HW_GEN_SPRX	// Dedicated mute pin
					if (usb_ch_swap == USB_CH_SWAPDET)
						usb_ch_swap = USB_CH_SWAPACK;			// Acknowledge a USB channel swap, that takes this task into startup
				#endif

//				if (input_select == MOBO_SRC_UAC2) {			// Set from playing nonzero USB
				// mobodebug untested fix
				if ( (input_select == MOBO_SRC_UAC2) && (playerStarted != FALSE) ) {			// Set from playing nonzero USB
					playerStarted = FALSE;	// Inserted here in mobodebug untested fix, removed above

					#ifdef HW_GEN_SPRX	// Dedicated mute pin
						// mobo_i2s_enable(MOBO_I2S_DISABLE);		// Hard-mute of I2S pin
					#endif

					// Clear buffers before give
//					print_dbg_char('9');
					mobo_clear_dac_channel();
					// mobodebug is this another scheduler thief?

//					#ifdef HW_GEN_SPRX		// With WM8805/WM8804 present, handle semaphores
					#if ( (defined HW_GEN_SPRX) || (defined HW_GEN_AB1X) ) // For USB playback, handle semaphores
						if (input_select != MOBO_SRC_NONE) {		// Always directly preceding give for RT reasons						
							if (xSemaphoreGive(input_select_semphr) == pdTRUE) {
								mobo_clear_dac_channel();				// Leave the DAC buffer empty as we check out
								print_dbg_char(62); // '>'				// USB gives after silence
								print_dbg_char('\n');					// USB gives

								// Report to cpu and debug terminal
								print_cpu_char(CPU_CHAR_IDLE);
								#ifdef HW_GEN_SPRX
									mobo_led_select(FREQ_NOCHANGE, MOBO_SRC_NONE);	// User interface NO-channel indicator
								#endif
								input_select = MOBO_SRC_NONE;			// Do this LATE! Indicate WM may take over control
							}
							else {
								print_dbg_char('*');
								print_dbg_char('c');
							}
						} // if (input_select != MOBO_SRC_NONE)
					#endif
				}
			}
		} // end opposite of usb_alternate_setting_out >=  1


		// BSB 20131201 attempting improved playerstarted detection
			/* SPDIF reduced */
#ifdef HW_GEN_SPRX 	// With WM8805/WM8804 input, USB subsystem will be running off a completely wacko MCLK!
		// On new hardware, don't do this if spdif is playing
		if ( (input_select == MOBO_SRC_SPDIF0) || (input_select == MOBO_SRC_SPDIF1) || (input_select == MOBO_SRC_TOSLINK0) || (input_select == MOBO_SRC_TOSLINK1) ) {
			// Do nothing at this stage
		}
		else {
#else
		// On old hardware always do this for USB inputs and idle
		if (1) {
#endif

			// Execute here if input is any USB input or no input at all
			// Practically, do mobo_clear_channel() with input_select == MOBO_SRC_NONE, but only touch semaphore controlled stuff with input_select == MOBO_SRC_UAC2
			// We should rephrase if() statements!			

			if (usb_buffer_toggle == USB_BUFFER_TOGGLE_LIM)	{	// Counter is increased by DMA and uacX_taskAK5394A.c, decreased by seq. code
				usb_buffer_toggle = USB_BUFFER_TOGGLE_PARK;		// When it reaches limit, stop counting and park this mechanism
				playerStarted = FALSE;
				
				mobo_clear_dac_channel();
				print_dbg_char('q');
				
				if (input_select == MOBO_SRC_UAC2) {			// We must own input_select before we can give it away!
					// If playing from USB on new hardware, give away control at this stage to permit toslink scanning
//					#ifdef HW_GEN_SPRX		// With WM8805/WM8804 present, handle semaphores
					#if ( (defined HW_GEN_SPRX) || (defined HW_GEN_AB1X) ) // For USB playback, handle semaphores
						if( xSemaphoreGive(input_select_semphr) == pdTRUE ) {
							print_dbg_char(')');				// USB gives after toggle timeout
							print_dbg_char('\n');				// USB gives after toggle timeout

							// Report to cpu and debug terminal
							print_cpu_char(CPU_CHAR_IDLE);
							#ifdef HW_GEN_SPRX
								mobo_led_select(FREQ_NOCHANGE, MOBO_SRC_NONE);	// User interface NO-channel indicator
							#endif
							input_select = MOBO_SRC_NONE;			// Do this LATE! Indicate WM may take over control
						}
						else {
							print_dbg_char('*');
							print_dbg_char('d');
						}
					#endif
				} // input_select == MOBO_SRC_UAC2

			} // end if usb buffer toggle limit reach
		} // end if USB playback or no playback


		// Process digital input
		#ifdef HW_GEN_SPRX
			// The passed parameters are overwritten if input_select is among spdif sources
			mobo_handle_spdif(&si_index_low, &si_score_high, &si_index_high, &num_samples, &cache_holds_silence);
		#endif

		// Start checking gap and then writing from cache to spk_buffer
		// Don't check input_source again, trust that num_samples > 0 only occurs when cache was legally written to

		num_samples = min(num_samples, SPK_CACHE_MAX_SAMPLES);	// prevent overshoot of cache_L and cache_R
		if (num_samples > 0) {									// Only start copying when there is something to legally copy

			// Consider long periods of silence to cause buffer reset
			#define CACHE_SILENCE_LIMIT	200						// 50ms of silence at 250�s packet rate
			if (cache_holds_silence) {
				if (cache_silence_counter < CACHE_SILENCE_LIMIT) {
					cache_silence_counter ++;
				}
				else {
					must_init_spk_index = TRUE;					// Long silence written through cache = may extend or shorten silence period
					// must_init_spk_index == TRUE will set cache_silence_counter = 0; so that we don't clear again and again
				}
			} 
			else {
				cache_silence_counter = 0;
			}


			si_action = SI_NORMAL;								// Most of the time, don't apply s/i. Only determine whether to s/i when time_to_calculate_gap == 0

			// Calculate gap after N packets, NOT each time feedback endpoint is polled
			if (time_to_calculate_gap > 0) {
				time_to_calculate_gap--;
			}
//			else if (!must_init_spk_index) {					// Time to calculate gap AND normal operation
			else if ( (!cache_holds_silence) && (!must_init_xo) && (!must_init_spk_index) && (input_select != MOBO_SRC_NONE) && (num_samples >= samples_per_package_min) && (num_samples <= samples_per_package_max) ) {	// Time to calculate gap AND normal operation
				time_to_calculate_gap = SPK_PACKETS_PER_GAP_CALCULATION - 1;

				gap = DAC_BUFFER_UNI - spk_index - (spk_pdca_channel->tcr);
				if (gap < 0) {
					gap += DAC_BUFFER_UNI;
				}

				if (gap < old_gap) {
					if (gap < SPK_GAP_LSKIP) { 					// gap < outer lower bound => 2*FB_RATE_DELTA, SI_SKIP
						if (usb_alternate_setting_out >= 1) {	// Rate system is only used by UAC2
							FB_rate -= 2*FB_RATE_DELTA;
						}
						old_gap = gap;

						// Aadaptive USB fallback using skip = SPDIF insert s/i
						si_pkg_counter = SI_PKG_RESOLUTION;		// Start s/i immediately
						si_pkg_increment ++;
						si_pkg_direction = SI_SKIP;				// Host must slow down
						
						#ifdef HW_GEN_SPRX 
							// rate/channel write status
							if ( (input_select == MOBO_SRC_SPDIF0) || (input_select == MOBO_SRC_SPDIF1) || (input_select == MOBO_SRC_TOSLINK0) || (input_select == MOBO_SRC_TOSLINK1) ) {
								mobo_rate_storage(spdif_rx_status.frequency, input_select, si_pkg_direction, RATE_STORE);
							}
							else if (input_select == MOBO_SRC_UAC2) {	// Only applies to broken
								mobo_rate_storage(spk_current_freq.frequency, input_select, si_pkg_direction, RATE_STORE);
							}
						#endif
									
						// Report to cpu and debug terminal
						print_cpu_char(CPU_CHAR_DECDEC_FREQ);
									
						return_to_nominal = TRUE;
					}
								
					else if (gap < SPK_GAP_L1) { 				// gap < inner lower bound => 1*FB_RATE_DELTA
						if (input_select == MOBO_SRC_UAC2) {	// Rate system is only used by UAC2, as is this limit for adaptive feedback
							FB_rate -= FB_RATE_DELTA;
							old_gap = gap;

							// Report to cpu and debug terminal
							print_cpu_char(CPU_CHAR_DEC_FREQ);

							return_to_nominal = TRUE;
						}
					}

					// New feature: gap returning to nominal gap
					// Goal: approach nominal sample rate without cycles of gap low-high-low
					// but rather attempt up gaps being low-nominal-low
					else if (gap < SPK_GAP_NOM) {
						if (return_to_nominal) {
							if (usb_alternate_setting_out >= 1) {	// Rate system is only used by UAC2
								FB_rate -= FB_RATE_DELTA;
							}
							old_gap = gap;
										
							// Aadaptive USB fallback using skip = SPDIF insert s/i
							si_pkg_counter = 0;						// No s/i for a while
							si_pkg_increment = 0;					// Not counting up to next s/i event
							si_pkg_direction = SI_NORMAL;			// Host will operate at nominal speed

							// Report to cpu and debug terminal
							print_cpu_char(CPU_CHAR_NOMDEC_FREQ);

							return_to_nominal = FALSE;
						}
					}
				}
				else if (gap > old_gap) {
					if (gap > SPK_GAP_USKIP) { 			// gap > outer upper bound => 2*FB_RATE_DELTA and SI_INSERT
						if (usb_alternate_setting_out >= 1) {	// Rate system is only used by UAC2
							FB_rate += 2*FB_RATE_DELTA; 
						}
						old_gap = gap;
									
						// New code for adaptive USB fallback using skip / insert s/i
						si_pkg_counter = SI_PKG_RESOLUTION;		// Start s/i immediately
						si_pkg_increment ++;
						si_pkg_direction = SI_INSERT;			// Host must speed up
						
						#ifdef HW_GEN_SPRX
							// rate/channel write status
							if ( (input_select == MOBO_SRC_SPDIF0) || (input_select == MOBO_SRC_SPDIF1) || (input_select == MOBO_SRC_TOSLINK0) || (input_select == MOBO_SRC_TOSLINK1) ) {
								mobo_rate_storage(spdif_rx_status.frequency, input_select, si_pkg_direction, RATE_STORE);
							}
							else if (input_select == MOBO_SRC_UAC2) {	// Only applies to broken 
								mobo_rate_storage(spk_current_freq.frequency, input_select, si_pkg_direction, RATE_STORE);
							}
						#endif

						// Report to cpu and debug terminal
						print_cpu_char(CPU_CHAR_INCINC_FREQ);	// This is '*'

						return_to_nominal = TRUE;
					}

					else if (gap > SPK_GAP_U1) { 				// gap > inner upper bound => 1*FB_RATE_DELTA
						if (input_select == MOBO_SRC_UAC2) {	// Rate system is only used by UAC2, as is this limit for adaptive feedback
							FB_rate += FB_RATE_DELTA;
							old_gap = gap;

							// Report to cpu and debug terminal
							print_cpu_char(CPU_CHAR_INC_FREQ);

							return_to_nominal = TRUE;
						}
					}

					// New feature: gap returning to nominal gap
					// Goal: approach nominal sample rate without cycles of gap low-high-low
					// but rather attempt up gaps being low-nominal-low
					else if (gap > SPK_GAP_NOM) {
						if (return_to_nominal) {
							if (usb_alternate_setting_out >= 1) {	// Rate system is only used by UAC2
								FB_rate += FB_RATE_DELTA;
							}
							old_gap = gap;

							// New code for adaptive USB fallback using skip / insert s/i
							si_pkg_counter = 0;						// No s/i for a while
							si_pkg_increment = 0;					// Not counting up to next s/i event
							si_pkg_direction = SI_NORMAL;			// Host will operate at nominal speed
										
							// Report to cpu and debug terminal
							print_cpu_char(CPU_CHAR_NOMINC_FREQ);
										
							return_to_nominal = FALSE;
						}
					}
				}
			} // end if time_to_calculate_gap == 0

			si_pkg_counter += si_pkg_increment;					// When must we perform s/i? This doesn't yet account for zero packages or historical energy levels
			// Must we force action?
			if (si_pkg_counter > SI_PKG_RESOLUTION_F) {			// "Force", apply skip/insert regardless
				si_pkg_counter = 0;								// instead of -= SI_PKG_RESOLUTION
				si_action = si_pkg_direction;					// Apply only once in a while
//				print_dbg_char('F');
			}
			else if (si_pkg_counter > SI_PKG_RESOLUTION_H) {	// "Hint", compare to 1/1 of finding in IIR filter
				if (si_score_high < prev_si_score_high) {		// si_score_high follows packet, prev_si_score_high is static
					si_pkg_counter = 0;							// instead of -= SI_PKG_RESOLUTION
					si_action = si_pkg_direction;				// Apply only once in a while
				}
//				print_dbg_char('H');
			}
			else if (si_pkg_counter > SI_PKG_RESOLUTION) {		// Preferred: compare to 1/2 of finding in IIR filter
				if (si_score_high < (prev_si_score_high / 2) ) {	// si_score_high follows packet, prev_si_score_high is static
					si_pkg_counter = 0;							// instead of -= SI_PKG_RESOLUTION
					si_action = si_pkg_direction;				// Apply only once in a while
				}
			}

//			gpio_set_gpio_pin(AVR32_PIN_PX31);				// Start copying cache to spk_buffer_X


			// Moving xo init here in an attempt to counter tock-tock-tock with two connected spdif sources operating at opposite rates
			if (must_init_xo) {
				if (!cache_holds_silence) {
					#if ( (defined HW_GEN_SPRX) || (defined HW_GEN_AB1X) )
						if ( (input_select == MOBO_SRC_SPDIF0) || (input_select == MOBO_SRC_SPDIF1) || (input_select == MOBO_SRC_TOSLINK0) || (input_select == MOBO_SRC_TOSLINK1) ) {
							mobo_xo_select(spdif_rx_status.frequency, input_select);
							mobo_clock_division(spdif_rx_status.frequency);
						}
						else if (input_select == MOBO_SRC_UAC2) {	// Only broken feedback system ever wrote to this one
							mobo_xo_select(spk_current_freq.frequency, input_select);
							mobo_clock_division(spk_current_freq.frequency);
						}
					#endif

					must_init_xo = FALSE;
					must_init_spk_index = TRUE;					// New frequency setting means resync DAC DMA
				}
			}

			// spk_index normalization
			if (must_init_spk_index) {
				
//				gpio_tgl_gpio_pin(AVR32_PIN_PA22);		// Indicate resetting
				
				// USB startup has this a little past the middle of the output buffer. But SPDIF startup seems to let it start a bit too soon
				// Understand that before code can be fully trusted!

				// Starting point basics
				spk_index = DAC_BUFFER_UNI - (spk_pdca_channel->tcr) + DAC_BUFFER_UNI / 2; // Starting half a unified buffer away from DMA's read head

				// Starting point offset depending on detected source speed
				#ifdef HW_GEN_SPRX
					// rate/channel read status and adapt starting point in buffer
					int8_t stored_direction = SI_NORMAL;
					if ( (input_select == MOBO_SRC_SPDIF0) || (input_select == MOBO_SRC_SPDIF1) || (input_select == MOBO_SRC_TOSLINK0) || (input_select == MOBO_SRC_TOSLINK1) ) {
						stored_direction = mobo_rate_storage(spdif_rx_status.frequency, input_select, 0, RATE_RETRIEVE);
					}
					else if (input_select == MOBO_SRC_UAC2) {	// Only broken feedback system ever wrote to this one
						stored_direction = mobo_rate_storage(spk_current_freq.frequency, input_select, 0, RATE_RETRIEVE);
					}
				
					if (stored_direction == SI_INSERT) {	// Source is known to be slow and we should start late in buffer
						spk_index += SPK_GAP_SIOFS;
//						print_dbg_char('i');
					}
					else if (stored_direction == SI_SKIP) {	// Source is known to be fast and we should start early in buffer
						spk_index -= SPK_GAP_SIOFS;
//						print_dbg_char('s');
					}
				#endif
				
				// Starting point adjustments
				spk_index = spk_index & ~((U32)1); 		// Clear LSB in order to start with L sample
				if (spk_index >= DAC_BUFFER_UNI) {		// Stay within bounds
					spk_index -= DAC_BUFFER_UNI;
				}
				else if (spk_index < 0) {			// Stay within bounds
					spk_index += DAC_BUFFER_UNI;
				}

				gap = SPK_GAP_NOM;
				old_gap = SPK_GAP_NOM;
				si_pkg_counter = 0;						// No s/i for a while
				si_pkg_increment = 0;					// Not counting up to next s/i event
				si_pkg_direction = SI_NORMAL;			// Host will operate at nominal speed
				si_action = SI_NORMAL;

				// Updated skip/insert system init apply to spdif playback as well! That happens without Is_usb_out_received()
				return_to_nominal = FALSE;				// Restart feedback system
				prev_sample_L = 0;
				prev_sample_R = 0;
				prev_si_score_high = 0;				// Clear energy history
				diff_value = 0;
				diff_sum = 0;
				cache_silence_counter = 0;				// Don't look for cached silence for a little while
				
				must_init_spk_index = FALSE;
			}

			prev_si_score_high = (si_score_high >> 1) + (prev_si_score_high >> 1);	// Establish energy history, primitive IIR, out(n) = 0.5*out(n-1) + 0.5*in(n)

			i = 0;
			while (i < si_index_low) { // before skip/insert
				// Fetch from cache
				sample_L = cache_L[i];
				sample_R = cache_R[i];
				
				spk_buffer[spk_index++] = sample_L;
				spk_buffer[spk_index++] = sample_R;
				
				if (spk_index >= DAC_BUFFER_UNI) {
					spk_index = 0;
					gpio_tgl_gpio_pin(AVR32_PIN_PX30); // Ideally 90 or 270 deg out of phase with interrupt based producer. Bad: 0 or 180 deg out of phase!

					// Actually used and needed? Now it's running at half the rate compared to non-unified spk buffer
					usb_buffer_toggle--;			// Counter is increased by DMA, decreased by seq. code.
				}

				i++;	// Next sample from cache
			} // end while i - before skip/insert

			// i now points at sample to be skipped or inserted
			sample_L = cache_L[i];
			sample_R = cache_R[i];

			if (si_action == SI_SKIP) {
				// Do nothing
			}
			else if (si_action == SI_NORMAL) {
				// Single stereo sample
				spk_buffer[spk_index++] = sample_L;
				spk_buffer[spk_index++] = sample_R;
				
				if (spk_index >= DAC_BUFFER_UNI) {
					spk_index = 0;
					gpio_tgl_gpio_pin(AVR32_PIN_PX30); // Ideally 90 or 270 deg out of phase with interrupt based producer. Bad: 0 or 180 deg out of phase!

					// Actually used and needed? Now it's running at half the rate compared to non-unified spk buffer
					usb_buffer_toggle--;			// Counter is increased by DMA, decreased by seq. code.
				}
			} // End SI_NORMAL
			
			else if (si_action == SI_INSERT) {
				// First of two insertions:
				spk_buffer[spk_index++] = sample_L;
				spk_buffer[spk_index++] = sample_R;
				
				if (spk_index >= DAC_BUFFER_UNI) {
					spk_index = 0;
					gpio_tgl_gpio_pin(AVR32_PIN_PX30); // Ideally 90 or 270 deg out of phase with interrupt based producer. Bad: 0 or 180 deg out of phase!

					// Actually used and needed? Now it's running at half the rate compared to non-unified spk buffer
					usb_buffer_toggle--;			// Counter is increased by DMA, decreased by seq. code.
				} // End switching buffers
							
				// Second insertion:
				spk_buffer[spk_index++] = sample_L;
				spk_buffer[spk_index++] = sample_R;
				
				if (spk_index >= DAC_BUFFER_UNI) {
					spk_index = 0;
					gpio_tgl_gpio_pin(AVR32_PIN_PX30); // Ideally 90 or 270 deg out of phase with interrupt based producer. Bad: 0 or 180 deg out of phase!

					// Actually used and needed? Now it's running at half the rate compared to non-unified spk buffer
					usb_buffer_toggle--;			// Counter is increased by DMA, decreased by seq. code.
				} // End switching buffers
			} // End SI_INSERT
			i++; // Point to the sample after the one which was skipped or inserted
						
			while (i < num_samples) { // after skip/insert
				// Fetch from cache
				sample_L = cache_L[i];
				sample_R = cache_R[i];

				spk_buffer[spk_index++] = sample_L;
				spk_buffer[spk_index++] = sample_R;
				
				if (spk_index >= DAC_BUFFER_UNI) {
					spk_index = 0;
					gpio_tgl_gpio_pin(AVR32_PIN_PX30); // Ideally 90 or 270 deg out of phase with interrupt based producer. Bad: 0 or 180 deg out of phase!

					// Actually used and needed? Now it's running at half the rate compared to non-unified spk buffer
					usb_buffer_toggle--;			// Counter is increased by DMA, decreased by seq. code.
				} // End switching buffers

				i++;
			} // end while i - after skip/insert
						
			num_samples = 0; // Write is complete. Source must set it to > 0 for next write to spk_buffer_X to happen

//			gpio_clr_gpio_pin(AVR32_PIN_PX31);		// End copying DAC data from cache to spk_audio_buffer_X

		} // end if num_samples > 0
		// End writing from cache to spk_buffer

//		gpio_clr_gpio_pin(AVR32_PIN_PX31); // End of task execution

	} // end while vTask
}

#endif  // USB_DEVICE_FEATURE == ENABLED
