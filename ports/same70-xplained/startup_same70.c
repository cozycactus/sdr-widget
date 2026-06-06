#include "types.h"

extern uint32_t _estack;
extern uint32_t _sidata;
extern uint32_t _sdata;
extern uint32_t _edata;
extern uint32_t _sbss;
extern uint32_t _ebss;

int main(void);
void Reset_Handler(void);
void Default_Handler(void);

void NMI_Handler(void) __attribute__((weak, alias("Default_Handler")));
void HardFault_Handler(void) __attribute__((weak, alias("Default_Handler")));
void MemManage_Handler(void) __attribute__((weak, alias("Default_Handler")));
void BusFault_Handler(void) __attribute__((weak, alias("Default_Handler")));
void UsageFault_Handler(void) __attribute__((weak, alias("Default_Handler")));
void SVC_Handler(void) __attribute__((weak, alias("Default_Handler")));
void DebugMon_Handler(void) __attribute__((weak, alias("Default_Handler")));
void PendSV_Handler(void) __attribute__((weak, alias("Default_Handler")));
void SysTick_Handler(void) __attribute__((weak, alias("Default_Handler")));
void SUPC_Handler(void) __attribute__((weak, alias("Default_Handler")));
void RSTC_Handler(void) __attribute__((weak, alias("Default_Handler")));
void RTC_Handler(void) __attribute__((weak, alias("Default_Handler")));
void RTT_Handler(void) __attribute__((weak, alias("Default_Handler")));
void WDT_Handler(void) __attribute__((weak, alias("Default_Handler")));
void PMC_Handler(void) __attribute__((weak, alias("Default_Handler")));
void EFC_Handler(void) __attribute__((weak, alias("Default_Handler")));
void UART0_Handler(void) __attribute__((weak, alias("Default_Handler")));
void UART1_Handler(void) __attribute__((weak, alias("Default_Handler")));
void SMC_Handler(void) __attribute__((weak, alias("Default_Handler")));
void PIOA_Handler(void) __attribute__((weak, alias("Default_Handler")));
void PIOB_Handler(void) __attribute__((weak, alias("Default_Handler")));
void PIOC_Handler(void) __attribute__((weak, alias("Default_Handler")));
void USART0_Handler(void) __attribute__((weak, alias("Default_Handler")));
void USART1_Handler(void) __attribute__((weak, alias("Default_Handler")));

__attribute__((section(".isr_vector"), used))
void (* const vector_table[])(void) = {
	(void (*)(void))(&_estack),
	Reset_Handler,
	NMI_Handler,
	HardFault_Handler,
	MemManage_Handler,
	BusFault_Handler,
	UsageFault_Handler,
	0,
	0,
	0,
	0,
	SVC_Handler,
	DebugMon_Handler,
	0,
	PendSV_Handler,
	SysTick_Handler,
	SUPC_Handler,
	RSTC_Handler,
	RTC_Handler,
	RTT_Handler,
	WDT_Handler,
	PMC_Handler,
	EFC_Handler,
	UART0_Handler,
	UART1_Handler,
	SMC_Handler,
	PIOA_Handler,
	PIOB_Handler,
	PIOC_Handler,
	USART0_Handler,
	USART1_Handler,
};

void Reset_Handler(void)
{
	uint32_t *src = &_sidata;
	uint32_t *dst = &_sdata;

	while (dst < &_edata) {
		*dst++ = *src++;
	}

	for (dst = &_sbss; dst < &_ebss; dst++) {
		*dst = 0;
	}

	main();

	for (;;) {
	}
}

void Default_Handler(void)
{
	for (;;) {
	}
}
