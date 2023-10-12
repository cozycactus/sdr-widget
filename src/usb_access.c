#include "gpio.h"
#include <avr32/io.h>
#include "compiler.h"
#include <stdint.h>
#include "usb_drv.h"


//! GPIO module instance.
#define GPIO  AVR32_GPIO


uint32_t test(void) {
    uint8_t a = 0;
	uint32_t b[20];
	
	for (a = 0; a<20; a++) {
		b[a] = Usb_read_endpoint_data(2, 32);
	}
	
	
	return b[4];
}


/* Drafting a sample rate detector. Compile with something like this:
 * http://www.delorie.com/djgpp/v2faq/faq8_20.html
 *
 * Latest success was with:
 * /cygdrive/c/Program\ Files\ \(x86\)/Atmel/AVR\ Tools/AVR\ Toolchain/bin/avr32-gcc.exe -DFEATURE_BOARD_DEFAULT=feature_board_usbi2s -DFEATURE_IMAGE_DEFAULT=feature_image_uac1_audio -DFEATURE_IN_DEFAULT=feature_in_normal -DFEATURE_OUT_DEFAULT=feature_out_normal -DFEATURE_ADC_DEFAULT=feature_adc_none -DFEATURE_DAC_DEFAULT=feature_dac_generic -DFEATURE_LCD_DEFAULT=feature_lcd_hd44780 -DFEATURE_LOG_DEFAULT=feature_log_500ms -DFEATURE_FILTER_DEFAULT=feature_filter_fir -DFEATURE_QUIRK_DEFAULT=feature_quirk_none -DUSB_STATE_MACHINE_DEBUG -DHW_GEN_DIN10 -DFEATURE_PRODUCT_AB1x -DBOARD=SDRwdgtLite -DFREERTOS_USED -I../src/SOFTWARE_FRAMEWORK/DRIVERS/SSC/I2S -I../src/SOFTWARE_FRAMEWORK/DRIVERS/PDCA -I../src/SOFTWARE_FRAMEWORK/DRIVERS/TWIM -I../src/SOFTWARE_FRAMEWORK/UTILS/DEBUG -I../src/SOFTWARE_FRAMEWORK/SERVICES/USB/CLASS/AUDIO -I../src/SOFTWARE_FRAMEWORK/SERVICES/USB/CLASS/CDC -I../src/SOFTWARE_FRAMEWORK/SERVICES/FREERTOS/Source/portable/GCC/AVR32_UC3 -I../src/SOFTWARE_FRAMEWORK/SERVICES/FREERTOS/Source/include -I../src/SOFTWARE_FRAMEWORK/SERVICES/USB/CLASS/HID -I../src/SOFTWARE_FRAMEWORK/SERVICES/USB -I../src/CONFIG -I../src/SOFTWARE_FRAMEWORK/DRIVERS/USBB/ENUM/DEVICE -I../src/SOFTWARE_FRAMEWORK/DRIVERS/USBB/ENUM -I../src/SOFTWARE_FRAMEWORK/DRIVERS/USBB -I../src/SOFTWARE_FRAMEWORK/DRIVERS/USART -I../src/SOFTWARE_FRAMEWORK/DRIVERS/TC -I../src/SOFTWARE_FRAMEWORK/DRIVERS/WDT -I../src/SOFTWARE_FRAMEWORK/DRIVERS/CPU/CYCLE_COUNTER -I../src/SOFTWARE_FRAMEWORK/DRIVERS/EIC -I../src/SOFTWARE_FRAMEWORK/DRIVERS/RTC -I../src/SOFTWARE_FRAMEWORK/DRIVERS/PM -I../src/SOFTWARE_FRAMEWORK/DRIVERS/GPIO -I../src/SOFTWARE_FRAMEWORK/DRIVERS/FLASHC -I../src/SOFTWARE_FRAMEWORK/UTILS/LIBS/NEWLIB_ADDONS/INCLUDE -I../src/SOFTWARE_FRAMEWORK/UTILS/PREPROCESSOR -I../src/SOFTWARE_FRAMEWORK/UTILS -I../src/SOFTWARE_FRAMEWORK/DRIVERS/INTC -I../src/SOFTWARE_FRAMEWORK/BOARDS -I../src -O2 -fdata-sections -Wall -c -fmessage-length=0 -mpart=uc3a3256 -ffunction-sections -masm-addr-pseudos -MMD -c -g -O2 -Wa,-a,-ad srd_test.c > srd_test.lst
 *
 * gives:
 * avr32-gcc -DFEATURE_BOARD_DEFAULT=feature_board_usbi2s -DFEATURE_IMAGE_DEFAULT=feature_image_uac1_audio -DFEATURE_IN_DEFAULT=feature_in_normal -DFEATURE_OUT_DEFAULT=feature_out_normal -DFEATURE_ADC_DEFAULT=feature_adc_none -DFEATURE_DAC_DEFAULT=feature_dac_generic -DFEATURE_LCD_DEFAULT=feature_lcd_hd44780 -DFEATURE_LOG_DEFAULT=feature_log_500ms -DFEATURE_FILTER_DEFAULT=feature_filter_fir -DFEATURE_QUIRK_DEFAULT=feature_quirk_none -DUSB_STATE_MACHINE_DEBUG -DHW_GEN_DIN10 -DFEATURE_PRODUCT_AB1x -DBOARD=SDRwdgtLite -DFREERTOS_USED -I../src/SOFTWARE_FRAMEWORK/DRIVERS/SSC/I2S -I../src/SOFTWARE_FRAMEWORK/DRIVERS/PDCA -I../src/SOFTWARE_FRAMEWORK/DRIVERS/TWIM -I../src/SOFTWARE_FRAMEWORK/UTILS/DEBUG -I../src/SOFTWARE_FRAMEWORK/SERVICES/USB/CLASS/AUDIO -I../src/SOFTWARE_FRAMEWORK/SERVICES/USB/CLASS/CDC -I../src/SOFTWARE_FRAMEWORK/SERVICES/FREERTOS/Source/portable/GCC/AVR32_UC3 -I../src/SOFTWARE_FRAMEWORK/SERVICES/FREERTOS/Source/include -I../src/SOFTWARE_FRAMEWORK/SERVICES/USB/CLASS/HID -I../src/SOFTWARE_FRAMEWORK/SERVICES/USB -I../src/CONFIG -I../src/SOFTWARE_FRAMEWORK/DRIVERS/USBB/ENUM/DEVICE -I../src/SOFTWARE_FRAMEWORK/DRIVERS/USBB/ENUM -I../src/SOFTWARE_FRAMEWORK/DRIVERS/USBB -I../src/SOFTWARE_FRAMEWORK/DRIVERS/USART -I../src/SOFTWARE_FRAMEWORK/DRIVERS/TC -I../src/SOFTWARE_FRAMEWORK/DRIVERS/WDT -I../src/SOFTWARE_FRAMEWORK/DRIVERS/CPU/CYCLE_COUNTER -I../src/SOFTWARE_FRAMEWORK/DRIVERS/EIC -I../src/SOFTWARE_FRAMEWORK/DRIVERS/RTC -I../src/SOFTWARE_FRAMEWORK/DRIVERS/PM -I../src/SOFTWARE_FRAMEWORK/DRIVERS/GPIO -I../src/SOFTWARE_FRAMEWORK/DRIVERS/FLASHC -I../src/SOFTWARE_FRAMEWORK/UTILS/LIBS/NEWLIB_ADDONS/INCLUDE -I../src/SOFTWARE_FRAMEWORK/UTILS/PREPROCESSOR -I../src/SOFTWARE_FRAMEWORK/UTILS -I../src/SOFTWARE_FRAMEWORK/DRIVERS/INTC -I../src/SOFTWARE_FRAMEWORK/BOARDS -I../src -O2 -fdata-sections -Wall -c -fmessage-length=0 -mpart=uc3a3256 -ffunction-sections -masm-addr-pseudos -MMD -c -g -O2 -Wa,-a,-ad srd_test.c > srd_test.lst
 *
 * Alternatively:
 * avr32-gcc -DFEATURE_BOARD_DEFAULT=feature_board_usbi2s -DFEATURE_IMAGE_DEFAULT=feature_image_uac1_audio -DFEATURE_IN_DEFAULT=feature_in_normal -DFEATURE_OUT_DEFAULT=feature_out_normal -DFEATURE_ADC_DEFAULT=feature_adc_none -DFEATURE_DAC_DEFAULT=feature_dac_generic -DFEATURE_LCD_DEFAULT=feature_lcd_hd44780 -DFEATURE_LOG_DEFAULT=feature_log_500ms -DFEATURE_FILTER_DEFAULT=feature_filter_fir -DFEATURE_QUIRK_DEFAULT=feature_quirk_none -DUSB_STATE_MACHINE_DEBUG -DHW_GEN_DIN10 -DFEATURE_PRODUCT_AB1x -DBOARD=SDRwdgtLite -DFREERTOS_USED -I../src/SOFTWARE_FRAMEWORK/DRIVERS/SSC/I2S -I../src/SOFTWARE_FRAMEWORK/DRIVERS/PDCA -I../src/SOFTWARE_FRAMEWORK/DRIVERS/TWIM -I../src/SOFTWARE_FRAMEWORK/UTILS/DEBUG -I../src/SOFTWARE_FRAMEWORK/SERVICES/USB/CLASS/AUDIO -I../src/SOFTWARE_FRAMEWORK/SERVICES/USB/CLASS/CDC -I../src/SOFTWARE_FRAMEWORK/SERVICES/FREERTOS/Source/portable/GCC/AVR32_UC3 -I../src/SOFTWARE_FRAMEWORK/SERVICES/FREERTOS/Source/include -I../src/SOFTWARE_FRAMEWORK/SERVICES/USB/CLASS/HID -I../src/SOFTWARE_FRAMEWORK/SERVICES/USB -I../src/CONFIG -I../src/SOFTWARE_FRAMEWORK/DRIVERS/USBB/ENUM/DEVICE -I../src/SOFTWARE_FRAMEWORK/DRIVERS/USBB/ENUM -I../src/SOFTWARE_FRAMEWORK/DRIVERS/USBB -I../src/SOFTWARE_FRAMEWORK/DRIVERS/USART -I../src/SOFTWARE_FRAMEWORK/DRIVERS/TC -I../src/SOFTWARE_FRAMEWORK/DRIVERS/WDT -I../src/SOFTWARE_FRAMEWORK/DRIVERS/CPU/CYCLE_COUNTER -I../src/SOFTWARE_FRAMEWORK/DRIVERS/EIC -I../src/SOFTWARE_FRAMEWORK/DRIVERS/RTC -I../src/SOFTWARE_FRAMEWORK/DRIVERS/PM -I../src/SOFTWARE_FRAMEWORK/DRIVERS/GPIO -I../src/SOFTWARE_FRAMEWORK/DRIVERS/FLASHC -I../src/SOFTWARE_FRAMEWORK/UTILS/LIBS/NEWLIB_ADDONS/INCLUDE -I../src/SOFTWARE_FRAMEWORK/UTILS/PREPROCESSOR -I../src/SOFTWARE_FRAMEWORK/UTILS -I../src/SOFTWARE_FRAMEWORK/DRIVERS/INTC -I../src/SOFTWARE_FRAMEWORK/BOARDS -I../src -O2 -fdata-sections -Wall -c -fmessage-length=0 -mpart=uc3a3256 -ffunction-sections -masm-addr-pseudos -MMD -S -fverbose-asm -g -O2 srd_test.c
 * avr32-gcc -DFEATURE_BOARD_DEFAULT=feature_board_usbi2s -DFEATURE_IMAGE_DEFAULT=feature_image_uac1_audio -DFEATURE_IN_DEFAULT=feature_in_normal -DFEATURE_OUT_DEFAULT=feature_out_normal -DFEATURE_ADC_DEFAULT=feature_adc_none -DFEATURE_DAC_DEFAULT=feature_dac_generic -DFEATURE_LCD_DEFAULT=feature_lcd_hd44780 -DFEATURE_LOG_DEFAULT=feature_log_500ms -DFEATURE_FILTER_DEFAULT=feature_filter_fir -DFEATURE_QUIRK_DEFAULT=feature_quirk_none -DUSB_STATE_MACHINE_DEBUG -DHW_GEN_DIN10 -DFEATURE_PRODUCT_AB1x -DBOARD=SDRwdgtLite -DFREERTOS_USED -I../src/SOFTWARE_FRAMEWORK/DRIVERS/SSC/I2S -I../src/SOFTWARE_FRAMEWORK/DRIVERS/PDCA -I../src/SOFTWARE_FRAMEWORK/DRIVERS/TWIM -I../src/SOFTWARE_FRAMEWORK/UTILS/DEBUG -I../src/SOFTWARE_FRAMEWORK/SERVICES/USB/CLASS/AUDIO -I../src/SOFTWARE_FRAMEWORK/SERVICES/USB/CLASS/CDC -I../src/SOFTWARE_FRAMEWORK/SERVICES/FREERTOS/Source/portable/GCC/AVR32_UC3 -I../src/SOFTWARE_FRAMEWORK/SERVICES/FREERTOS/Source/include -I../src/SOFTWARE_FRAMEWORK/SERVICES/USB/CLASS/HID -I../src/SOFTWARE_FRAMEWORK/SERVICES/USB -I../src/CONFIG -I../src/SOFTWARE_FRAMEWORK/DRIVERS/USBB/ENUM/DEVICE -I../src/SOFTWARE_FRAMEWORK/DRIVERS/USBB/ENUM -I../src/SOFTWARE_FRAMEWORK/DRIVERS/USBB -I../src/SOFTWARE_FRAMEWORK/DRIVERS/USART -I../src/SOFTWARE_FRAMEWORK/DRIVERS/TC -I../src/SOFTWARE_FRAMEWORK/DRIVERS/WDT -I../src/SOFTWARE_FRAMEWORK/DRIVERS/CPU/CYCLE_COUNTER -I../src/SOFTWARE_FRAMEWORK/DRIVERS/EIC -I../src/SOFTWARE_FRAMEWORK/DRIVERS/RTC -I../src/SOFTWARE_FRAMEWORK/DRIVERS/PM -I../src/SOFTWARE_FRAMEWORK/DRIVERS/GPIO -I../src/SOFTWARE_FRAMEWORK/DRIVERS/FLASHC -I../src/SOFTWARE_FRAMEWORK/UTILS/LIBS/NEWLIB_ADDONS/INCLUDE -I../src/SOFTWARE_FRAMEWORK/UTILS/PREPROCESSOR -I../src/SOFTWARE_FRAMEWORK/UTILS -I../src/SOFTWARE_FRAMEWORK/DRIVERS/INTC -I../src/SOFTWARE_FRAMEWORK/BOARDS -I../src -O2 -fdata-sections -Wall -c -fmessage-length=0 -mpart=uc3a3256 -ffunction-sections -masm-addr-pseudos -MMD -S -g -O2 srd_test.c
 *
 * A good asm syntax list:
 * http://www.ibiblio.org/gferg/ldp/GCC-Inline-Assembly-HOWTO.html
 */
