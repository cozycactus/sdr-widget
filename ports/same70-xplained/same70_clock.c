#include "same70_clock.h"

#define REG32(addr) (*(volatile uint32_t *)(addr))

#define UTMI_CKTRIM REG32(0x400E0430u)

#define EEFC_FMR    REG32(0x400E0C00u)

#define PMC_SCER    REG32(0x400E0600u)
#define PMC_SCSR    REG32(0x400E0608u)
#define CKGR_UCKR   REG32(0x400E061Cu)
#define CKGR_MOR    REG32(0x400E0620u)
#define CKGR_PLLAR  REG32(0x400E0628u)
#define PMC_MCKR    REG32(0x400E0630u)
#define PMC_USB     REG32(0x400E0638u)
#define PMC_SR      REG32(0x400E0668u)

#define EEFC_FMR_FWS_MASK       (0xfu << 8)
#define EEFC_FMR_FWS(x)         ((x) << 8)
#define EEFC_FMR_CLOE           (1u << 26)

#define UTMI_CKTRIM_FREQ_XTAL12 0u

#define PMC_SCER_USBCLK         (1u << 5)

#define CKGR_UCKR_UPLLEN        (1u << 16)
#define CKGR_UCKR_UPLLCOUNT(x)  ((x) << 20)

#define CKGR_MOR_MOSCXTEN       (1u << 0)
#define CKGR_MOR_MOSCRCEN       (1u << 3)
#define CKGR_MOR_MOSCRCF_MASK   (7u << 4)
#define CKGR_MOR_MOSCRCF_12MHZ  (2u << 4)
#define CKGR_MOR_MOSCXTST_MASK  (0xffu << 8)
#define CKGR_MOR_MOSCXTST(x)    ((x) << 8)
#define CKGR_MOR_KEY_PASSWD     (0x37u << 16)
#define CKGR_MOR_MOSCSEL        (1u << 24)

#define CKGR_PLLAR_DIVA(x)      ((x) << 0)
#define CKGR_PLLAR_PLLACOUNT(x) ((x) << 8)
#define CKGR_PLLAR_MULA(x)      ((x) << 16)
#define CKGR_PLLAR_ONE          (1u << 29)

#define PMC_MCKR_CSS_MASK       (3u << 0)
#define PMC_MCKR_CSS_PLLA       (2u << 0)
#define PMC_MCKR_PRES_MASK      (7u << 4)
#define PMC_MCKR_PRES_CLK_1     (0u << 4)
#define PMC_MCKR_MDIV_MASK      (3u << 8)
#define PMC_MCKR_MDIV_PCK_DIV2  (1u << 8)
#define PMC_MCKR_UPLLDIV2       (1u << 13)

#define PMC_USB_USBS_UPLL       (1u << 0)
#define PMC_USB_USBDIV(x)       ((x) << 8)

#define PMC_SR_MOSCXTS          (1u << 0)
#define PMC_SR_LOCKA            (1u << 1)
#define PMC_SR_MCKRDY           (1u << 3)
#define PMC_SR_LOCKU            (1u << 6)
#define PMC_SR_MOSCSELS         (1u << 16)

static void wait_for_pmc_status(uint32_t mask)
{
	while ((PMC_SR & mask) != mask) {
	}
}

void same70_clock_init(void)
{
	uint32_t mor;

	EEFC_FMR = (EEFC_FMR & ~EEFC_FMR_FWS_MASK) | EEFC_FMR_FWS(6u) | EEFC_FMR_CLOE;

	mor = CKGR_MOR_KEY_PASSWD |
		CKGR_MOR_MOSCRCEN |
		CKGR_MOR_MOSCRCF_12MHZ |
		CKGR_MOR_MOSCXTST(0xffu) |
		CKGR_MOR_MOSCXTEN;
	CKGR_MOR = mor;
	wait_for_pmc_status(PMC_SR_MOSCXTS);

	CKGR_MOR = mor | CKGR_MOR_MOSCSEL;
	wait_for_pmc_status(PMC_SR_MOSCSELS);

	CKGR_PLLAR = CKGR_PLLAR_ONE |
		CKGR_PLLAR_PLLACOUNT(0x3fu) |
		CKGR_PLLAR_MULA(24u) |
		CKGR_PLLAR_DIVA(1u);
	wait_for_pmc_status(PMC_SR_LOCKA);

	UTMI_CKTRIM = UTMI_CKTRIM_FREQ_XTAL12;
	CKGR_UCKR = CKGR_UCKR_UPLLEN | CKGR_UCKR_UPLLCOUNT(0xfu);
	wait_for_pmc_status(PMC_SR_LOCKU);

	PMC_MCKR &= ~PMC_MCKR_UPLLDIV2;
	wait_for_pmc_status(PMC_SR_MCKRDY);

	PMC_MCKR = (PMC_MCKR & ~PMC_MCKR_PRES_MASK) | PMC_MCKR_PRES_CLK_1;
	wait_for_pmc_status(PMC_SR_MCKRDY);

	PMC_MCKR = (PMC_MCKR & ~PMC_MCKR_MDIV_MASK) | PMC_MCKR_MDIV_PCK_DIV2;
	wait_for_pmc_status(PMC_SR_MCKRDY);

	PMC_MCKR = (PMC_MCKR & ~PMC_MCKR_CSS_MASK) | PMC_MCKR_CSS_PLLA;
	wait_for_pmc_status(PMC_SR_MCKRDY);

	PMC_USB = PMC_USB_USBS_UPLL | PMC_USB_USBDIV(9u);
	PMC_SCER = PMC_SCER_USBCLK;
}

void same70_clock_get_status(same70_clock_status_t *status)
{
	status->ckgr_mor = CKGR_MOR;
	status->ckgr_pllar = CKGR_PLLAR;
	status->pmc_mckr = PMC_MCKR;
	status->ckgr_uckr = CKGR_UCKR;
	status->pmc_usb = PMC_USB;
	status->pmc_scsr = PMC_SCSR;
	status->pmc_sr = PMC_SR;
	status->utmi_cktrim = UTMI_CKTRIM;
	status->efc_fmr = EEFC_FMR;
}
