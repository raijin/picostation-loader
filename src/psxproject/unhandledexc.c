#include <stdint.h>
#include <stdio.h>
#include "system.h"

#ifndef NDEBUG
static const char *const _causeNames[] = {
	"Load address error",
	"Store address error",
	"Instruction bus error",
	"Data bus error",
	"Syscall",
	"Break instruction",
	"Reserved instruction",
	"Coprocessor unusable",
	"Arithmetic overflow"
};

static const char _registerNames[] =
	"pc" "at" "v0" "v1" "a0" "a1" "a2" "a3"
	"t0" "t1" "t2" "t3" "t4" "t5" "t6" "t7"
	"s0" "s1" "s2" "s3" "s4" "s5" "s6" "s7"
	"t8" "t9" "gp" "sp" "fp" "ra" "hi" "lo";

static const char _hexDigits[] = "0123456789abcdef";

static void _printHexValue(uint32_t value) {
	for (int i = 8; i; i--, value <<= 4)
		putchar(_hexDigits[value >> 28]);
}
#endif

void _unhandledException(int cause, uint32_t badv) {
#ifndef NDEBUG
	puts(_causeNames[cause - 4]);

	if (cause <= 5) {
		putchar('@');
		putchar(' ');
		_printHexValue(badv);
		putchar('\n');
	}

	puts("Register dump:");

	const char *name = _registerNames;
	uint32_t   *reg  = (uint32_t *) &(currentThread->pc);

	for (int i = 31; i >= 0; i--) {
		putchar(' ');
		putchar(' ');
		putchar(*(name++));
		putchar(*(name++));
		putchar('=');
		_printHexValue(*(reg++));

		if (!(i % 4))
			putchar('\n');
	}

	puts("Stack dump:");

	uint32_t *addr = ((uint32_t *) currentThread->sp) - 7;
	uint32_t *end  = ((uint32_t *) currentThread->sp) + 7;

	for (; addr <= end; addr++) {
		if (((uint32_t) addr) == currentThread->sp)
			putchar('>');
		else
			putchar(' ');

		putchar(' ');
		_printHexValue((uint32_t) addr);
		putchar(':');
		putchar(' ');
		_printHexValue(*addr);
		putchar('\n');
	}
#endif

	for (;;)
		__asm__ volatile("");
}
