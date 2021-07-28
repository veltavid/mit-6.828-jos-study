/* See COPYRIGHT for copyright information. */

#include <inc/mmu.h>
#include <inc/memlayout.h>
#include <inc/trap.h>



###################################################################
# exceptions/interrupts
###################################################################

/* TRAPHANDLER defines a globally-visible function for handling a trap.
 * It pushes a trap number onto the stack, then jumps to _alltraps.
 * Use TRAPHANDLER for traps where the CPU automatically pushes an error code.
 *
 * You shouldn't call a TRAPHANDLER function from C, but you may
 * need to _declare_ one in C (for instance, to get a function pointer
 * during IDT setup).  You can declare the function with
 *   void NAME();
 * where NAME is the argument passed to TRAPHANDLER.
 */
#define TRAPHANDLER(name, num)						\
	.globl name;		/* define global symbol for 'name' */	\
	.type name, @function;	/* symbol type is function */		\
	.align 2;		/* align function definition */		\
	name:			/* function starts here */		\
	pushl $(num);							\
	jmp _alltraps

/* Use TRAPHANDLER_NOEC for traps where the CPU doesn't push an error code.
 * It pushes a 0 in place of the error code, so the trap frame has the same
 * format in either case.
 */
#define TRAPHANDLER_NOEC(name, num)					\
	.globl name;							\
	.type name, @function;						\
	.align 2;							\
	name:								\
	pushl $0;							\
	pushl $(num);							\
	jmp _alltraps

.text

/*
 * Lab 3: Your code here for generating entry points for the different traps.
 */

TRAPHANDLER_NOEC("DIVIDE_handler",T_DIVIDE)
TRAPHANDLER_NOEC("DEBUG_handler",T_DEBUG)
TRAPHANDLER_NOEC("NMI_handler",T_NMI)
TRAPHANDLER_NOEC("BRKPT_handler",T_BRKPT)
TRAPHANDLER_NOEC("OFLOW_handler",T_OFLOW)
TRAPHANDLER_NOEC("BOUND_handler",T_BOUND)
TRAPHANDLER_NOEC("ILLOP_handler",T_ILLOP)
TRAPHANDLER_NOEC("DEVICE_handler",T_DEVICE)
TRAPHANDLER("DBLFLT_handler",T_DBLFLT)
TRAPHANDLER("TSS_handler",T_TSS)
TRAPHANDLER("SEGNP_handler",T_SEGNP)
TRAPHANDLER("STACK_handler",T_STACK)
TRAPHANDLER("GPFLT_handler",T_GPFLT)
TRAPHANDLER("PGFLT_handler",T_PGFLT)
TRAPHANDLER_NOEC("FPERR_handler",T_FPERR)
TRAPHANDLER("ALIGN_handler",T_ALIGN)
TRAPHANDLER_NOEC("MCHK_handler",T_MCHK)
TRAPHANDLER_NOEC("SIMDERR_handler",T_SIMDERR)

TRAPHANDLER_NOEC("SYSCALL_handler",T_SYSCALL)

/*
 * Lab 3: Your code here for _alltraps
 */
_alltraps:
	pushw $0
	pushw %ds
	pushw $0
	pushw %es
	pushal
	pushw $GD_KD
	pushw $GD_KD
	popw %ds
	popw %es
	pushl %esp
	call trap
