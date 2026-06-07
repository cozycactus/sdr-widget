#include "same70_usb.h"

#define REG32(addr) (*(volatile uint32_t *)(addr))

#define PMC_SCER   REG32(0x400E0600u)
#define CKGR_UCKR  REG32(0x400E061Cu)
#define PMC_USB    REG32(0x400E0638u)
#define PMC_SR     REG32(0x400E0668u)
#define PMC_PCER1  REG32(0x400E0700u)
#define PMC_PCSR1  REG32(0x400E0708u)

#define CKGR_UCKR_UPLLEN       (1u << 16)
#define CKGR_UCKR_UPLLCOUNT(x) ((x) << 20)
#define PMC_USB_USBS_UPLL      (1u << 0)
#define PMC_USB_USBDIV(x)      ((x) << 8)
#define PMC_SCER_USBCLK        (1u << 5)
#define PMC_SR_LOCKU           (1u << 6)

#define USBHS_BASE     0x40038000u
#define USBHS_DEVCTRL  REG32(USBHS_BASE + 0x000u)
#define USBHS_DEVISR   REG32(USBHS_BASE + 0x004u)
#define USBHS_DEVICR   REG32(USBHS_BASE + 0x008u)
#define USBHS_DEVIDR   REG32(USBHS_BASE + 0x014u)
#define USBHS_DEVIER   REG32(USBHS_BASE + 0x018u)
#define USBHS_DEVEPT   REG32(USBHS_BASE + 0x01Cu)
#define USBHS_CTRL     REG32(USBHS_BASE + 0x800u)
#define USBHS_SR       REG32(USBHS_BASE + 0x804u)

#define USBHS_DEVEPTCFG(ep) REG32(USBHS_BASE + 0x100u + ((ep) * 4u))
#define USBHS_DEVEPTISR(ep) REG32(USBHS_BASE + 0x130u + ((ep) * 4u))
#define USBHS_DEVEPTICR(ep) REG32(USBHS_BASE + 0x160u + ((ep) * 4u))
#define USBHS_DEVEPTIMR(ep) REG32(USBHS_BASE + 0x1C0u + ((ep) * 4u))
#define USBHS_DEVEPTIER(ep) REG32(USBHS_BASE + 0x1F0u + ((ep) * 4u))
#define USBHS_DEVEPTIDR(ep) REG32(USBHS_BASE + 0x220u + ((ep) * 4u))

#define USBHS_RAM_ADDR 0xA0100000u
#define USBHS_EP_FIFO(ep) ((volatile uint8_t *)(USBHS_RAM_ADDR + ((ep) * 0x8000u)))

#define SCB_AIRCR              REG32(0xE000ED0Cu)
#define SCB_AIRCR_VECTKEY      (0x5fau << 16)
#define SCB_AIRCR_PRIGROUP_Msk (7u << 8)
#define SCB_AIRCR_SYSRESETREQ  (1u << 2)

#define ID_USBHS               34u
#define USBHS_PCER1_BIT        (1u << (ID_USBHS - 32u))

#define USBHS_CTRL_FRZCLK      (1u << 14)
#define USBHS_CTRL_USBE        (1u << 15)
#define USBHS_CTRL_VBUSHWC     (1u << 8)
#define USBHS_CTRL_UIMOD_DEV   (1u << 25)
#define USBHS_SR_CLKUSABLE     (1u << 14)

#define USBHS_DEVCTRL_UADD_MASK 0x7fu
#define USBHS_DEVCTRL_ADDEN    (1u << 7)
#define USBHS_DEVCTRL_DETACH   (1u << 8)
#define USBHS_DEVCTRL_NORMAL   (0u << 10)
#define USBHS_DEVCTRL_FORCED_FS (3u << 10)

#define USBHS_DEVISR_EORST     (1u << 3)
#define USBHS_DEVICR_EORSTC    (1u << 3)
#define USBHS_DEVIDR_ALL       0xfe3ff07fu
#define USBHS_DEVIER_EORSTES   (1u << 3)
#define USBHS_DEVIER_PEP0      (1u << 12)

#define USBHS_DEVEPT_EPEN(ep)  (1u << (ep))
#define USBHS_DEVEPT_EPRST(ep) (1u << (16u + (ep)))
#define USBHS_DEVEPT_EPEN0     USBHS_DEVEPT_EPEN(0u)
#define USBHS_DEVEPT_EPRST0    USBHS_DEVEPT_EPRST(0u)
#define USBHS_DEVEPTCFG_ALLOC  (1u << 1)
#define USBHS_DEVEPTCFG_1_BANK (0u << 2)
#define USBHS_DEVEPTCFG_2_BANK (1u << 2)
#define USBHS_DEVEPTCFG_8B     (0u << 4)
#define USBHS_DEVEPTCFG_64B    (3u << 4)
#define USBHS_DEVEPTCFG_512B   (6u << 4)
#define USBHS_DEVEPTCFG_IN     (1u << 8)
#define USBHS_DEVEPTCFG_CTRL   (0u << 11)
#define USBHS_DEVEPTCFG_ISO    (1u << 11)

#define USBHS_DEVEPTISR_TXINI  (1u << 0)
#define USBHS_DEVEPTISR_RXOUTI (1u << 1)
#define USBHS_DEVEPTISR_RXSTPI (1u << 2)
#define USBHS_DEVEPTISR_UNDERFI (1u << 2)
#define USBHS_DEVEPTISR_HBISOINERRI (1u << 3)
#define USBHS_DEVEPTISR_HBISOFLUSHI (1u << 4)
#define USBHS_DEVEPTISR_OVERFI (1u << 5)
#define USBHS_DEVEPTISR_CRCERRI (1u << 6)
#define USBHS_DEVEPTISR_SHORTPACKETI (1u << 7)
#define USBHS_DEVEPTISR_ERRORTRANS (1u << 10)
#define USBHS_DEVEPTISR_NBUSYBK_MASK (3u << 12)
#define USBHS_DEVEPTISR_NBUSYBK_SHIFT 12u
#define USBHS_DEVEPTISR_RWALL (1u << 16)
#define USBHS_DEVEPTISR_BYCT_MASK (0x7ffu << 20)
#define USBHS_DEVEPTISR_BYCT_SHIFT 20u
#define USBHS_DEVEPTISR_CFGOK  (1u << 18)

#define USBHS_DEVEPTICR_TXINIC  (1u << 0)
#define USBHS_DEVEPTICR_RXOUTIC (1u << 1)
#define USBHS_DEVEPTICR_RXSTPIC (1u << 2)
#define USBHS_DEVEPTICR_UNDERFIC (1u << 2)
#define USBHS_DEVEPTICR_HBISOINERRIC (1u << 3)
#define USBHS_DEVEPTICR_HBISOFLUSHIC (1u << 4)
#define USBHS_DEVEPTICR_OVERFIC (1u << 5)
#define USBHS_DEVEPTICR_CRCERRIC (1u << 6)
#define USBHS_DEVEPTICR_SHORTPACKETIC (1u << 7)

#define USBHS_DEVEPTIER_RXSTPES  (1u << 2)
#define USBHS_DEVEPTIER_RXOUTES  (1u << 1)
#define USBHS_DEVEPTIER_RSTDTS   (1u << 18)
#define USBHS_DEVEPTIER_STALLRQS (1u << 19)

#define USBHS_DEVEPTIDR_FIFOCONC (1u << 14)
#define USBHS_DEVEPTIDR_STALLRQC (1u << 19)

#define USB_REQ_GET_STATUS        0u
#define USB_REQ_CLEAR_FEATURE     1u
#define USB_REQ_SET_ADDRESS       5u
#define USB_REQ_GET_DESCRIPTOR    6u
#define USB_REQ_GET_CONFIGURATION 8u
#define USB_REQ_SET_CONFIGURATION 9u
#define USB_REQ_GET_INTERFACE     10u
#define USB_REQ_SET_INTERFACE     11u

#define AUDIO_REQ_SET_CUR         0x01u
#define AUDIO_REQ_GET_CUR         0x81u
#define AUDIO_REQ_GET_MIN         0x82u
#define AUDIO_REQ_GET_MAX         0x83u
#define AUDIO_REQ_GET_RES         0x84u

#define AUDIO_CONTROL_MUTE        0x01u
#define AUDIO_CONTROL_VOLUME      0x02u
#define AUDIO_CONTROL_SAMPLE_FREQ 0x01u
#define AUDIO_MIC_FEATURE_UNIT    0x02u
#define AUDIO_SPK_FEATURE_UNIT    0x12u

#define WIDGET_REQ_RESET          0x0fu

#define FEATURE_DG8SAQ_COMMAND    0x71u
#define FEATURE_DG8SAQ_SET_NVRAM  3u
#define FEATURE_DG8SAQ_GET_NVRAM  4u
#define FEATURE_DG8SAQ_SET_RAM    5u
#define FEATURE_DG8SAQ_GET_RAM    6u
#define FEATURE_DG8SAQ_GET_INDEX  7u
#define FEATURE_DG8SAQ_GET_VALUE  8u
#define FEATURE_DG8SAQ_GET_DEFAULT 9u

#define FEATURE_MAJOR_INDEX       0u
#define FEATURE_MINOR_INDEX       1u
#define FEATURE_BOARD_INDEX       2u
#define FEATURE_END_INDEX         10u
#define FEATURE_END_VALUES        37u

#define FEATURE_BOARD_WIDGET      1u
#define FEATURE_IMAGE_UAC1_DG8SAQ 8u
#define FEATURE_IN_NORMAL         14u
#define FEATURE_OUT_NORMAL        17u
#define FEATURE_ADC_AK5394A       21u
#define FEATURE_DAC_CS4344        24u
#define FEATURE_LCD_HD44780       28u
#define FEATURE_LOG_500MS         33u

#define USB_DESC_DEVICE        1u
#define USB_DESC_CONFIGURATION 2u
#define USB_DESC_STRING        3u
#define USB_DESC_INTERFACE     4u
#define USB_DESC_DEVICE_QUAL   6u
#define USB_DESC_OTHER_SPEED   7u

#define EP0_SIZE 64u
#define USB_INTERFACE_COUNT 4u
#define USB_AUDIO_OUT_EP 3u
#define USB_AUDIO_FB_EP  4u
#define USB_AUDIO_IN_EP  5u
#define AUDIO_OUT_48K24_MAX_BYTES 294u
#define AUDIO_OUT_44K16_MAX_BYTES 180u
#define AUDIO_IN_48K24_PACKET_BYTES 288u
#define AUDIO_44K16_FRAME_BYTES 4u
#define AUDIO_44K16_FRAMES_PER_10MS 441u
#define AUDIO_44K16_PACKET_DIVISOR 10u
#define AUDIO_LOOPBACK_BUFFER_BYTES 8192u

typedef enum {
	EP0_STATE_IDLE = 0,
	EP0_STATE_TX_DATA = 1,
	EP0_STATE_TX_STATUS = 2,
	EP0_STATE_WAIT_OUT_STATUS = 3,
	EP0_STATE_RX_DATA_STATUS = 4
} ep0_state_t;

typedef struct {
	uint8_t bm_request_type;
	uint8_t request;
	uint16_t value;
	uint16_t index;
	uint16_t length;
} usb_setup_t;

static const uint8_t device_descriptor[] = {
	18, USB_DESC_DEVICE,
	0x00, 0x02,
	0xef, 0x02, 0x01,
	EP0_SIZE,
	0xc0, 0x16,
	0xdc, 0x05,
	0x00, 0x10,
	1, 2, 3,
	1
};

static const uint8_t configuration_descriptor[] = {
	0x09, 0x02, 0x41, 0x01, 0x04, 0x01, 0x00, 0xc0, 0xfa,
	0x09, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x08, 0x0b, 0x01, 0x03, 0x01, 0x01, 0x00, 0x00,
	0x09, 0x04, 0x01, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00,
	0x0a, 0x24, 0x01, 0x00, 0x01, 0x4e, 0x00, 0x02, 0x02, 0x03,
	0x0c, 0x24, 0x02, 0x01, 0x01, 0x02, 0x00, 0x02, 0x03, 0x00, 0x00, 0x00,
	0x0d, 0x24, 0x06, 0x02, 0x01, 0x02, 0x01, 0x00, 0x02, 0x00, 0x02, 0x00, 0x00,
	0x09, 0x24, 0x03, 0x03, 0x01, 0x01, 0x00, 0x02, 0x00,
	0x0c, 0x24, 0x02, 0x11, 0x01, 0x01, 0x00, 0x02, 0x03, 0x00, 0x00, 0x00,
	0x0d, 0x24, 0x06, 0x12, 0x11, 0x02, 0x01, 0x00, 0x02, 0x00, 0x02, 0x00, 0x00,
	0x09, 0x24, 0x03, 0x13, 0x02, 0x03, 0x00, 0x12, 0x00,
	0x09, 0x04, 0x02, 0x00, 0x00, 0x01, 0x02, 0x00, 0x00,
	0x09, 0x04, 0x02, 0x01, 0x02, 0x01, 0x02, 0x00, 0x00,
	0x07, 0x24, 0x01, 0x11, 0x04, 0x01, 0x00,
	0x0b, 0x24, 0x02, 0x01, 0x02, 0x03, 0x18, 0x01, 0x80, 0xbb, 0x00,
	0x09, 0x05, 0x03, 0x05, 0x26, 0x01, 0x04, 0x04, 0x84,
	0x07, 0x25, 0x01, 0x01, 0x00, 0x00, 0x00,
	0x09, 0x05, 0x84, 0x11, 0x04, 0x00, 0x04, 0x05, 0x00,
	0x09, 0x04, 0x02, 0x02, 0x02, 0x01, 0x02, 0x00, 0x00,
	0x07, 0x24, 0x01, 0x11, 0x04, 0x01, 0x00,
	0x0b, 0x24, 0x02, 0x01, 0x02, 0x02, 0x10, 0x01, 0x44, 0xac, 0x00,
	0x09, 0x05, 0x03, 0x05, 0xb4, 0x00, 0x04, 0x04, 0x84,
	0x07, 0x25, 0x01, 0x01, 0x00, 0x00, 0x00,
	0x09, 0x05, 0x84, 0x11, 0x04, 0x00, 0x04, 0x05, 0x00,
	0x09, 0x04, 0x03, 0x00, 0x00, 0x01, 0x02, 0x00, 0x00,
	0x09, 0x04, 0x03, 0x01, 0x01, 0x01, 0x02, 0x00, 0x00,
	0x07, 0x24, 0x01, 0x03, 0x01, 0x01, 0x00,
	0x0b, 0x24, 0x02, 0x01, 0x02, 0x03, 0x18, 0x01, 0x80, 0xbb, 0x00,
	0x09, 0x05, 0x85, 0x25, 0x26, 0x01, 0x04, 0x00, 0x00,
	0x07, 0x25, 0x01, 0x01, 0x00, 0x00, 0x00,
	0x09, 0x04, 0x03, 0x02, 0x01, 0x01, 0x02, 0x00, 0x00,
	0x07, 0x24, 0x01, 0x03, 0x01, 0x01, 0x00,
	0x0b, 0x24, 0x02, 0x01, 0x02, 0x02, 0x10, 0x01, 0x44, 0xac, 0x00,
	0x09, 0x05, 0x85, 0x25, 0xb4, 0x00, 0x04, 0x00, 0x00,
	0x07, 0x25, 0x01, 0x01, 0x00, 0x00, 0x00
};

static const uint8_t other_speed_configuration_descriptor[] = {
	0x09, 0x07, 0x41, 0x01, 0x04, 0x01, 0x00, 0xc0, 0xfa,
	0x09, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x08, 0x0b, 0x01, 0x03, 0x01, 0x01, 0x00, 0x00,
	0x09, 0x04, 0x01, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00,
	0x0a, 0x24, 0x01, 0x00, 0x01, 0x4e, 0x00, 0x02, 0x02, 0x03,
	0x0c, 0x24, 0x02, 0x01, 0x01, 0x02, 0x00, 0x02, 0x03, 0x00, 0x00, 0x00,
	0x0d, 0x24, 0x06, 0x02, 0x01, 0x02, 0x01, 0x00, 0x02, 0x00, 0x02, 0x00, 0x00,
	0x09, 0x24, 0x03, 0x03, 0x01, 0x01, 0x00, 0x02, 0x00,
	0x0c, 0x24, 0x02, 0x11, 0x01, 0x01, 0x00, 0x02, 0x03, 0x00, 0x00, 0x00,
	0x0d, 0x24, 0x06, 0x12, 0x11, 0x02, 0x01, 0x00, 0x02, 0x00, 0x02, 0x00, 0x00,
	0x09, 0x24, 0x03, 0x13, 0x02, 0x03, 0x00, 0x12, 0x00,
	0x09, 0x04, 0x02, 0x00, 0x00, 0x01, 0x02, 0x00, 0x00,
	0x09, 0x04, 0x02, 0x01, 0x02, 0x01, 0x02, 0x00, 0x00,
	0x07, 0x24, 0x01, 0x11, 0x04, 0x01, 0x00,
	0x0b, 0x24, 0x02, 0x01, 0x02, 0x03, 0x18, 0x01, 0x80, 0xbb, 0x00,
	0x09, 0x05, 0x03, 0x05, 0x26, 0x01, 0x01, 0x01, 0x84,
	0x07, 0x25, 0x01, 0x01, 0x00, 0x00, 0x00,
	0x09, 0x05, 0x84, 0x11, 0x03, 0x00, 0x01, 0x05, 0x00,
	0x09, 0x04, 0x02, 0x02, 0x02, 0x01, 0x02, 0x00, 0x00,
	0x07, 0x24, 0x01, 0x11, 0x04, 0x01, 0x00,
	0x0b, 0x24, 0x02, 0x01, 0x02, 0x02, 0x10, 0x01, 0x44, 0xac, 0x00,
	0x09, 0x05, 0x03, 0x05, 0xb4, 0x00, 0x01, 0x01, 0x84,
	0x07, 0x25, 0x01, 0x01, 0x00, 0x00, 0x00,
	0x09, 0x05, 0x84, 0x11, 0x03, 0x00, 0x01, 0x05, 0x00,
	0x09, 0x04, 0x03, 0x00, 0x00, 0x01, 0x02, 0x00, 0x00,
	0x09, 0x04, 0x03, 0x01, 0x01, 0x01, 0x02, 0x00, 0x00,
	0x07, 0x24, 0x01, 0x03, 0x01, 0x01, 0x00,
	0x0b, 0x24, 0x02, 0x01, 0x02, 0x03, 0x18, 0x01, 0x80, 0xbb, 0x00,
	0x09, 0x05, 0x85, 0x25, 0x26, 0x01, 0x01, 0x00, 0x00,
	0x07, 0x25, 0x01, 0x01, 0x00, 0x00, 0x00,
	0x09, 0x04, 0x03, 0x02, 0x01, 0x01, 0x02, 0x00, 0x00,
	0x07, 0x24, 0x01, 0x03, 0x01, 0x01, 0x00,
	0x0b, 0x24, 0x02, 0x01, 0x02, 0x02, 0x10, 0x01, 0x44, 0xac, 0x00,
	0x09, 0x05, 0x85, 0x25, 0xb4, 0x00, 0x01, 0x00, 0x00,
	0x07, 0x25, 0x01, 0x01, 0x00, 0x00, 0x00
};

static const uint8_t device_qualifier_descriptor[] = {
	10, USB_DESC_DEVICE_QUAL,
	0x00, 0x02,
	0xef, 0x02, 0x01,
	EP0_SIZE,
	1,
	0
};

static const uint8_t string0_descriptor[] = {
	4, USB_DESC_STRING,
	0x09, 0x04
};

static const uint8_t manufacturer_string_descriptor[] = {
	22, USB_DESC_STRING,
	'S', 0, 'D', 0, 'R', 0, '-', 0, 'W', 0,
	'i', 0, 'd', 0, 'g', 0, 'e', 0, 't', 0
};

static const uint8_t product_string_descriptor[] = {
	40, USB_DESC_STRING,
	'Y', 0, 'o', 0, 'y', 0, 'o', 0, 'd', 0,
	'y', 0, 'n', 0, 'e', 0, ' ', 0, 'S', 0,
	'D', 0, 'R', 0, '-', 0, 'W', 0, 'i', 0,
	'd', 0, 'g', 0, 'e', 0, 't', 0
};

static const uint8_t serial_string_descriptor[] = {
	28, USB_DESC_STRING,
	'1', 0, '.', 0, '0', 0, '.', 0, '0', 0,
	'.', 0, '0', 0, '.', 0, '0', 0, '.', 0,
	'0', 0, '.', 0, '0', 0
};

static const uint8_t status_self_powered[] = { 1, 0 };
static const uint8_t status_zero[] = { 0, 0 };
static const uint8_t one_byte_zero[] = { 0 };

static const char * const feature_index_names[] = {
	"major",
	"minor",
	"board",
	"image",
	"in",
	"out",
	"adc",
	"dac",
	"lcd",
	"log",
	"end"
};

static const char * const feature_value_names[] = {
	"none",
	"widget",
	"usbi2s",
	"usbdac",
	"test",
	"end",
	"flashyblinky",
	"uac1_audio",
	"uac1_dg8saq",
	"uac2_audio",
	"uac2_dg8saq",
	"hpsdr",
	"test",
	"end",
	"normal",
	"swapped",
	"end",
	"normal",
	"swapped",
	"end",
	"none",
	"ak5394a",
	"end",
	"none",
	"cs4344",
	"es9022",
	"end",
	"none",
	"hd44780",
	"ks0073",
	"end",
	"none",
	"250ms",
	"500ms",
	"1sec",
	"2sec",
	"end",
	"end"
};

static const uint8_t feature_defaults[FEATURE_END_INDEX] = {
	FEATURE_END_INDEX,
	FEATURE_END_VALUES,
	FEATURE_BOARD_WIDGET,
	FEATURE_IMAGE_UAC1_DG8SAQ,
	FEATURE_IN_NORMAL,
	FEATURE_OUT_NORMAL,
	FEATURE_ADC_AK5394A,
	FEATURE_DAC_CS4344,
	FEATURE_LCD_HD44780,
	FEATURE_LOG_500MS
};

static uint8_t feature_nvram[FEATURE_END_INDEX] = {
	FEATURE_END_INDEX,
	FEATURE_END_VALUES,
	FEATURE_BOARD_WIDGET,
	FEATURE_IMAGE_UAC1_DG8SAQ,
	FEATURE_IN_NORMAL,
	FEATURE_OUT_NORMAL,
	FEATURE_ADC_AK5394A,
	FEATURE_DAC_CS4344,
	FEATURE_LCD_HD44780,
	FEATURE_LOG_500MS
};

static uint8_t feature_ram[FEATURE_END_INDEX] = {
	FEATURE_END_INDEX,
	FEATURE_END_VALUES,
	FEATURE_BOARD_WIDGET,
	FEATURE_IMAGE_UAC1_DG8SAQ,
	FEATURE_IN_NORMAL,
	FEATURE_OUT_NORMAL,
	FEATURE_ADC_AK5394A,
	FEATURE_DAC_CS4344,
	FEATURE_LCD_HD44780,
	FEATURE_LOG_500MS
};

static const uint8_t audio_sample_rate_48k[] = { 0x80, 0xbb, 0x00 };
static const uint8_t audio_sample_rate_44k1[] = { 0x44, 0xac, 0x00 };
static const uint8_t audio_sample_rate_res[] = { 0x01, 0x00, 0x00 };
static const uint8_t audio_mute_off[] = { 0x00 };
static const uint8_t audio_volume_current[] = { 0x00, 0x00 };
static const uint8_t audio_volume_min[] = { 0x00, 0x80 };
static const uint8_t audio_volume_max[] = { 0xff, 0x7f };
static const uint8_t audio_volume_res[] = { 0x0a, 0x00 };
static const uint8_t audio_feedback_48k_hs[] = { 0x00, 0x00, 0x06, 0x00 };
static const uint8_t audio_feedback_44k1_hs[] = { 0x33, 0x83, 0x05, 0x00 };

static uint32_t usb_initialized;
static uint32_t usb_attached;
static uint32_t usb_reset_count;
static uint32_t usb_setup_count;
static uint32_t usb_tx_count;
static uint32_t usb_rxout_count;
static uint32_t usb_stall_count;
static uint32_t usb_descriptor_count;
static uint32_t usb_set_address_count;
static uint32_t usb_set_configuration_count;
static uint32_t usb_set_interface_count;
static uint32_t audio_set_interface_count;
static uint32_t interface_alternate_peak_mask;
static uint32_t last_set_interface_index;
static uint32_t last_set_interface_value;
static uint32_t audio_config_count;
static uint32_t audio_out_count;
static uint32_t audio_out_bytes;
static uint32_t audio_out_nonzero_bytes;
static uint32_t audio_out_last_bytes;
static uint32_t audio_out_max_bytes;
static uint32_t audio_feedback_count;
static uint32_t audio_feedback_bytes;
static uint32_t audio_feedback_busy_last;
static uint32_t audio_feedback_busy_max;
static uint32_t audio_in_count;
static uint32_t audio_in_bytes;
static uint32_t audio_in_busy_last;
static uint32_t audio_in_busy_max;
static uint32_t audio_error_count;
static uint32_t audio_short_count;
static uint32_t audio_crc_count;
static uint32_t audio_overflow_count;
static uint32_t audio_underflow_count;
static uint32_t audio_source_mode;
static uint32_t audio_out_format;
static uint32_t audio_in_format;
static uint32_t audio_cd_in_frame_accumulator;
static uint8_t audio_loopback_buffer[AUDIO_LOOPBACK_BUFFER_BYTES];
static uint32_t audio_loopback_read;
static uint32_t audio_loopback_write;
static uint32_t audio_loopback_level;
static uint32_t audio_loopback_primed;
static uint32_t audio_loopback_peak;
static uint32_t audio_loopback_drop_bytes;
static uint32_t audio_loopback_silence_bytes;
static uint32_t audio_pattern_lcg;
static uint32_t audio_tone_phase;
static uint32_t audio_sine_phase;
static uint32_t usb_address;
static uint32_t usb_configuration;
static uint8_t interface_alternate[USB_INTERFACE_COUNT];
static uint32_t pending_address_valid;
static uint32_t pending_address;
static uint32_t pending_device_reset;
static uint32_t pending_audio_sample_rate_valid;
static uint32_t pending_audio_sample_rate_endpoint;
static uint32_t last_setup0;
static uint32_t last_wvalue;
static uint32_t last_windex;
static uint32_t last_wlength;
static ep0_state_t ep0_state;
static const uint8_t *ep0_tx_data;
static uint32_t ep0_tx_remaining;
static uint8_t vendor_response[EP0_SIZE];

static uint16_t read_le16(const uint8_t *data)
{
	return (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

static uint32_t min_u32(uint32_t a, uint32_t b)
{
	return (a < b) ? a : b;
}

static void reset_audio_in_packet_schedule(void)
{
	audio_cd_in_frame_accumulator = 0u;
}

static void reset_audio_stream_formats(void)
{
	audio_out_format = SAME70_USB_AUDIO_FORMAT_48K24;
	audio_in_format = SAME70_USB_AUDIO_FORMAT_48K24;
	reset_audio_in_packet_schedule();
}

static uint32_t audio_format_from_alternate(uint32_t alternate)
{
	return (alternate == 2u) ? SAME70_USB_AUDIO_FORMAT_44K16 : SAME70_USB_AUDIO_FORMAT_48K24;
}

static uint32_t audio_format_for_endpoint_address(uint32_t endpoint)
{
	uint32_t number = endpoint & 0x0fu;

	if (number == USB_AUDIO_IN_EP) {
		return audio_in_format;
	}
	if ((number == USB_AUDIO_OUT_EP) || (number == USB_AUDIO_FB_EP)) {
		return audio_out_format;
	}

	return audio_out_format;
}

static const uint8_t *audio_sample_rate_for_format(uint32_t format, uint32_t *length)
{
	if (format == SAME70_USB_AUDIO_FORMAT_44K16) {
		*length = sizeof(audio_sample_rate_44k1);
		return audio_sample_rate_44k1;
	}

	*length = sizeof(audio_sample_rate_48k);
	return audio_sample_rate_48k;
}

static const uint8_t *audio_feedback_for_output_format(uint32_t *length)
{
	if (audio_out_format == SAME70_USB_AUDIO_FORMAT_44K16) {
		*length = sizeof(audio_feedback_44k1_hs);
		return audio_feedback_44k1_hs;
	}

	*length = sizeof(audio_feedback_48k_hs);
	return audio_feedback_48k_hs;
}

static uint32_t audio_out_max_packet_bytes(void)
{
	if (audio_out_format == SAME70_USB_AUDIO_FORMAT_44K16) {
		return AUDIO_OUT_44K16_MAX_BYTES;
	}

	return AUDIO_OUT_48K24_MAX_BYTES;
}

static uint32_t audio_in_packet_bytes(void)
{
	uint32_t frames;

	if (audio_in_format != SAME70_USB_AUDIO_FORMAT_44K16) {
		return AUDIO_IN_48K24_PACKET_BYTES;
	}

	audio_cd_in_frame_accumulator += AUDIO_44K16_FRAMES_PER_10MS;
	frames = audio_cd_in_frame_accumulator / AUDIO_44K16_PACKET_DIVISOR;
	audio_cd_in_frame_accumulator -= frames * AUDIO_44K16_PACKET_DIVISOR;
	return frames * AUDIO_44K16_FRAME_BYTES;
}

static uint32_t audio_loopback_prime_bytes(void)
{
	if (audio_in_format == SAME70_USB_AUDIO_FORMAT_44K16) {
		return AUDIO_OUT_44K16_MAX_BYTES * 4u;
	}

	return AUDIO_IN_48K24_PACKET_BYTES * 4u;
}

static void set_audio_interface_format(uint32_t interface, uint32_t alternate)
{
	uint32_t format;

	if (alternate == 0u) {
		return;
	}

	format = audio_format_from_alternate(alternate);
	if (interface == 2u) {
		audio_out_format = format;
	} else if (interface == 3u) {
		audio_in_format = format;
		reset_audio_in_packet_schedule();
	}
}

static uint32_t audio_format_from_sample_rate_data(const uint8_t *data, uint32_t *format)
{
	if ((data[0] == audio_sample_rate_44k1[0]) &&
	    (data[1] == audio_sample_rate_44k1[1]) &&
	    (data[2] == audio_sample_rate_44k1[2])) {
		*format = SAME70_USB_AUDIO_FORMAT_44K16;
		return 1u;
	}
	if ((data[0] == audio_sample_rate_48k[0]) &&
	    (data[1] == audio_sample_rate_48k[1]) &&
	    (data[2] == audio_sample_rate_48k[2])) {
		*format = SAME70_USB_AUDIO_FORMAT_48K24;
		return 1u;
	}

	return 0u;
}

static void set_audio_endpoint_format(uint32_t endpoint, uint32_t format)
{
	uint32_t number = endpoint & 0x0fu;

	if (number == USB_AUDIO_IN_EP) {
		audio_in_format = format;
		reset_audio_in_packet_schedule();
	} else if ((number == USB_AUDIO_OUT_EP) || (number == USB_AUDIO_FB_EP)) {
		audio_out_format = format;
	} else {
		audio_out_format = format;
		audio_in_format = format;
		reset_audio_in_packet_schedule();
	}
}

static void handle_ep0_out_data(uint32_t length)
{
	volatile uint8_t *fifo = USBHS_EP_FIFO(0u);
	uint8_t rate[3];
	uint32_t format;

	if ((pending_audio_sample_rate_valid == 0u) || (length < sizeof(rate))) {
		pending_audio_sample_rate_valid = 0u;
		return;
	}

	rate[0] = fifo[0];
	rate[1] = fifo[1];
	rate[2] = fifo[2];
	if (audio_format_from_sample_rate_data(rate, &format) != 0u) {
		set_audio_endpoint_format(pending_audio_sample_rate_endpoint, format);
	}
	pending_audio_sample_rate_valid = 0u;
}

static uint32_t string_length(const char *text)
{
	uint32_t length = 0u;

	while (text[length] != '\0') {
		length++;
	}

	return length;
}

static uint32_t copy_reversed_string(uint8_t *dest, const char *text)
{
	uint32_t length = min_u32(string_length(text), EP0_SIZE);
	uint32_t index;

	for (index = 0u; index < length; index++) {
		dest[index] = (uint8_t)text[length - 1u - index];
	}

	return length;
}

static void wait_for_locku(void)
{
	uint32_t timeout = 1000000u;

	while (((PMC_SR & PMC_SR_LOCKU) == 0u) && (timeout != 0u)) {
		timeout--;
	}
}

static void wait_for_clkusable(void)
{
	uint32_t timeout = 1000000u;

	while (((USBHS_SR & USBHS_SR_CLKUSABLE) == 0u) && (timeout != 0u)) {
		timeout--;
	}
}

static void system_reset(void)
{
	SCB_AIRCR = SCB_AIRCR_VECTKEY | (SCB_AIRCR & SCB_AIRCR_PRIGROUP_Msk) | SCB_AIRCR_SYSRESETREQ;
	while (1) {
	}
}

static void set_usb_address(uint32_t address)
{
	uint32_t devctrl = USBHS_DEVCTRL;

	devctrl &= ~(USBHS_DEVCTRL_UADD_MASK | USBHS_DEVCTRL_ADDEN);
	devctrl |= address & USBHS_DEVCTRL_UADD_MASK;
	if (address != 0u) {
		devctrl |= USBHS_DEVCTRL_ADDEN;
	}
	USBHS_DEVCTRL = devctrl;
	usb_address = address;
}

static void clear_interface_alternates(void)
{
	uint32_t index;

	for (index = 0u; index < USB_INTERFACE_COUNT; index++) {
		interface_alternate[index] = 0u;
	}
	reset_audio_stream_formats();
}

static uint32_t interface_alternate_mask(void)
{
	uint32_t mask = 0u;
	uint32_t index;

	for (index = 0u; index < USB_INTERFACE_COUNT; index++) {
		if (interface_alternate[index] != 0u) {
			mask |= 1u << index;
		}
	}

	return mask;
}

static void update_interface_alternate_peak(void)
{
	interface_alternate_peak_mask |= interface_alternate_mask();
}

static void clear_ep0_state(void)
{
	ep0_state = EP0_STATE_IDLE;
	ep0_tx_data = 0;
	ep0_tx_remaining = 0u;
	pending_address_valid = 0u;
	pending_device_reset = 0u;
	pending_audio_sample_rate_valid = 0u;
}

static void configure_ep0(void)
{
	USBHS_DEVEPT = USBHS_DEVEPT_EPRST0;
	USBHS_DEVEPT = 0u;
	USBHS_DEVEPTCFG(0u) =
		USBHS_DEVEPTCFG_ALLOC |
		USBHS_DEVEPTCFG_1_BANK |
		USBHS_DEVEPTCFG_64B |
		USBHS_DEVEPTCFG_CTRL;
	USBHS_DEVEPT = USBHS_DEVEPT_EPEN0;
	USBHS_DEVEPTIER(0u) = USBHS_DEVEPTIER_RSTDTS | USBHS_DEVEPTIER_RXSTPES | USBHS_DEVEPTIER_RXOUTES;
	USBHS_DEVEPTIDR(0u) = USBHS_DEVEPTIDR_STALLRQC;
	clear_ep0_state();
}

static void reset_endpoint(uint32_t ep)
{
	uint32_t enabled = USBHS_DEVEPT & 0xffffu;

	USBHS_DEVEPT = enabled | USBHS_DEVEPT_EPRST(ep);
	USBHS_DEVEPT = enabled & ~USBHS_DEVEPT_EPEN(ep);
}

static void configure_audio_endpoint(uint32_t ep, uint32_t direction, uint32_t size)
{
	reset_endpoint(ep);
	USBHS_DEVEPTCFG(ep) =
		USBHS_DEVEPTCFG_ALLOC |
		USBHS_DEVEPTCFG_2_BANK |
		size |
		direction |
		USBHS_DEVEPTCFG_ISO;
	USBHS_DEVEPT |= USBHS_DEVEPT_EPEN(ep);
	USBHS_DEVEPTIER(ep) = USBHS_DEVEPTIER_RSTDTS;
	USBHS_DEVEPTIDR(ep) = USBHS_DEVEPTIDR_STALLRQC;
}

static void disable_audio_endpoints(void)
{
	reset_endpoint(USB_AUDIO_OUT_EP);
	reset_endpoint(USB_AUDIO_FB_EP);
	reset_endpoint(USB_AUDIO_IN_EP);
	USBHS_DEVEPTCFG(USB_AUDIO_OUT_EP) = 0u;
	USBHS_DEVEPTCFG(USB_AUDIO_FB_EP) = 0u;
	USBHS_DEVEPTCFG(USB_AUDIO_IN_EP) = 0u;
}

static uint32_t audio_cfgok_mask(void)
{
	uint32_t mask = 0u;

	if ((USBHS_DEVEPTISR(USB_AUDIO_OUT_EP) & USBHS_DEVEPTISR_CFGOK) != 0u) {
		mask |= USBHS_DEVEPT_EPEN(USB_AUDIO_OUT_EP);
	}
	if ((USBHS_DEVEPTISR(USB_AUDIO_FB_EP) & USBHS_DEVEPTISR_CFGOK) != 0u) {
		mask |= USBHS_DEVEPT_EPEN(USB_AUDIO_FB_EP);
	}
	if ((USBHS_DEVEPTISR(USB_AUDIO_IN_EP) & USBHS_DEVEPTISR_CFGOK) != 0u) {
		mask |= USBHS_DEVEPT_EPEN(USB_AUDIO_IN_EP);
	}

	return mask;
}

static void configure_audio_endpoints(void)
{
	uint32_t expected =
		USBHS_DEVEPT_EPEN(USB_AUDIO_OUT_EP) |
		USBHS_DEVEPT_EPEN(USB_AUDIO_FB_EP) |
		USBHS_DEVEPT_EPEN(USB_AUDIO_IN_EP);
	uint32_t timeout = 1000u;

	configure_audio_endpoint(USB_AUDIO_OUT_EP, 0u, USBHS_DEVEPTCFG_512B);
	configure_audio_endpoint(USB_AUDIO_FB_EP, USBHS_DEVEPTCFG_IN, USBHS_DEVEPTCFG_8B);
	configure_audio_endpoint(USB_AUDIO_IN_EP, USBHS_DEVEPTCFG_IN, USBHS_DEVEPTCFG_512B);
	audio_config_count++;

	while ((audio_cfgok_mask() != expected) && (timeout != 0u)) {
		timeout--;
	}

	if (audio_cfgok_mask() != expected) {
		audio_error_count++;
	}
}

static void clear_iso_status(uint32_t ep, uint32_t isr);

static void reset_audio_loopback_buffer(void)
{
	audio_loopback_read = 0u;
	audio_loopback_write = 0u;
	audio_loopback_level = 0u;
	audio_loopback_primed = 0u;
}

static void audio_loopback_push(uint8_t value)
{
	if (audio_loopback_level >= AUDIO_LOOPBACK_BUFFER_BYTES) {
		audio_loopback_drop_bytes++;
		return;
	}

	audio_loopback_buffer[audio_loopback_write] = value;
	audio_loopback_write++;
	if (audio_loopback_write >= AUDIO_LOOPBACK_BUFFER_BYTES) {
		audio_loopback_write = 0u;
	}
	audio_loopback_level++;
	if (audio_loopback_level > audio_loopback_peak) {
		audio_loopback_peak = audio_loopback_level;
	}
}

static uint32_t audio_loopback_pop(uint8_t *value)
{
	if (audio_loopback_level == 0u) {
		*value = 0u;
		audio_loopback_silence_bytes++;
		return 0u;
	}

	*value = audio_loopback_buffer[audio_loopback_read];
	audio_loopback_read++;
	if (audio_loopback_read >= AUDIO_LOOPBACK_BUFFER_BYTES) {
		audio_loopback_read = 0u;
	}
	audio_loopback_level--;
	return 1u;
}

static uint32_t next_audio_pattern_sample(void)
{
	uint32_t sample;

	audio_pattern_lcg = (audio_pattern_lcg * 1664525u) + 1013904223u;
	sample = (audio_pattern_lcg >> 9) & 0x3fffffu;
	if (sample == 0x200000u) {
		sample++;
	}

	return (sample - 0x200000u) & 0x00ffffffu;
}

static uint32_t next_audio_pattern_sample16(void)
{
	uint32_t sample;

	audio_pattern_lcg = (audio_pattern_lcg * 1664525u) + 1013904223u;
	sample = (audio_pattern_lcg >> 17) & 0x3fffu;
	if (sample == 0x2000u) {
		sample++;
	}

	return (sample - 0x2000u) & 0x0000ffffu;
}

static void reset_audio_pattern(void)
{
	audio_pattern_lcg = 0x12345678u;
}

static void reset_audio_tone(void)
{
	audio_tone_phase = 0u;
}

static void reset_audio_sine(void)
{
	audio_sine_phase = 0u;
}

static int next_audio_tone_sample(void)
{
	int sample;

	if (audio_tone_phase < 48u) {
		sample = 0x200000;
	} else {
		sample = -0x200000;
	}
	audio_tone_phase++;
	if (audio_tone_phase >= 96u) {
		audio_tone_phase = 0u;
	}

	return sample;
}

static uint32_t next_audio_tone_sample24(void)
{
	return (uint32_t)next_audio_tone_sample() & 0x00ffffffu;
}

static uint32_t next_audio_tone_sample16(void)
{
	return (next_audio_tone_sample() > 0) ? 0x2000u : 0xe000u;
}

static int next_audio_sine_sample16(void)
{
	static const int samples[96] = {
		0, 536, 1069, 1598, 2120, 2633, 3135, 3623,
		4096, 4551, 4987, 5401, 5793, 6159, 6499, 6811,
		7094, 7347, 7568, 7757, 7913, 8035, 8122, 8174,
		8192, 8174, 8122, 8035, 7913, 7757, 7568, 7347,
		7094, 6811, 6499, 6159, 5793, 5401, 4987, 4551,
		4096, 3623, 3135, 2633, 2120, 1598, 1069, 536,
		0, -536, -1069, -1598, -2120, -2633, -3135, -3623,
		-4096, -4551, -4987, -5401, -5793, -6159, -6499, -6811,
		-7094, -7347, -7568, -7757, -7913, -8035, -8122, -8174,
		-8192, -8174, -8122, -8035, -7913, -7757, -7568, -7347,
		-7094, -6811, -6499, -6159, -5793, -5401, -4987, -4551,
		-4096, -3623, -3135, -2633, -2120, -1598, -1069, -536,
	};
	int sample = samples[audio_sine_phase];

	audio_sine_phase++;
	if (audio_sine_phase >= 96u) {
		audio_sine_phase = 0u;
	}

	return sample;
}

static uint32_t next_audio_sine_sample16u(void)
{
	return (uint32_t)next_audio_sine_sample16() & 0x0000ffffu;
}

static uint32_t next_audio_sine_sample24(void)
{
	return (uint32_t)(next_audio_sine_sample16() * 256) & 0x00ffffffu;
}

static void write_pcm16_le(volatile uint8_t *fifo, uint32_t offset, uint32_t value)
{
	fifo[offset] = (uint8_t)(value & 0xffu);
	fifo[offset + 1u] = (uint8_t)((value >> 8) & 0xffu);
}

static void write_pcm24_le(volatile uint8_t *fifo, uint32_t offset, uint32_t value)
{
	fifo[offset] = (uint8_t)(value & 0xffu);
	fifo[offset + 1u] = (uint8_t)((value >> 8) & 0xffu);
	fifo[offset + 2u] = (uint8_t)((value >> 16) & 0xffu);
}

static void reset_audio_interface_data_toggle(uint32_t interface)
{
	if (interface == 2u) {
		reset_audio_loopback_buffer();
		USBHS_DEVEPTIER(USB_AUDIO_OUT_EP) = USBHS_DEVEPTIER_RSTDTS;
		USBHS_DEVEPTIER(USB_AUDIO_FB_EP) = USBHS_DEVEPTIER_RSTDTS;
		clear_iso_status(USB_AUDIO_OUT_EP, USBHS_DEVEPTISR(USB_AUDIO_OUT_EP));
		clear_iso_status(USB_AUDIO_FB_EP, USBHS_DEVEPTISR(USB_AUDIO_FB_EP));
	} else if (interface == 3u) {
		reset_audio_loopback_buffer();
		reset_audio_pattern();
		reset_audio_tone();
		reset_audio_sine();
		reset_audio_in_packet_schedule();
		USBHS_DEVEPTIER(USB_AUDIO_IN_EP) = USBHS_DEVEPTIER_RSTDTS;
		clear_iso_status(USB_AUDIO_IN_EP, USBHS_DEVEPTISR(USB_AUDIO_IN_EP));
	}
}

static void ep0_write_packet(const uint8_t *data, uint32_t length)
{
	volatile uint8_t *fifo = USBHS_EP_FIFO(0u);
	uint32_t index;

	for (index = 0u; index < length; index++) {
		fifo[index] = data[index];
	}

	USBHS_DEVEPTICR(0u) = USBHS_DEVEPTICR_TXINIC;
	USBHS_DEVEPTIDR(0u) = USBHS_DEVEPTIDR_FIFOCONC;
	usb_tx_count++;
}

static void ep0_send_next_data(void)
{
	uint32_t count = min_u32(ep0_tx_remaining, EP0_SIZE);

	ep0_write_packet(ep0_tx_data, count);
	ep0_tx_data += count;
	ep0_tx_remaining -= count;
	if (ep0_tx_remaining == 0u) {
		ep0_state = EP0_STATE_WAIT_OUT_STATUS;
	}
}

static void ep0_start_in(const uint8_t *data, uint32_t length)
{
	uint32_t count = min_u32(length, EP0_SIZE);

	ep0_tx_data = data + count;
	ep0_tx_remaining = length - count;
	ep0_state = (ep0_tx_remaining == 0u) ? EP0_STATE_WAIT_OUT_STATUS : EP0_STATE_TX_DATA;
	ep0_write_packet(data, count);
}

static void ep0_start_status_in(void)
{
	ep0_state = EP0_STATE_TX_STATUS;
	ep0_write_packet(one_byte_zero, 0u);
}

static void ep0_expect_out_data_status(void)
{
	ep0_state = EP0_STATE_RX_DATA_STATUS;
	ep0_tx_data = 0;
	ep0_tx_remaining = 0u;
}

static void ep0_stall(void)
{
	USBHS_DEVEPTIER(0u) = USBHS_DEVEPTIER_STALLRQS;
	ep0_state = EP0_STATE_IDLE;
	ep0_tx_data = 0;
	ep0_tx_remaining = 0u;
	pending_address_valid = 0u;
	usb_stall_count++;
}

static void write_endpoint_packet(uint32_t ep, const uint8_t *data, uint32_t length)
{
	volatile uint8_t *fifo = USBHS_EP_FIFO(ep);
	uint32_t index;

	for (index = 0u; index < length; index++) {
		fifo[index] = data[index];
	}

	USBHS_DEVEPTICR(ep) = USBHS_DEVEPTICR_TXINIC;
	USBHS_DEVEPTIDR(ep) = USBHS_DEVEPTIDR_FIFOCONC;
}

static void write_endpoint_loopback_packet(uint32_t ep, uint32_t length)
{
	volatile uint8_t *fifo = USBHS_EP_FIFO(ep);
	uint32_t index;
	uint8_t value;

	if (audio_loopback_primed == 0u) {
		if (audio_loopback_level < audio_loopback_prime_bytes()) {
			for (index = 0u; index < length; index++) {
				fifo[index] = 0u;
			}
			audio_loopback_silence_bytes += length;
			USBHS_DEVEPTICR(ep) = USBHS_DEVEPTICR_TXINIC;
			USBHS_DEVEPTIDR(ep) = USBHS_DEVEPTIDR_FIFOCONC;
			return;
		}
		audio_loopback_primed = 1u;
	}

	if (audio_loopback_level < length) {
		audio_loopback_primed = 0u;
		for (index = 0u; index < length; index++) {
			fifo[index] = 0u;
		}
		audio_loopback_silence_bytes += length;
		USBHS_DEVEPTICR(ep) = USBHS_DEVEPTICR_TXINIC;
		USBHS_DEVEPTIDR(ep) = USBHS_DEVEPTIDR_FIFOCONC;
		return;
	}

	for (index = 0u; index < length; index++) {
		(void)audio_loopback_pop(&value);
		fifo[index] = value;
	}

	USBHS_DEVEPTICR(ep) = USBHS_DEVEPTICR_TXINIC;
	USBHS_DEVEPTIDR(ep) = USBHS_DEVEPTIDR_FIFOCONC;
}

static void write_endpoint_pattern_packet(uint32_t ep, uint32_t length)
{
	volatile uint8_t *fifo = USBHS_EP_FIFO(ep);
	uint32_t index;

	if (audio_in_format == SAME70_USB_AUDIO_FORMAT_44K16) {
		for (index = 0u; (index + 1u) < length; index += 2u) {
			write_pcm16_le(fifo, index, next_audio_pattern_sample16());
		}
	} else {
		for (index = 0u; (index + 2u) < length; index += 3u) {
			write_pcm24_le(fifo, index, next_audio_pattern_sample());
		}
	}
	for (; index < length; index++) {
		fifo[index] = 0u;
	}

	USBHS_DEVEPTICR(ep) = USBHS_DEVEPTICR_TXINIC;
	USBHS_DEVEPTIDR(ep) = USBHS_DEVEPTIDR_FIFOCONC;
}

static void write_endpoint_tone_packet(uint32_t ep, uint32_t length)
{
	volatile uint8_t *fifo = USBHS_EP_FIFO(ep);
	uint32_t index;

	if (audio_in_format == SAME70_USB_AUDIO_FORMAT_44K16) {
		for (index = 0u; (index + 1u) < length; index += 2u) {
			write_pcm16_le(fifo, index, next_audio_tone_sample16());
		}
	} else {
		for (index = 0u; (index + 2u) < length; index += 3u) {
			write_pcm24_le(fifo, index, next_audio_tone_sample24());
		}
	}
	for (; index < length; index++) {
		fifo[index] = 0u;
	}

	USBHS_DEVEPTICR(ep) = USBHS_DEVEPTICR_TXINIC;
	USBHS_DEVEPTIDR(ep) = USBHS_DEVEPTIDR_FIFOCONC;
}

static void write_endpoint_sine_packet(uint32_t ep, uint32_t length)
{
	volatile uint8_t *fifo = USBHS_EP_FIFO(ep);
	uint32_t index;

	if (audio_in_format == SAME70_USB_AUDIO_FORMAT_44K16) {
		for (index = 0u; (index + 3u) < length; index += 4u) {
			uint32_t sample = next_audio_sine_sample16u();

			write_pcm16_le(fifo, index, sample);
			write_pcm16_le(fifo, index + 2u, sample);
		}
	} else {
		for (index = 0u; (index + 5u) < length; index += 6u) {
			uint32_t sample = next_audio_sine_sample24();

			write_pcm24_le(fifo, index, sample);
			write_pcm24_le(fifo, index + 3u, sample);
		}
	}
	for (; index < length; index++) {
		fifo[index] = 0u;
	}

	USBHS_DEVEPTICR(ep) = USBHS_DEVEPTICR_TXINIC;
	USBHS_DEVEPTIDR(ep) = USBHS_DEVEPTIDR_FIFOCONC;
}

static void write_endpoint_silence_packet(uint32_t ep, uint32_t length)
{
	volatile uint8_t *fifo = USBHS_EP_FIFO(ep);
	uint32_t index;

	for (index = 0u; index < length; index++) {
		fifo[index] = 0u;
	}

	USBHS_DEVEPTICR(ep) = USBHS_DEVEPTICR_TXINIC;
	USBHS_DEVEPTIDR(ep) = USBHS_DEVEPTIDR_FIFOCONC;
}

static uint32_t endpoint_byte_count(uint32_t isr)
{
	return (isr & USBHS_DEVEPTISR_BYCT_MASK) >> USBHS_DEVEPTISR_BYCT_SHIFT;
}

static uint32_t endpoint_busy_banks(uint32_t isr)
{
	return (isr & USBHS_DEVEPTISR_NBUSYBK_MASK) >> USBHS_DEVEPTISR_NBUSYBK_SHIFT;
}

static void clear_iso_status(uint32_t ep, uint32_t isr)
{
	uint32_t clear =
		isr & (USBHS_DEVEPTICR_UNDERFIC |
		       USBHS_DEVEPTICR_HBISOINERRIC |
		       USBHS_DEVEPTICR_HBISOFLUSHIC |
		       USBHS_DEVEPTICR_OVERFIC |
		       USBHS_DEVEPTICR_CRCERRIC |
		       USBHS_DEVEPTICR_SHORTPACKETIC);

	if (clear != 0u) {
		USBHS_DEVEPTICR(ep) = clear;
	}
}

static void count_iso_status(uint32_t ep, uint32_t isr)
{
	uint32_t clear = 0u;

	if ((isr & USBHS_DEVEPTISR_SHORTPACKETI) != 0u) {
		audio_short_count++;
		clear |= USBHS_DEVEPTICR_SHORTPACKETIC;
	}
	if ((isr & USBHS_DEVEPTISR_CRCERRI) != 0u) {
		audio_crc_count++;
		audio_error_count++;
		clear |= USBHS_DEVEPTICR_CRCERRIC;
	}
	if ((isr & USBHS_DEVEPTISR_OVERFI) != 0u) {
		audio_overflow_count++;
		audio_error_count++;
		clear |= USBHS_DEVEPTICR_OVERFIC;
	}
	if ((isr & USBHS_DEVEPTISR_HBISOFLUSHI) != 0u) {
		audio_error_count++;
		clear |= USBHS_DEVEPTICR_HBISOFLUSHIC;
	}
	if ((isr & USBHS_DEVEPTISR_HBISOINERRI) != 0u) {
		audio_error_count++;
		clear |= USBHS_DEVEPTICR_HBISOINERRIC;
	}
	if ((isr & USBHS_DEVEPTISR_UNDERFI) != 0u) {
		audio_underflow_count++;
		audio_error_count++;
		clear |= USBHS_DEVEPTICR_UNDERFIC;
	}

	if (clear != 0u) {
		USBHS_DEVEPTICR(ep) = clear;
	}
}

static void poll_audio_out(void)
{
	volatile uint8_t *fifo = USBHS_EP_FIFO(USB_AUDIO_OUT_EP);
	uint32_t isr = USBHS_DEVEPTISR(USB_AUDIO_OUT_EP);
	uint32_t bytes;
	uint32_t index;

	if ((usb_configuration == 0u) || (interface_alternate[2] == 0u)) {
		clear_iso_status(USB_AUDIO_OUT_EP, isr);
		return;
	}

	count_iso_status(USB_AUDIO_OUT_EP, isr);
	if ((isr & USBHS_DEVEPTISR_RXOUTI) == 0u) {
		return;
	}

	bytes = min_u32(endpoint_byte_count(isr), audio_out_max_packet_bytes());
	for (index = 0u; index < bytes; index++) {
		if (fifo[index] != 0u) {
			audio_out_nonzero_bytes++;
		}
		if (audio_source_mode == SAME70_USB_AUDIO_SOURCE_LOOPBACK) {
			audio_loopback_push(fifo[index]);
		} else {
			(void)fifo[index];
		}
	}

	USBHS_DEVEPTICR(USB_AUDIO_OUT_EP) = USBHS_DEVEPTICR_RXOUTIC;
	USBHS_DEVEPTIDR(USB_AUDIO_OUT_EP) = USBHS_DEVEPTIDR_FIFOCONC;
	audio_out_count++;
	audio_out_bytes += bytes;
	audio_out_last_bytes = bytes;
	if (bytes > audio_out_max_bytes) {
		audio_out_max_bytes = bytes;
	}
}

static void poll_audio_feedback(void)
{
	uint32_t isr = USBHS_DEVEPTISR(USB_AUDIO_FB_EP);
	const uint8_t *feedback;
	uint32_t feedback_length;
	uint32_t busy;
	uint32_t attempts;

	if ((usb_configuration == 0u) || (interface_alternate[2] == 0u)) {
		clear_iso_status(USB_AUDIO_FB_EP, isr);
		return;
	}

	for (attempts = 0u; attempts < 2u; attempts++) {
		isr = USBHS_DEVEPTISR(USB_AUDIO_FB_EP);
		count_iso_status(USB_AUDIO_FB_EP, isr);
		busy = endpoint_busy_banks(isr);
		audio_feedback_busy_last = busy;
		if (busy > audio_feedback_busy_max) {
			audio_feedback_busy_max = busy;
		}
		if (((isr & USBHS_DEVEPTISR_RWALL) == 0u) || (busy >= 2u)) {
			break;
		}
		feedback = audio_feedback_for_output_format(&feedback_length);
		write_endpoint_packet(USB_AUDIO_FB_EP, feedback, feedback_length);
		audio_feedback_count++;
		audio_feedback_bytes += feedback_length;
	}
}

static void poll_audio_in(void)
{
	uint32_t isr = USBHS_DEVEPTISR(USB_AUDIO_IN_EP);
	uint32_t packet_bytes;
	uint32_t busy;
	uint32_t attempts;

	if ((usb_configuration == 0u) || (interface_alternate[3] == 0u)) {
		clear_iso_status(USB_AUDIO_IN_EP, isr);
		return;
	}

	for (attempts = 0u; attempts < 2u; attempts++) {
		isr = USBHS_DEVEPTISR(USB_AUDIO_IN_EP);
		count_iso_status(USB_AUDIO_IN_EP, isr);
		busy = endpoint_busy_banks(isr);
		audio_in_busy_last = busy;
		if (busy > audio_in_busy_max) {
			audio_in_busy_max = busy;
		}
		if (((isr & USBHS_DEVEPTISR_RWALL) == 0u) || (busy >= 2u)) {
			break;
		}
		packet_bytes = audio_in_packet_bytes();
		if (audio_source_mode == SAME70_USB_AUDIO_SOURCE_PATTERN) {
			write_endpoint_pattern_packet(USB_AUDIO_IN_EP, packet_bytes);
		} else if (audio_source_mode == SAME70_USB_AUDIO_SOURCE_TONE) {
			write_endpoint_tone_packet(USB_AUDIO_IN_EP, packet_bytes);
		} else if (audio_source_mode == SAME70_USB_AUDIO_SOURCE_SINE) {
			write_endpoint_sine_packet(USB_AUDIO_IN_EP, packet_bytes);
		} else if (audio_source_mode == SAME70_USB_AUDIO_SOURCE_SILENCE) {
			write_endpoint_silence_packet(USB_AUDIO_IN_EP, packet_bytes);
		} else {
			write_endpoint_loopback_packet(USB_AUDIO_IN_EP, packet_bytes);
		}
		audio_in_count++;
		audio_in_bytes += packet_bytes;
	}
}

static void poll_audio_endpoints(void)
{
	poll_audio_out();
	poll_audio_feedback();
	poll_audio_in();
}

static void prime_audio_interface(uint32_t interface)
{
	if (interface == 2u) {
		poll_audio_feedback();
	} else if (interface == 3u) {
		poll_audio_in();
	}
}

static uint32_t select_descriptor(uint8_t type, uint8_t index, const uint8_t **data, uint32_t *length)
{
	switch (type) {
	case USB_DESC_DEVICE:
		*data = device_descriptor;
		*length = sizeof(device_descriptor);
		return 1u;
	case USB_DESC_CONFIGURATION:
		*data = configuration_descriptor;
		*length = sizeof(configuration_descriptor);
		return 1u;
	case USB_DESC_OTHER_SPEED:
		*data = other_speed_configuration_descriptor;
		*length = sizeof(other_speed_configuration_descriptor);
		return 1u;
	case USB_DESC_DEVICE_QUAL:
		*data = device_qualifier_descriptor;
		*length = sizeof(device_qualifier_descriptor);
		return 1u;
	case USB_DESC_STRING:
		if (index == 0u) {
			*data = string0_descriptor;
			*length = sizeof(string0_descriptor);
			return 1u;
		}
		if (index == 1u) {
			*data = manufacturer_string_descriptor;
			*length = sizeof(manufacturer_string_descriptor);
			return 1u;
		}
		if (index == 2u) {
			*data = product_string_descriptor;
			*length = sizeof(product_string_descriptor);
			return 1u;
		}
		if (index == 3u) {
			*data = serial_string_descriptor;
			*length = sizeof(serial_string_descriptor);
			return 1u;
		}
		break;
	default:
		break;
	}

	return 0u;
}

static void handle_get_descriptor(const usb_setup_t *setup)
{
	const uint8_t *data = 0;
	uint32_t length = 0u;
	uint8_t index = (uint8_t)(setup->value & 0xffu);
	uint8_t type = (uint8_t)(setup->value >> 8);

	if (select_descriptor(type, index, &data, &length) == 0u) {
		ep0_stall();
		return;
	}

	if (setup->length < length) {
		length = setup->length;
	}

	usb_descriptor_count++;
	ep0_start_in(data, length);
}

static uint8_t feature_get(const uint8_t *features, uint32_t index)
{
	if (index >= FEATURE_END_INDEX) {
		return 0xffu;
	}

	return features[index];
}

static uint8_t feature_set(uint8_t *features, uint32_t encoded_index_value)
{
	uint32_t index = encoded_index_value & 0xffu;
	uint32_t value = (encoded_index_value >> 8) & 0xffu;

	if ((index <= FEATURE_MINOR_INDEX) || (index >= FEATURE_END_INDEX) || (value >= FEATURE_END_VALUES)) {
		return 0xffu;
	}

	features[index] = (uint8_t)value;
	return features[index];
}

static void ep0_start_in_clipped(const usb_setup_t *setup, const uint8_t *data, uint32_t length)
{
	ep0_start_in(data, min_u32(length, setup->length));
}

static void handle_feature_request(const usb_setup_t *setup)
{
	uint32_t index = setup->index;
	uint32_t length = 1u;

	vendor_response[0] = 0u;

	switch (setup->value) {
	case 0u:
	case 1u:
		vendor_response[0] = 0u;
		break;
	case FEATURE_DG8SAQ_SET_NVRAM:
		vendor_response[0] = feature_set(feature_nvram, index);
		break;
	case FEATURE_DG8SAQ_GET_NVRAM:
		vendor_response[0] = feature_get(feature_nvram, index);
		break;
	case FEATURE_DG8SAQ_SET_RAM:
		vendor_response[0] = feature_set(feature_ram, index);
		break;
	case FEATURE_DG8SAQ_GET_RAM:
		vendor_response[0] = feature_get(feature_ram, index);
		break;
	case FEATURE_DG8SAQ_GET_INDEX:
		length = copy_reversed_string(vendor_response,
			(index <= FEATURE_END_INDEX) ? feature_index_names[index] : "?");
		break;
	case FEATURE_DG8SAQ_GET_VALUE:
		length = copy_reversed_string(vendor_response,
			(index <= FEATURE_END_VALUES) ? feature_value_names[index] : "?");
		break;
	case FEATURE_DG8SAQ_GET_DEFAULT:
		vendor_response[0] = feature_get(feature_defaults, index);
		break;
	default:
		vendor_response[0] = 0xffu;
		break;
	}

	ep0_start_in_clipped(setup, vendor_response, length);
}

static void handle_vendor_request(const usb_setup_t *setup)
{
	if (setup->bm_request_type != 0xc0u) {
		ep0_stall();
		return;
	}

	switch (setup->request) {
	case WIDGET_REQ_RESET:
		vendor_response[0] = 0u;
		pending_device_reset = 1u;
		ep0_start_in_clipped(setup, vendor_response, 1u);
		break;
	case FEATURE_DG8SAQ_COMMAND:
		handle_feature_request(setup);
		break;
	default:
		ep0_stall();
		break;
	}
}

static void handle_audio_class_in_request(const usb_setup_t *setup)
{
	const uint8_t *data = 0;
	uint32_t length = 0u;
	uint32_t recipient = setup->bm_request_type & 0x1fu;
	uint32_t control = setup->value >> 8;
	uint32_t unit = setup->index >> 8;

	if (recipient == 2u) {
		if (control != AUDIO_CONTROL_SAMPLE_FREQ) {
			ep0_stall();
			return;
		}
		switch (setup->request) {
		case AUDIO_REQ_GET_CUR:
		case AUDIO_REQ_GET_MIN:
		case AUDIO_REQ_GET_MAX:
			data = audio_sample_rate_for_format(
				audio_format_for_endpoint_address(setup->index & 0xffu),
				&length);
			break;
		case AUDIO_REQ_GET_RES:
			data = audio_sample_rate_res;
			length = sizeof(audio_sample_rate_res);
			break;
		default:
			ep0_stall();
			return;
		}
		ep0_start_in_clipped(setup, data, length);
		return;
	}

	if ((recipient != 1u) || ((unit != AUDIO_MIC_FEATURE_UNIT) && (unit != AUDIO_SPK_FEATURE_UNIT))) {
		ep0_stall();
		return;
	}

	if (control == AUDIO_CONTROL_MUTE) {
		data = audio_mute_off;
		length = sizeof(audio_mute_off);
	} else if (control == AUDIO_CONTROL_VOLUME) {
		length = sizeof(audio_volume_current);
		switch (setup->request) {
		case AUDIO_REQ_GET_CUR:
			data = audio_volume_current;
			break;
		case AUDIO_REQ_GET_MIN:
			data = audio_volume_min;
			break;
		case AUDIO_REQ_GET_MAX:
			data = audio_volume_max;
			break;
		case AUDIO_REQ_GET_RES:
			data = audio_volume_res;
			break;
		default:
			ep0_stall();
			return;
		}
	} else {
		ep0_stall();
		return;
	}

	ep0_start_in_clipped(setup, data, length);
}

static void handle_class_request(const usb_setup_t *setup)
{
	if ((setup->bm_request_type & 0x80u) != 0u) {
		handle_audio_class_in_request(setup);
		return;
	}

	pending_audio_sample_rate_valid = 0u;
	if ((setup->request == AUDIO_REQ_SET_CUR) && (setup->length <= EP0_SIZE)) {
		if (((setup->bm_request_type & 0x1fu) == 2u) &&
		    ((setup->value >> 8) == AUDIO_CONTROL_SAMPLE_FREQ) &&
		    (setup->length == sizeof(audio_sample_rate_48k))) {
			pending_audio_sample_rate_valid = 1u;
			pending_audio_sample_rate_endpoint = setup->index & 0xffu;
		}
		if (setup->length == 0u) {
			ep0_start_status_in();
		} else {
			ep0_expect_out_data_status();
		}
		return;
	}

	ep0_stall();
}

static void handle_setup(const usb_setup_t *setup)
{
	uint32_t recipient = setup->bm_request_type & 0x1fu;

	last_setup0 = ((uint32_t)setup->bm_request_type << 8) | setup->request;
	last_wvalue = setup->value;
	last_windex = setup->index;
	last_wlength = setup->length;

	if ((setup->bm_request_type & 0x60u) == 0x40u) {
		handle_vendor_request(setup);
		return;
	}

	if ((setup->bm_request_type & 0x60u) == 0x20u) {
		handle_class_request(setup);
		return;
	}

	if ((setup->bm_request_type & 0x60u) != 0u) {
		ep0_stall();
		return;
	}

	switch (setup->request) {
	case USB_REQ_GET_DESCRIPTOR:
		if (setup->bm_request_type != 0x80u) {
			ep0_stall();
			return;
		}
		handle_get_descriptor(setup);
		break;
	case USB_REQ_SET_ADDRESS:
		if ((setup->bm_request_type != 0x00u) || (setup->index != 0u) || (setup->length != 0u) ||
		    (setup->value > 127u)) {
			ep0_stall();
			return;
		}
		pending_address = setup->value;
		pending_address_valid = 1u;
		usb_set_address_count++;
		ep0_start_status_in();
		break;
	case USB_REQ_SET_CONFIGURATION:
		if ((setup->bm_request_type != 0x00u) || (setup->index != 0u) || (setup->length != 0u) ||
		    (setup->value > 1u)) {
			ep0_stall();
			return;
		}
		usb_configuration = setup->value;
		clear_interface_alternates();
		interface_alternate_peak_mask = 0u;
		if (usb_configuration == 0u) {
			disable_audio_endpoints();
		} else {
			configure_audio_endpoints();
		}
		usb_set_configuration_count++;
		ep0_start_status_in();
		break;
	case USB_REQ_GET_CONFIGURATION:
		if (setup->bm_request_type != 0x80u) {
			ep0_stall();
			return;
		}
		ep0_start_in((usb_configuration == 0u) ? one_byte_zero : (const uint8_t *)&usb_configuration, 1u);
		break;
	case USB_REQ_GET_STATUS:
		if (((setup->bm_request_type & 0x80u) == 0u) || (setup->value != 0u) || (setup->length != 2u)) {
			ep0_stall();
			return;
		}
		ep0_start_in((recipient == 0u) ? status_self_powered : status_zero, 2u);
		break;
	case USB_REQ_GET_INTERFACE:
		if ((setup->bm_request_type != 0x81u) || (setup->value != 0u) ||
		    (setup->index >= USB_INTERFACE_COUNT)) {
			ep0_stall();
			return;
		}
		vendor_response[0] = interface_alternate[setup->index];
		ep0_start_in(vendor_response, min_u32(setup->length, 1u));
		break;
	case USB_REQ_SET_INTERFACE:
		if ((setup->bm_request_type != 0x01u) || (setup->length != 0u) ||
		    (setup->index >= USB_INTERFACE_COUNT) ||
		    ((setup->index < 2u) && (setup->value != 0u)) ||
		    ((setup->index >= 2u) && (setup->value > 2u))) {
			ep0_stall();
			return;
		}
		interface_alternate[setup->index] = (uint8_t)setup->value;
		update_interface_alternate_peak();
		usb_set_interface_count++;
		last_set_interface_index = setup->index;
		last_set_interface_value = setup->value;
		if (setup->index >= 2u) {
			set_audio_interface_format(setup->index, setup->value);
			audio_set_interface_count++;
			reset_audio_interface_data_toggle(setup->index);
			if (setup->value != 0u) {
				prime_audio_interface(setup->index);
			}
		}
		ep0_start_status_in();
		break;
	case USB_REQ_CLEAR_FEATURE:
		if ((setup->length != 0u) || (setup->value != 0u)) {
			ep0_stall();
			return;
		}
		ep0_start_status_in();
		break;
	default:
		ep0_stall();
		break;
	}
}

static void poll_setup_packet(void)
{
	volatile uint8_t *fifo = USBHS_EP_FIFO(0u);
	uint8_t raw[8];
	usb_setup_t setup;
	uint32_t index;

	for (index = 0u; index < sizeof(raw); index++) {
		raw[index] = fifo[index];
	}

	setup.bm_request_type = raw[0];
	setup.request = raw[1];
	setup.value = read_le16(&raw[2]);
	setup.index = read_le16(&raw[4]);
	setup.length = read_le16(&raw[6]);

	usb_setup_count++;
	USBHS_DEVEPTIDR(0u) = USBHS_DEVEPTIDR_STALLRQC;
	USBHS_DEVEPTICR(0u) = USBHS_DEVEPTICR_RXSTPIC;
	handle_setup(&setup);
}

void same70_usb_init(void)
{
	CKGR_UCKR = CKGR_UCKR_UPLLEN | CKGR_UCKR_UPLLCOUNT(0xfu);
	wait_for_locku();

	PMC_USB = PMC_USB_USBS_UPLL | PMC_USB_USBDIV(9u);
	PMC_SCER = PMC_SCER_USBCLK;
	PMC_PCER1 = USBHS_PCER1_BIT;

	USBHS_DEVIDR = USBHS_DEVIDR_ALL;
	USBHS_CTRL = USBHS_CTRL_VBUSHWC | USBHS_CTRL_UIMOD_DEV;
	USBHS_CTRL |= USBHS_CTRL_USBE;
	USBHS_CTRL &= ~USBHS_CTRL_FRZCLK;
	wait_for_clkusable();

	USBHS_DEVCTRL = USBHS_DEVCTRL_DETACH | USBHS_DEVCTRL_NORMAL;
	USBHS_DEVIER = USBHS_DEVIER_EORSTES | USBHS_DEVIER_PEP0;
	set_usb_address(0u);
	usb_configuration = 0u;
	clear_interface_alternates();
	configure_ep0();

	usb_initialized = 1u;
	usb_attached = 0u;
}

void same70_usb_attach(void)
{
	if (usb_initialized == 0u) {
		same70_usb_init();
	}

	USBHS_DEVCTRL &= ~USBHS_DEVCTRL_DETACH;
	usb_attached = 1u;
}

void same70_usb_detach(void)
{
	if (usb_initialized != 0u) {
		USBHS_DEVCTRL |= USBHS_DEVCTRL_DETACH;
	}
	usb_attached = 0u;
}

void same70_usb_poll(void)
{
	uint32_t ep0isr;

	if (usb_initialized == 0u) {
		return;
	}

	if ((USBHS_DEVISR & USBHS_DEVISR_EORST) != 0u) {
		usb_reset_count++;
		USBHS_DEVICR = USBHS_DEVICR_EORSTC;
		set_usb_address(0u);
		usb_configuration = 0u;
		clear_interface_alternates();
		interface_alternate_peak_mask = 0u;
		disable_audio_endpoints();
		configure_ep0();
	}

	ep0isr = USBHS_DEVEPTISR(0u);
	if ((ep0isr & USBHS_DEVEPTISR_RXSTPI) != 0u) {
		poll_setup_packet();
		return;
	}

	if (((ep0isr & USBHS_DEVEPTISR_TXINI) != 0u) && (ep0_state == EP0_STATE_TX_DATA)) {
		ep0_send_next_data();
		return;
	}

	if (((ep0isr & USBHS_DEVEPTISR_TXINI) != 0u) && (ep0_state == EP0_STATE_TX_STATUS)) {
		if (pending_address_valid != 0u) {
			set_usb_address(pending_address);
			pending_address_valid = 0u;
		}
		ep0_state = EP0_STATE_IDLE;
		return;
	}

	if ((ep0isr & USBHS_DEVEPTISR_RXOUTI) != 0u) {
		usb_rxout_count++;
		if (ep0_state == EP0_STATE_RX_DATA_STATUS) {
			handle_ep0_out_data(endpoint_byte_count(ep0isr));
		}
		USBHS_DEVEPTICR(0u) = USBHS_DEVEPTICR_RXOUTIC;
		if (ep0_state == EP0_STATE_RX_DATA_STATUS) {
			ep0_start_status_in();
		} else if (ep0_state == EP0_STATE_WAIT_OUT_STATUS) {
			ep0_state = EP0_STATE_IDLE;
			if (pending_device_reset != 0u) {
				system_reset();
			}
		}
	}

	poll_audio_endpoints();
}

void same70_usb_get_status(same70_usb_status_t *status)
{
	status->initialized = usb_initialized;
	status->attached = usb_attached;
	status->pmc_sr = PMC_SR;
	status->pmc_usb = PMC_USB;
	status->pmc_pcsr1 = PMC_PCSR1;
	status->usbhs_ctrl = USBHS_CTRL;
	status->usbhs_sr = USBHS_SR;
	status->devctrl = USBHS_DEVCTRL;
	status->devisr = USBHS_DEVISR;
	status->devept = USBHS_DEVEPT;
	status->ep0cfg = USBHS_DEVEPTCFG(0u);
	status->ep0isr = USBHS_DEVEPTISR(0u);
	status->ep0imr = USBHS_DEVEPTIMR(0u);
	status->ep3cfg = USBHS_DEVEPTCFG(USB_AUDIO_OUT_EP);
	status->ep3isr = USBHS_DEVEPTISR(USB_AUDIO_OUT_EP);
	status->ep4cfg = USBHS_DEVEPTCFG(USB_AUDIO_FB_EP);
	status->ep4isr = USBHS_DEVEPTISR(USB_AUDIO_FB_EP);
	status->ep5cfg = USBHS_DEVEPTCFG(USB_AUDIO_IN_EP);
	status->ep5isr = USBHS_DEVEPTISR(USB_AUDIO_IN_EP);
	status->reset_count = usb_reset_count;
	status->setup_count = usb_setup_count;
	status->tx_count = usb_tx_count;
	status->rxout_count = usb_rxout_count;
	status->stall_count = usb_stall_count;
	status->audio_config_count = audio_config_count;
	status->audio_cfgok_mask = audio_cfgok_mask();
	status->audio_out_count = audio_out_count;
	status->audio_out_bytes = audio_out_bytes;
	status->audio_out_nonzero_bytes = audio_out_nonzero_bytes;
	status->audio_out_last_bytes = audio_out_last_bytes;
	status->audio_out_max_bytes = audio_out_max_bytes;
	status->audio_feedback_count = audio_feedback_count;
	status->audio_feedback_bytes = audio_feedback_bytes;
	status->audio_feedback_busy_last = audio_feedback_busy_last;
	status->audio_feedback_busy_max = audio_feedback_busy_max;
	status->audio_in_count = audio_in_count;
	status->audio_in_bytes = audio_in_bytes;
	status->audio_in_busy_last = audio_in_busy_last;
	status->audio_in_busy_max = audio_in_busy_max;
	status->audio_error_count = audio_error_count;
	status->audio_short_count = audio_short_count;
	status->audio_crc_count = audio_crc_count;
	status->audio_overflow_count = audio_overflow_count;
	status->audio_underflow_count = audio_underflow_count;
	status->audio_source_mode = audio_source_mode;
	status->audio_out_format = audio_out_format;
	status->audio_in_format = audio_in_format;
	status->audio_loopback_level = audio_loopback_level;
	status->audio_loopback_peak = audio_loopback_peak;
	status->audio_loopback_drop_bytes = audio_loopback_drop_bytes;
	status->audio_loopback_silence_bytes = audio_loopback_silence_bytes;
	status->descriptor_count = usb_descriptor_count;
	status->set_address_count = usb_set_address_count;
	status->set_configuration_count = usb_set_configuration_count;
	status->set_interface_count = usb_set_interface_count;
	status->audio_set_interface_count = audio_set_interface_count;
	status->interface_alternate_mask = interface_alternate_mask();
	status->interface_alternate_peak_mask = interface_alternate_peak_mask;
	status->last_set_interface_index = last_set_interface_index;
	status->last_set_interface_value = last_set_interface_value;
	status->address = usb_address;
	status->configuration = usb_configuration;
	status->ep0_state = (uint32_t)ep0_state;
	status->last_setup0 = last_setup0;
	status->last_wvalue = last_wvalue;
	status->last_windex = last_windex;
	status->last_wlength = last_wlength;
}

uint32_t same70_usb_set_audio_source(uint32_t source)
{
	if (source > SAME70_USB_AUDIO_SOURCE_SINE) {
		return 0u;
	}

	audio_source_mode = source;
	reset_audio_loopback_buffer();
	reset_audio_pattern();
	reset_audio_tone();
	reset_audio_sine();
	return 1u;
}
