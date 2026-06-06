#ifndef SAME70_XPLAINED_CLOCK_H
#define SAME70_XPLAINED_CLOCK_H

#include "types.h"

#define SAME70_CPU_CLOCK_HZ 300000000u
#define SAME70_MASTER_CLOCK_HZ 150000000u

typedef struct {
	uint32_t ckgr_mor;
	uint32_t ckgr_pllar;
	uint32_t pmc_mckr;
	uint32_t ckgr_uckr;
	uint32_t pmc_usb;
	uint32_t pmc_scsr;
	uint32_t pmc_sr;
	uint32_t utmi_cktrim;
	uint32_t efc_fmr;
} same70_clock_status_t;

void same70_clock_init(void);
void same70_clock_get_status(same70_clock_status_t *status);

#endif
