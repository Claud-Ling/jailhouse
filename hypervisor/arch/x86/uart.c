/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2013-2016
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <jailhouse/control.h>
#include <jailhouse/mmio.h>
#include <jailhouse/printk.h>
#include <jailhouse/processor.h>
#include <asm/io.h>
#include <asm/uart.h>
#include <asm/vga.h>

#define UART_TX			0x0
#define UART_DLL		0x0
#define UART_DLM		0x1
#define UART_LCR		0x3
#define UART_LCR_8N1		0x03
#define UART_LCR_DLAB		0x80
#define UART_LSR		0x5
#define UART_LSR_THRE		0x20

u64 uart_base;

static void uart_pio_out(unsigned int reg, u8 value)
{
	outb(value, uart_base + reg);
}

static u8 uart_pio_in(unsigned int reg)
{
	return inb(uart_base + reg);
}

static void uart_mmio8_out(unsigned int reg, u8 value)
{
	mmio_write8((void *)(uart_base + reg), value);
}

static u8 uart_mmio8_in(unsigned int reg)
{
	return mmio_read8((void *)(uart_base + reg));
}

static void uart_mmio32_out(unsigned int reg, u8 value)
{
	mmio_write32((void *)(uart_base + reg * 4), value);
}

static u8 uart_mmio32_in(unsigned int reg)
{
	return mmio_read32((void *)(uart_base + reg * 4));
}

static void (*uart_reg_out)(unsigned int, u8) = uart_pio_out;
static u8 (*uart_reg_in)(unsigned int) = uart_pio_in;

void uart_init(void)
{
	u64 flags = system_config->debug_console.flags;

	if (system_config->debug_console.phys_start == 0)
		return;

	if (flags & JAILHOUSE_MEM_IO) {
		if (system_config->debug_console.phys_start < VGA_LIMIT)
			return; /* VGA memory */

		if (flags & JAILHOUSE_MEM_IO_32) {
			uart_reg_out = uart_mmio32_out;
			uart_reg_in = uart_mmio32_in;
		} else {
			uart_reg_out = uart_mmio8_out;
			uart_reg_in = uart_mmio8_in;
		}
		uart_base = (u64)hypervisor_header.debug_console_base;
	} else {
		uart_base = system_config->debug_console.phys_start;
	}

	uart_reg_out(UART_LCR, UART_LCR_DLAB);
#ifdef CONFIG_SERIAL_OXPCIE952
	outb(0x22, uart_base + UART_DLL);
#else
	uart_reg_out(UART_DLL, 1);
#endif
	uart_reg_out(UART_DLM, 0);
	uart_reg_out(UART_LCR, UART_LCR_8N1);
}

void uart_write(const char *msg)
{
	char c = 0;

	while (1) {
		if (c == '\n')
			c = '\r';
		else
			c = *msg++;
		if (!c)
			break;
		while (!(uart_reg_in(UART_LSR) & UART_LSR_THRE))
			cpu_relax();
		if (panic_in_progress && panic_cpu != phys_processor_id())
			break;
		uart_reg_out(UART_TX, c);
	}
}
