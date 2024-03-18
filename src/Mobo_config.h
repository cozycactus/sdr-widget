/* -*- mode: c++; tab-width: 4; c-basic-offset: 4 -*- */
/*
 * Mobo_config.h
 *
 *  Created on: 2010-06-13
 *      Author: Loftur Jonasson, TF3LJ
 */

#ifndef MOBO_CONFIG_H_
#define MOBO_CONFIG_H_

#include <stdint.h>
#include "board.h"


// Hardware control functions

// Low-power sleep for a number of milliseconds by means of RTC
// Use only during init, before any MCU hardware (including application use of RTC) is enabled.
void mobo_sleep_rtc_ms(uint16_t time_ms);

// Print the frequency
void mobo_print_selected_frequency(U32 frequency);

// Audio Widget select oscillator
void mobo_xo_select(U32 frequency, uint8_t source);

// Master clock to DAC's I2S port frequency setup
void mobo_clock_division(U32 frequency);


// Empty the contents of the incoming pdca buffers
void mobo_clear_adc_channel(void);

// Empty the contents of the outgoing pdca buffers
void mobo_clear_dac_channel(void);


// Commands to and from CPU, primarily used with #ifdef USB_REDUCED_DEBUG
#define CPU_CHAR_BOOT				'H'	// MCU boots up
#define CPU_CHAR_IDLE				'0'	// MCU reports no input
#define CPU_CHAR_SPDIF0				's'	// MCU reports that SPDIF0 is input
#define CPU_CHAR_SPDIF1				'S'	// MCU reports that SPDIF1 is input
#define CPU_CHAR_TOSLINK0			't'	// MCU reports that TOSLINK0 is input
#define CPU_CHAR_TOSLINK1			'T'	// MCU reports that TOSLINK1 is input
#define CPU_CHAR_UAC1_B				'b'	// MCU reports that Rear USB-B USB Audio Class 1 is input
#define CPU_CHAR_UAC2_B				'B'	// MCU reports that Rear USB-B USB Audio Class 2 is input
#define CPU_CHAR_UAC1_C				'c'	// MCU reports that Front USB-C USB Audio Class 1 is input
#define CPU_CHAR_UAC2_C				'C'	// MCU reports that Front USB-C USB Audio Class 2 is input
#define CPU_CHAR_SRC_DEF			'y' // MCU reports that Input is not known
#define CPU_CHAR_44					'1' // MCU requests oscillator for 44.1ksps and outputs on that rate
#define CPU_CHAR_48					'2' // MCU requests oscillator for 48ksps and outputs on that rate
#define CPU_CHAR_88					'3' // MCU requests oscillator for 88.2ksps and outputs on that rate
#define CPU_CHAR_96					'4' // MCU requests oscillator for 96ksps and outputs on that rate
#define CPU_CHAR_176				'5' // MCU requests oscillator for 176.4ksps and outputs on that rate
#define CPU_CHAR_192				'6' // MCU requests oscillator for 192ksps and outputs on that rate
#define CPU_CHAR_REGEN				'X' // MCU requests regenerated clock from SPDIF/TOSLINK receiver. Probably outputs on last reported rate ('1'-'6')
#define CPU_CHAR_RATE_DEF			'z' // 'Y' // MCU rate request is unknown
#define CPU_CHAR_INC_FREQ			'+' // MCU requests increased sample rate from Host
#define CPU_CHAR_DEC_FREQ			'-' // MCU requests decreased sample rate from Host
#define CPU_CHAR_NOMINC_FREQ		'.' // MCU requests increased sample rate from Host while returning to nominal gap
#define CPU_CHAR_NOMDEC_FREQ		':' // MCU requests decreased sample rate from Host while returning to nominal gap
#define CPU_CHAR_INCINC_FREQ		'*' // MCU intensely requests increased-increased sample rate from Host
#define CPU_CHAR_DECDEC_FREQ		'/' // MCU intensely requests decreased-decreased sample rate from Host
#define CPU_CHAR_ALIVE				'l' // MCU is alive
#define MCU_CHAR_SI_ENABLE			'I' // CPU instructs MCU to enable sample skip/insert on SPDIF/TOSLINK reception. NOT IMPLEMENTED!
#define MCU_CHAR_SI_DISABLE			'i' // CPU instructs MCU to disable sample skip/insert on SPDIF/TOSLINK reception. NOT IMPLEMENTED! - Probably entails wm8804_CLKOUT(WM8804_CLKOUT_ENABLE)
#define MCU_CHAR_RESET				'R'	// CPU resets MCU over UART
#define MCU_CHAR_ALIVE				'L' // CPU asks MCU if it is alive
#define MCU_CHAR_SPRATE				's' // CPU asks MCU about sample rate on SPDIF/TOSLINK receiver - response is rate in Hz as 32-bit signed, then \n
#define MCU_CHAR_FBRATE				'f' // CPU asks MCU about feedback sample rate in USB playback - response is rate in kHz left-shifted 14 positions, as 32-bit signed, then \n
#define MCU_CHAR_RATEUP				'U' // CPU asks MCU to increase feedback rate by 64 = FB_RATE_DELTA
#define MCU_CHAR_RATEDOWN			'u' // CPU asks MCU to increase feedback rate by 64 = FB_RATE_DELTA

// New code for adaptive USB fallback using skip / insert s/i
#define SI_SKIP						-1
#define SI_NORMAL					0
#define SI_INSERT					1
#define RATE_STORE					0
#define RATE_RETRIEVE				1
#define RATE_ALL_INIT				2
#define RATE_CH_INIT				3
#define RATE_PRINT					4
#define RATE_INVALID				0x3F
#define RATE_FREQUENCIES			6 // The number of relevant frequencies
#define RATE_SOURCES				5 // The number of relevant sources


#ifdef HW_GEN_SPRX
// USB multiplexer definitions
#define USB_DATA_ENABLE_PIN_INV		AVR32_PIN_PA31		// USB_OE0, inverted MUX output enable pin
#define USB_DATA_C0_B1_PIN			AVR32_PIN_PA02		// USB_C0_B1, MUX address control
#define USB_VBUS_C_PIN				AVR32_PIN_PB06		// Active high enable A's VBUS
#define USB_VBUS_B_PIN				AVR32_PIN_PA28		// Active high enable B's VBUS
#define	MOBO_I2S_ENABLE				1
#define MOBO_I2S_DISABLE			0
// RXMODFIX IO is available on TP, currently not in use
/*
#define MOBO_HP_KM_ENABLE			1					// Enable power to headphones
#define MOBO_HP_KM_DISABLE			0					// Disable power to headphones

void mobo_km(uint8_t enable);
*/

// Control USB multiplexer in HW_GEN_SPRX
void mobo_usb_select(uint8_t usb_ch);

// Quick and dirty detect of whether front USB (A) is plugged in. No debounce here!
uint8_t mobo_usb_detect(void);
#endif

// Generic I2C functionality
#if (defined HW_GEN_SPRX) || (defined HW_GEN_FMADC)
int8_t mobo_i2c_read (uint8_t *data, uint8_t device_address, uint8_t internal_address);
int8_t mobo_i2c_write (uint8_t device_address, uint8_t internal_address, uint8_t data);
#endif
	
// Sensors and actuators on FM ADC board
#ifdef HW_GEN_FMADC
void mobo_pcm1863_init(void);
uint8_t mobo_fmadc_gain(uint8_t channel, uint8_t gain);
#define FMADC_MINGAIN	0
#define FMADC_MAXGAIN	3
#define FMADC_REPORT	0xff
#define FMADC_ERROR_CH	0xfe
#define FMADC_ERROR_G1	0xfd
#define FMADC_ERROR_G2	0xfc
#endif


#ifdef HW_GEN_AB1X
// Front panel RG LED control
void mobo_led(uint8_t fled);
#endif


#ifdef HW_GEN_SPRX
// Store and retrieve the relative frequency shift of the source
int8_t mobo_rate_storage(U32 frequency, uint8_t source, int8_t state, uint8_t mode);

// LED control
void mobo_led(uint8_t fled0);
#endif


#if (defined  HW_GEN_SPRX)
// RXmod SPDIF mux control
void mobo_SPRX_input(uint8_t input_sel);
#endif

// Sample rate detection on ADC interface
uint32_t mobo_srd(void);
uint32_t mobo_srd_asm2(void);
uint32_t mobo_wait_LRCK_RX_asm(void);
uint32_t mobo_wait_LRCK_TX_asm(void);

#ifdef HW_GEN_SPRX
// Process spdif and toslink inputs
void mobo_handle_spdif(U32 *si_index_low, S32 *si_score_high, U32 *si_index_high, S32 *num_samples, Bool *cache_holds_silence);

// Start the timer/counter that monitors spdif traffic
void mobo_start_spdif_tc(U32 frequency);

// Stop the timer/counter that monitors spdif traffic
void mobo_stop_spdif_tc(void);

// Front panel RGB LED control
void mobo_led_select(U32 frequency, uint8_t source);

// I2S hard mute control
void  mobo_i2s_enable(uint8_t i2s_mode);

#endif // HW_GEN_SPRX




//
//-----------------------------------------------------------------------------
// Implementation dependent definitions (user tweak stuff)
//-----------------------------------------------------------------------------
//

#define VERSION_MAJOR 16
#define VERSION_MINOR 100

// EEPROM settings Serial Number. Increment this number when firmware mods necessitate
// fresh "Factory Default Settings" to be forced into the EEPROM (NVRAM)at first boot after
// an upgrade
#define COLDSTART_REF		0x0a

// DEFS for PA BIAS selection and autocalibration
#define	BIAS_SELECT			1		// Which bias, 0 = Cal, 1 = AB, 2 = A
									// If BIAS_SELECT is set at 0, then the first operation
									// after any Reset will be to autocalibrate
#define	BIAS_LO				2		// PA Bias in 10 * mA, Class B ( 2 =  20mA)
#define	BIAS_HI				35		// PA Bias in 10 * mA, Class A  (35 = 350mA)
#define	BIAS_MAX			100		// Max allowable PA Bias in 10 * mA, Class A  (100 = 1A)
#define	CAL_LO				0		// PA Bias setting, Class B
#define	CAL_HI				0		// PA Bias setting, Class A

// Defs for Power and SWR measurement (values adjustable through USB command 0x44)
#define SWR_TRIGGER			30				// Max SWR threshold (10 x SWR, 30 = 3.0)
#define SWR_PROTECT_TIMER	200				// Timer loop value in increments of 10ms
#define P_MIN_TRIGGER 		49				// Min P out in mW for SWR trigger
#define V_MIN_TRIGGER		0x20			// Min Vin in 1/4096 Full Scale, for valid SWR measurement
											// (SWR = 1.0 if lower values measured)
#define PWR_CALIBRATE		1000			// Power meter calibration value
#define PEP_MAX_PERIOD		20				// Max Time period for PEP measurement (in 100ms)
#define PEP_PERIOD			10				// Time period for PEP measurement (in 100ms)
											// PEP_PERIOD is used with PWR_PEAK_ENVELOPE func
											// under LCD display.

#define PWR_FULL_SCALE		4				// Bargraph Power fullscale in Watts
#define SWR_FULL_SCALE		4				// Bargraph SWR fullscale: Max SWR = Value + 1 (4 = 5.0:1)

#define	HI_TMP_TRIGGER		55				// If measured PA temperature goes above this point
											// then disable transmission. Value is in deg C
											// even if the LCD is set to display temp in deg F

#define RTC_COUNTER_MAX		1150000			// Max count for the 115kHz Real Time Counter (10 seconds)
#define RTC_COUNTER_FREQ	115000			// Nominal frequency of Real Time Counter in Hz



//
//-----------------------------------------------------------------------------
// Miscellaneous software defines, functions and variables
//-----------------------------------------------------------------------------
//


// Conditional def based on the above, do not touch:
#if PCF_LPF
#define TXF  8
#elif PCF_16LPF
#define TXF  16
#elif PCF_FILTER_IO
#define TXF  8
#elif M0RZF_FILTER_IO
#define TXF  4
#else
#define TXF  16
#endif

// Various flags, may be moved around
extern volatile bool MENU_mode;				// LCD Menu mode.  Owned by taskPushButtonMenu, used by all LCD users
extern bool	SWR_alarm;						// SWR alarm condition
extern bool	TMP_alarm;						// Temperature alarm condition
extern bool	PA_cal_lo;						// Used by PA Bias auto adjust routine
extern bool	PA_cal_hi;						// Used by PA Bias auto adjust routine
extern bool	PA_cal;							// Indicates PA Bias auto adjust in progress

#define	_2(x)		((uint32_t)1<<(x))		// Macro: Take power of 2



#endif /* MOBO_CONFIG_H_ */
