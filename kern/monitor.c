// Simple command-line kernel monitor useful for
// controlling the kernel and exploring the system interactively.

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/assert.h>
#include <inc/x86.h>

#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/kdebug.h>

#define CMDBUF_SIZE	80	// enough for one VGA text line
#define get32(addr) ((uint32_t*)(*(uint32_t*)addr))

struct Command {
	const char *name;
	const char *desc;
	// return -1 to force monitor to exit
	int (*func)(int argc, char** argv, struct Trapframe* tf);
};

static struct Command commands[] = {
	{ "help", "Display this list of commands", mon_help },
	{ "kerninfo", "Display information about the kernel", mon_kerninfo },
	{ "backtrace", "Display information about the stack", mon_backtrace }
};

/***** Implementations of basic kernel monitor commands *****/

int
mon_help(int argc, char **argv, struct Trapframe *tf)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(commands); i++)
		cprintf("%s - %s\n", commands[i].name, commands[i].desc);
	return 0;
}

int
mon_kerninfo(int argc, char **argv, struct Trapframe *tf)
{
	extern char _start[], entry[], etext[], edata[], end[];

	cprintf("Special kernel symbols:\n");
	cprintf("  _start                  %08x (phys)\n", _start);
	cprintf("  entry  %08x (virt)  %08x (phys)\n", entry, entry - KERNBASE);
	cprintf("  etext  %08x (virt)  %08x (phys)\n", etext, etext - KERNBASE);
	cprintf("  edata  %08x (virt)  %08x (phys)\n", edata, edata - KERNBASE);
	cprintf("  end    %08x (virt)  %08x (phys)\n", end, end - KERNBASE);
	cprintf("Kernel executable memory footprint: %dKB\n",
		ROUNDUP(end - entry, 1024) / 1024);
	return 0;
}

int
mon_backtrace(int argc, char **argv, struct Trapframe *tf)
{
	int temp=0;
	uint32_t ebp,eip,arg1,arg2,arg3,arg4,arg5;
	struct Eipdebuginfo info;
	char func_name[64];
	int func_line;
	ebp=((uint32_t)&temp)+0x1c;
	eip=((uint32_t)&temp)+0x20;
	arg1=((uint32_t)&temp)+0x24;
	arg2=((uint32_t)&temp)+0x28;
	arg3=((uint32_t)&temp)+0x2c;
	arg4=((uint32_t)&temp)+0x30;
	arg5=((uint32_t)&temp)+0x34;
	cprintf("Stack backtrace:\n");
	while(1)
	{
		debuginfo_eip((uintptr_t)get32(eip), &info);
		func_line=(uint32_t)get32(eip)-(uint32_t)info.eip_fn_addr+1;
		strncpy(func_name,info.eip_fn_name,info.eip_fn_namelen);
		*(func_name+info.eip_fn_namelen)='\0';

		cprintf("  ebp %08x  eip %08x  args %08x %08x %08x %08x %08x\n",ebp,get32(eip),get32(arg1),get32(arg2),get32(arg3),\
		get32(arg4),get32(arg5));
		cprintf("         %s:%d: %s+%d\n",info.eip_file,info.eip_line,func_name,func_line);
		if(!get32(ebp))
		break;
		ebp=(uint32_t)get32(ebp);
		eip=ebp+4;
		arg1=ebp+8;
		arg2=ebp+0xc;
		arg3=ebp+0x10;
		arg4=ebp+0x14;
		arg5=ebp+0x18;
	}
	return 0;
}



/***** Kernel monitor command interpreter *****/

#define WHITESPACE "\t\r\n "
#define MAXARGS 16

static int
runcmd(char *buf, struct Trapframe *tf)
{
	int argc;
	char *argv[MAXARGS];
	int i;

	// Parse the command buffer into whitespace-separated arguments
	argc = 0;
	argv[argc] = 0;
	while (1) {
		// gobble whitespace
		while (*buf && strchr(WHITESPACE, *buf))
			*buf++ = 0;
		if (*buf == 0)
			break;

		// save and scan past next arg
		if (argc == MAXARGS-1) {
			cprintf("Too many arguments (max %d)\n", MAXARGS);
			return 0;
		}
		argv[argc++] = buf;
		while (*buf && !strchr(WHITESPACE, *buf))
			buf++;
	}
	argv[argc] = 0;

	// Lookup and invoke the command
	if (argc == 0)
		return 0;
	for (i = 0; i < ARRAY_SIZE(commands); i++) {
		if (strcmp(argv[0], commands[i].name) == 0)
			return commands[i].func(argc, argv, tf);
	}
	cprintf("Unknown command '%s'\n", argv[0]);
	return 0;
}

void
monitor(struct Trapframe *tf)
{
	char *buf;

	cprintf("Welcome to the JOS kernel monitor!\n");
	cprintf("Type 'help' for a list of commands.\n");


	while (1) {
		buf = readline("K> ");
		if (buf != NULL)
			if (runcmd(buf, tf) < 0)
				break;
	}
}
