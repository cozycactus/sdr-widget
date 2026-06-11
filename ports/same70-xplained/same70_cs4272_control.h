#ifndef SAME70_XPLAINED_CS4272_CONTROL_H
#define SAME70_XPLAINED_CS4272_CONTROL_H

#include "types.h"

#define SAME70_CS4272_I2C_ADDRESS_7BIT 0x10u
#define SAME70_CS4272_TWI_HZ 100000u
#define SAME70_CS4272_BASELINE_WRITE_COUNT 8u

typedef struct {
	uint8_t reg;
	uint8_t value;
	uint8_t rate_dependent;
} same70_cs4272_register_write_t;

typedef struct {
	uint32_t control_enabled;
	uint32_t bus_configured;
	uint32_t reset_configured;
	uint32_t external_codec;
	uint32_t address_7bit;
	uint32_t twi_hz;
	uint32_t baseline_write_count;
	const char *bus_plan;
	const char *reset_plan;
	const char *power_plan;
	const char *startup_sequence;
	const char *mode1_table;
	const char *policy;
	const char *boundary;
	const char *next_step;
} same70_cs4272_control_status_t;

void same70_cs4272_control_get_status(same70_cs4272_control_status_t *status);
const same70_cs4272_register_write_t *same70_cs4272_control_baseline_writes(void);
uint8_t same70_cs4272_control_mode1_for_rate(uint32_t sample_rate_hz);

#endif
