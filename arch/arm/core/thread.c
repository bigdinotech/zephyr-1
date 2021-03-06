/*
 * Copyright (c) 2013-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief New thread creation for ARM Cortex-M
 *
 * Core thread related primitives for the ARM Cortex-M processor architecture.
 */

#include <kernel.h>
#include <toolchain.h>
#include <kernel_structs.h>
#include <wait_q.h>
#ifdef CONFIG_INIT_STACKS
#include <string.h>
#endif /* CONFIG_INIT_STACKS */

/**
 *
 * @brief Initialize a new thread from its stack space
 *
 * The control structure (thread) is put at the lower address of the stack. An
 * initial context, to be "restored" by __pendsv(), is put at the other end of
 * the stack, and thus reusable by the stack when not needed anymore.
 *
 * The initial context is an exception stack frame (ESF) since exiting the
 * PendSV exception will want to pop an ESF. Interestingly, even if the lsb of
 * an instruction address to jump to must always be set since the CPU always
 * runs in thumb mode, the ESF expects the real address of the instruction,
 * with the lsb *not* set (instructions are always aligned on 16 bit halfwords).
 * Since the compiler automatically sets the lsb of function addresses, we have
 * to unset it manually before storing it in the 'pc' field of the ESF.
 *
 * <options> is currently unused.
 *
 * @param pStackMem the aligned stack memory
 * @param stackSize stack size in bytes
 * @param pEntry the entry point
 * @param parameter1 entry point to the first param
 * @param parameter2 entry point to the second param
 * @param parameter3 entry point to the third param
 * @param priority thread priority
 * @param options thread options: K_ESSENTIAL, K_FP_REGS
 *
 * @return N/A
 */

void _new_thread(struct k_thread *thread, k_thread_stack_t stack,
		 size_t stackSize, _thread_entry_t pEntry,
		 void *parameter1, void *parameter2, void *parameter3,
		 int priority, unsigned int options)
{
	char *pStackMem = K_THREAD_STACK_BUFFER(stack);

	_ASSERT_VALID_PRIO(priority, pEntry);

	__ASSERT(!((u32_t)pStackMem & (STACK_ALIGN - 1)),
		 "stack is not aligned properly\n"
		 "%d-byte alignment required\n", STACK_ALIGN);

	char *stackEnd = pStackMem + stackSize;
	struct __esf *pInitCtx;

	_new_thread_init(thread, pStackMem, stackSize, priority, options);

	/* carve the thread entry struct from the "base" of the stack */

	pInitCtx = (struct __esf *)(STACK_ROUND_DOWN(stackEnd) -
				    sizeof(struct __esf));

	pInitCtx->pc = ((u32_t)_thread_entry) & 0xfffffffe;
	pInitCtx->a1 = (u32_t)pEntry;
	pInitCtx->a2 = (u32_t)parameter1;
	pInitCtx->a3 = (u32_t)parameter2;
	pInitCtx->a4 = (u32_t)parameter3;
	pInitCtx->xpsr =
		0x01000000UL; /* clear all, thumb bit is 1, even if RO */

#ifdef CONFIG_THREAD_MONITOR
	/*
	 * In debug mode thread->entry give direct access to the thread entry
	 * and the corresponding parameters.
	 */
	thread->entry = (struct __thread_entry *)(pInitCtx);
#endif

	thread->callee_saved.psp = (u32_t)pInitCtx;
	thread->arch.basepri = 0;

	/* swap_return_value can contain garbage */

	/*
	 * initial values in all other registers/thread entries are
	 * irrelevant.
	 */

	thread_monitor_init(thread);
}
