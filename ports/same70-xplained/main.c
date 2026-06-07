#include "types.h"
#include "same70_clock.h"
#include "same70_usb.h"

#define REG32(addr) (*(volatile uint32_t *)(addr))

#define PMC_PCER0 REG32(0x400E0610u)
#define WDT_MR    REG32(0x400E1854u)

#define MATRIX_CCFG_SYSIO REG32(0x40088114u)

#define PIOA_BASE 0x400E0E00u
#define PIOB_BASE 0x400E1000u
#define PIOC_BASE 0x400E1200u

#define PIO_PER(base)      REG32((base) + 0x0000u)
#define PIO_PDR(base)      REG32((base) + 0x0004u)
#define PIO_OER(base)      REG32((base) + 0x0010u)
#define PIO_PUER(base)     REG32((base) + 0x0064u)
#define PIO_ABCDSR1(base)  REG32((base) + 0x0070u)
#define PIO_ABCDSR2(base)  REG32((base) + 0x0074u)
#define PIO_SODR(base)     REG32((base) + 0x0030u)
#define PIO_CODR(base)     REG32((base) + 0x0034u)

#define USART1_BASE 0x40028000u
#define US_CR       REG32(USART1_BASE + 0x0000u)
#define US_MR       REG32(USART1_BASE + 0x0004u)
#define US_IER      REG32(USART1_BASE + 0x0008u)
#define US_IDR      REG32(USART1_BASE + 0x000Cu)
#define US_CSR      REG32(USART1_BASE + 0x0014u)
#define US_RHR      REG32(USART1_BASE + 0x0018u)
#define US_THR      REG32(USART1_BASE + 0x001Cu)
#define US_BRGR     REG32(USART1_BASE + 0x0020u)

#define NVIC_ISER0  REG32(0xE000E100u)
#define NVIC_ICPR0  REG32(0xE000E280u)

#define SYST_CSR    REG32(0xE000E010u)
#define SYST_RVR    REG32(0xE000E014u)
#define SYST_CVR    REG32(0xE000E018u)

#define ID_PIOA     10u
#define ID_PIOB     11u
#define ID_PIOC     12u
#define ID_USART1   14u

#define LED0_PC8    (1u << 8)
#define RXD1_PA21   (1u << 21)
#define TXD1_PB4    (1u << 4)

#define SYSIO_PB4   (1u << 4)

#define WDT_WDDIS   (1u << 15)

#define US_CR_RSTRX (1u << 2)
#define US_CR_RSTTX (1u << 3)
#define US_CR_RXEN  (1u << 4)
#define US_CR_RXDIS (1u << 5)
#define US_CR_TXEN  (1u << 6)
#define US_CR_TXDIS (1u << 7)
#define US_CR_RSTSTA (1u << 8)

#define US_MR_USART_MODE_NORMAL (0u << 0)
#define US_MR_USCLKS_MCK        (0u << 4)
#define US_MR_CHRL_8_BIT        (3u << 6)
#define US_MR_PAR_NO            (4u << 9)
#define US_MR_NBSTOP_1_BIT      (0u << 12)
#define US_MR_CHMODE_NORMAL     (0u << 14)

#define US_CSR_RXRDY  (1u << 0)
#define US_CSR_TXRDY  (1u << 1)
#define US_CSR_OVRE   (1u << 5)
#define US_CSR_FRAME  (1u << 6)
#define US_CSR_PARE   (1u << 7)

#define USART_BAUD_DIV_9600 ((SAME70_MASTER_CLOCK_HZ + 76800u) / 153600u)
#define SYSTICK_HZ 1000u
#define CONSOLE_LINE_MAX 32u
#define RX_RING_SIZE 64u

static void usart1_drain_rx(void);
static void console_poll(void);
static void console_write_hex_field(const char *name, uint32_t value);
static void console_clock_status(void);
static void console_audio_source_name(uint32_t source);
static void console_audio_format_name(uint32_t format);

static volatile uint32_t system_ms;
static uint32_t tick_count;
static char rx_ring[RX_RING_SIZE];
static volatile uint32_t rx_head;
static volatile uint32_t rx_tail;
static char console_line[CONSOLE_LINE_MAX];
static uint32_t console_line_len;

static void led_init(void)
{
	PIO_PER(PIOC_BASE) = LED0_PC8;
	PIO_OER(PIOC_BASE) = LED0_PC8;
	PIO_SODR(PIOC_BASE) = LED0_PC8;
}

static void led_on(void)
{
	PIO_CODR(PIOC_BASE) = LED0_PC8;
}

static void led_off(void)
{
	PIO_SODR(PIOC_BASE) = LED0_PC8;
}

static void systick_init(void)
{
	SYST_RVR = (SAME70_CPU_CLOCK_HZ / SYSTICK_HZ) - 1u;
	SYST_CVR = 0u;
	SYST_CSR = 0x7u;
}

static void usart1_init(void)
{
	PMC_PCER0 = (1u << ID_PIOA) | (1u << ID_PIOB) | (1u << ID_USART1);

	/* PB4 resets as JTAG TDI; select the normal PB4 pin before TXD1 muxing. */
	MATRIX_CCFG_SYSIO |= SYSIO_PB4;

	/* PA21 is RXD1 on peripheral A; PB4 is TXD1 on peripheral D. */
	PIO_ABCDSR1(PIOA_BASE) &= ~RXD1_PA21;
	PIO_ABCDSR2(PIOA_BASE) &= ~RXD1_PA21;
	PIO_PDR(PIOA_BASE) = RXD1_PA21;
	PIO_PUER(PIOA_BASE) = RXD1_PA21;

	PIO_ABCDSR1(PIOB_BASE) |= TXD1_PB4;
	PIO_ABCDSR2(PIOB_BASE) |= TXD1_PB4;
	PIO_PDR(PIOB_BASE) = TXD1_PB4;

	US_CR = US_CR_RXDIS | US_CR_TXDIS | US_CR_RSTRX | US_CR_RSTTX | US_CR_RSTSTA;
	US_IDR = 0xffffffffu;
	(void)US_RHR;

	US_MR = US_MR_USART_MODE_NORMAL |
		US_MR_USCLKS_MCK |
		US_MR_CHRL_8_BIT |
		US_MR_PAR_NO |
		US_MR_NBSTOP_1_BIT |
		US_MR_CHMODE_NORMAL;
	US_BRGR = USART_BAUD_DIV_9600;
	US_CR = US_CR_RXEN | US_CR_TXEN;
	NVIC_ICPR0 = (1u << ID_USART1);
	NVIC_ISER0 = (1u << ID_USART1);
	US_IER = US_CSR_RXRDY | US_CSR_OVRE | US_CSR_FRAME | US_CSR_PARE;
}

static void usart1_putc(char c)
{
	while ((US_CSR & US_CSR_TXRDY) == 0u) {
	}
	US_THR = (uint32_t)c;
}

static void usart1_write(const char *text)
{
	while (*text != '\0') {
		usart1_putc(*text++);
	}
}

static void usart1_write_u32(uint32_t value)
{
	char digits[10];
	uint32_t index = 0;

	if (value == 0u) {
		usart1_putc('0');
		return;
	}

	while (value != 0u) {
		digits[index++] = (char)('0' + (value % 10u));
		value /= 10u;
	}

	while (index != 0u) {
		usart1_putc(digits[--index]);
	}
}

static void usart1_write_hex32(uint32_t value)
{
	uint32_t index;

	usart1_write("0x");
	for (index = 0u; index < 8u; index++) {
		uint32_t shift = 28u - (index * 4u);
		uint32_t nibble = (value >> shift) & 0xfu;
		usart1_putc((char)((nibble < 10u) ? ('0' + nibble) : ('a' + (nibble - 10u))));
	}
}

static uint32_t text_equals(const char *a, const char *b)
{
	while ((*a != '\0') && (*b != '\0')) {
		if (*a++ != *b++) {
			return 0u;
		}
	}

	return (*a == '\0') && (*b == '\0');
}

static void console_write_hex_field(const char *name, uint32_t value)
{
	usart1_write(name);
	usart1_write("=");
	usart1_write_hex32(value);
	usart1_write(" ");
}

static void console_usb_status(void)
{
	same70_usb_status_t status;

	same70_usb_get_status(&status);
	usart1_write("usb init=");
	usart1_write_u32(status.initialized);
	usart1_write(" attached=");
	usart1_write_u32(status.attached);
	usart1_write("\r\n");
	console_write_hex_field("pmc_sr", status.pmc_sr);
	console_write_hex_field("pmc_usb", status.pmc_usb);
	console_write_hex_field("pcsr1", status.pmc_pcsr1);
	usart1_write("\r\n");
	console_write_hex_field("ctrl", status.usbhs_ctrl);
	console_write_hex_field("sr", status.usbhs_sr);
	console_write_hex_field("devctrl", status.devctrl);
	usart1_write("\r\n");
	console_write_hex_field("devisr", status.devisr);
	console_write_hex_field("devept", status.devept);
	usart1_write("\r\n");
	console_write_hex_field("ep0cfg", status.ep0cfg);
	console_write_hex_field("ep0isr", status.ep0isr);
	console_write_hex_field("ep0imr", status.ep0imr);
	usart1_write("\r\n");
	console_write_hex_field("ep3cfg", status.ep3cfg);
	console_write_hex_field("ep3isr", status.ep3isr);
	usart1_write("\r\n");
	console_write_hex_field("ep4cfg", status.ep4cfg);
	console_write_hex_field("ep4isr", status.ep4isr);
	usart1_write("\r\n");
	console_write_hex_field("ep5cfg", status.ep5cfg);
	console_write_hex_field("ep5isr", status.ep5isr);
	usart1_write("\r\n");
	usart1_write("events reset=");
	usart1_write_u32(status.reset_count);
	usart1_write(" setup=");
	usart1_write_u32(status.setup_count);
	usart1_write(" tx=");
	usart1_write_u32(status.tx_count);
	usart1_write(" rxout=");
	usart1_write_u32(status.rxout_count);
	usart1_write(" stall=");
	usart1_write_u32(status.stall_count);
	usart1_write("\r\n");
	usart1_write("ctrl address=");
	usart1_write_u32(status.address);
	usart1_write(" config=");
	usart1_write_u32(status.configuration);
	usart1_write(" ep0_state=");
	usart1_write_u32(status.ep0_state);
	usart1_write(" desc=");
	usart1_write_u32(status.descriptor_count);
	usart1_write(" set_addr=");
	usart1_write_u32(status.set_address_count);
	usart1_write(" set_cfg=");
	usart1_write_u32(status.set_configuration_count);
	usart1_write(" set_int=");
	usart1_write_u32(status.set_interface_count);
	usart1_write(" alt=");
	usart1_write_hex32(status.interface_alternate_mask);
	usart1_write(" peak_alt=");
	usart1_write_hex32(status.interface_alternate_peak_mask);
	usart1_write(" last_int=");
	usart1_write_u32(status.last_set_interface_index);
	usart1_write(":");
	usart1_write_u32(status.last_set_interface_value);
	usart1_write("\r\n");
	usart1_write("audio cfg=");
	usart1_write_u32(status.audio_config_count);
	usart1_write(" set_int=");
	usart1_write_u32(status.audio_set_interface_count);
	usart1_write(" cfgok=");
	usart1_write_hex32(status.audio_cfgok_mask);
	usart1_write(" out=");
	usart1_write_u32(status.audio_out_count);
	usart1_write("/");
	usart1_write_u32(status.audio_out_bytes);
	usart1_write(" fb=");
	usart1_write_u32(status.audio_feedback_count);
	usart1_write("/");
	usart1_write_u32(status.audio_feedback_bytes);
	usart1_write(" in=");
	usart1_write_u32(status.audio_in_count);
	usart1_write("/");
	usart1_write_u32(status.audio_in_bytes);
	usart1_write(" err=");
	usart1_write_u32(status.audio_error_count);
	usart1_write("\r\n");
	usart1_write("audio last_out=");
	usart1_write_u32(status.audio_out_last_bytes);
	usart1_write(" max_out=");
	usart1_write_u32(status.audio_out_max_bytes);
	usart1_write(" outnz=");
	usart1_write_u32(status.audio_out_nonzero_bytes);
	usart1_write(" short=");
	usart1_write_u32(status.audio_short_count);
	usart1_write(" crc=");
	usart1_write_u32(status.audio_crc_count);
	usart1_write(" over=");
	usart1_write_u32(status.audio_overflow_count);
	usart1_write(" under=");
	usart1_write_u32(status.audio_underflow_count);
	usart1_write(" fb_busy=");
	usart1_write_u32(status.audio_feedback_busy_last);
	usart1_write("/");
	usart1_write_u32(status.audio_feedback_busy_max);
	usart1_write(" in_busy=");
	usart1_write_u32(status.audio_in_busy_last);
	usart1_write("/");
	usart1_write_u32(status.audio_in_busy_max);
	usart1_write("\r\n");
	usart1_write("audio loop=");
	usart1_write_u32(status.audio_loopback_level);
	usart1_write("/");
	usart1_write_u32(status.audio_loopback_peak);
	usart1_write(" drop=");
	usart1_write_u32(status.audio_loopback_drop_bytes);
	usart1_write(" silence=");
	usart1_write_u32(status.audio_loopback_silence_bytes);
	usart1_write(" source=");
	console_audio_source_name(status.audio_source_mode);
	usart1_write(" fmt=");
	console_audio_format_name(status.audio_out_format);
	usart1_write("/");
	console_audio_format_name(status.audio_in_format);
	usart1_write("\r\n");
	console_write_hex_field("last", status.last_setup0);
	console_write_hex_field("wValue", status.last_wvalue);
	console_write_hex_field("wIndex", status.last_windex);
	console_write_hex_field("wLength", status.last_wlength);
	usart1_write("\r\n");
}

static void console_audio_source_name(uint32_t source)
{
	if (source == SAME70_USB_AUDIO_SOURCE_PATTERN) {
		usart1_write("pattern");
	} else if (source == SAME70_USB_AUDIO_SOURCE_SILENCE) {
		usart1_write("silence");
	} else if (source == SAME70_USB_AUDIO_SOURCE_TONE) {
		usart1_write("tone");
	} else if (source == SAME70_USB_AUDIO_SOURCE_SINE) {
		usart1_write("sine");
	} else {
		usart1_write("loop");
	}
}

static void console_audio_source_status(uint32_t source)
{
	usart1_write("audio source=");
	console_audio_source_name(source);
	usart1_write("\r\n");
}

static void console_audio_format_name(uint32_t format)
{
	if (format == SAME70_USB_AUDIO_FORMAT_44K16) {
		usart1_write("44k16");
	} else {
		usart1_write("48k24");
	}
}

static void console_clock_status(void)
{
	same70_clock_status_t status;

	same70_clock_get_status(&status);
	console_write_hex_field("mor", status.ckgr_mor);
	console_write_hex_field("pllar", status.ckgr_pllar);
	console_write_hex_field("mckr", status.pmc_mckr);
	usart1_write("\r\n");
	console_write_hex_field("uckr", status.ckgr_uckr);
	console_write_hex_field("pmc_usb", status.pmc_usb);
	console_write_hex_field("scsr", status.pmc_scsr);
	console_write_hex_field("sr", status.pmc_sr);
	usart1_write("\r\n");
	console_write_hex_field("utmi", status.utmi_cktrim);
	console_write_hex_field("efc_fmr", status.efc_fmr);
	usart1_write("\r\n");
}

static uint32_t rx_next_index(uint32_t index)
{
	index++;
	if (index >= RX_RING_SIZE) {
		index = 0u;
	}

	return index;
}

static void rx_push(char c)
{
	uint32_t next = rx_next_index(rx_head);

	if (next != rx_tail) {
		rx_ring[rx_head] = c;
		rx_head = next;
	}
}

static uint32_t rx_pop(char *c)
{
	if (rx_tail == rx_head) {
		return 0u;
	}

	*c = rx_ring[rx_tail];
	rx_tail = rx_next_index(rx_tail);
	return 1u;
}

static void usart1_drain_rx(void)
{
	uint32_t status = US_CSR;

	while ((status & US_CSR_RXRDY) != 0u) {
		rx_push((char)(US_RHR & 0xffu));
		status = US_CSR;
	}

	if ((status & (US_CSR_OVRE | US_CSR_FRAME | US_CSR_PARE)) != 0u) {
		US_CR = US_CR_RSTSTA;
	}
}

static void console_prompt(void)
{
	usart1_write("> ");
}

static void console_handle_line(void)
{
	console_line[console_line_len] = '\0';

	if (console_line_len == 0u) {
		console_prompt();
		return;
	}

	if (text_equals(console_line, "?") || text_equals(console_line, "help")) {
		usart1_write("commands: ?, help, status, clk, usb, usb init, usb attach, usb detach, audio loop, audio pattern, audio tone, audio sine, audio silence\r\n");
	} else if (text_equals(console_line, "status")) {
		usart1_write("status tick=");
		usart1_write_u32(tick_count);
		usart1_write(" uptime_ms=");
		usart1_write_u32(system_ms);
		usart1_write(" us_csr=");
		usart1_write_hex32(US_CSR);
		usart1_write("\r\n");
	} else if (text_equals(console_line, "clk")) {
		console_clock_status();
	} else if (text_equals(console_line, "usb")) {
		console_usb_status();
	} else if (text_equals(console_line, "usb init")) {
		same70_usb_init();
		console_usb_status();
	} else if (text_equals(console_line, "usb attach")) {
		same70_usb_attach();
		console_usb_status();
	} else if (text_equals(console_line, "usb detach")) {
		same70_usb_detach();
		console_usb_status();
	} else if (text_equals(console_line, "audio loop")) {
		(void)same70_usb_set_audio_source(SAME70_USB_AUDIO_SOURCE_LOOPBACK);
		console_audio_source_status(SAME70_USB_AUDIO_SOURCE_LOOPBACK);
	} else if (text_equals(console_line, "audio pattern")) {
		(void)same70_usb_set_audio_source(SAME70_USB_AUDIO_SOURCE_PATTERN);
		console_audio_source_status(SAME70_USB_AUDIO_SOURCE_PATTERN);
	} else if (text_equals(console_line, "audio tone")) {
		(void)same70_usb_set_audio_source(SAME70_USB_AUDIO_SOURCE_TONE);
		console_audio_source_status(SAME70_USB_AUDIO_SOURCE_TONE);
	} else if (text_equals(console_line, "audio sine")) {
		(void)same70_usb_set_audio_source(SAME70_USB_AUDIO_SOURCE_SINE);
		console_audio_source_status(SAME70_USB_AUDIO_SOURCE_SINE);
	} else if (text_equals(console_line, "audio silence")) {
		(void)same70_usb_set_audio_source(SAME70_USB_AUDIO_SOURCE_SILENCE);
		console_audio_source_status(SAME70_USB_AUDIO_SOURCE_SILENCE);
	} else {
		usart1_write("unknown: ");
		usart1_write(console_line);
		usart1_write("\r\n");
	}

	console_line_len = 0u;
	console_prompt();
}

static void console_receive(char c)
{
	if ((c == '\r') || (c == '\n')) {
		usart1_write("\r\n");
		console_handle_line();
		return;
	}

	if ((c == '\b') || (c == 0x7f)) {
		if (console_line_len != 0u) {
			console_line_len--;
			usart1_write("\b \b");
		}
		return;
	}

	if ((c < ' ') || (c > '~')) {
		return;
	}

	if (console_line_len < (CONSOLE_LINE_MAX - 1u)) {
		console_line[console_line_len++] = c;
		usart1_putc(c);
	}
}

static void console_poll(void)
{
	char c;

	while (rx_pop(&c) != 0u) {
		console_receive(c);
	}
}

void USART1_Handler(void)
{
	usart1_drain_rx();
}

void SysTick_Handler(void)
{
	system_ms++;
}

int main(void)
{
	uint32_t last_blink_ms;
	uint32_t led_state = 0u;

	WDT_MR = WDT_WDDIS;
	same70_clock_init();
	PMC_PCER0 = (1u << ID_PIOC);

	led_init();
	systick_init();
	usart1_init();
	usart1_write("\r\nSAME70 Xplained SDR Widget bring-up\r\n");
	usart1_write("USART1 via EDBG VCOM: 9600 8N1 status output\r\n");
	same70_usb_attach();
	usart1_write("USBHS target port auto-attach enabled\r\n");
	console_prompt();
	last_blink_ms = system_ms;

	for (;;) {
		same70_usb_poll();
		console_poll();
		if ((system_ms - last_blink_ms) >= 500u) {
			last_blink_ms = system_ms;
			if (led_state == 0u) {
				led_on();
				led_state = 1u;
			} else {
				led_off();
				led_state = 0u;
				tick_count++;
			}
		}
	}
}
