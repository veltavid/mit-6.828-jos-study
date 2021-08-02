// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	int r;

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at uvpt
	//   (see <inc/memlayout.h>).

	// LAB 4: Your code here.
	uintptr_t mapped_va=(uintptr_t)addr;
	envid_t c_envid;
	mapped_va=((uintptr_t)(uvpt)&0xffc00000)+(mapped_va>>12)*4;
	if(!(err & FEC_WR) || !(*(pte_t *)mapped_va & PTE_COW))
	panic("pgfault: Bad access to the page, fault va %p\n",utf->utf_fault_va);

	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.

	// LAB 4: Your code here.
	c_envid=sys_getenvid();
	r=sys_page_alloc(c_envid,(void *)PFTEMP,PTE_U | PTE_P | PTE_W);
	if(r<0)
	panic("sys_page_alloc: %e",r);
	addr=(void *)ROUNDDOWN((uintptr_t)addr,PGSIZE);
	memmove((void *)PFTEMP,addr,PGSIZE);
	r=sys_page_map(c_envid,(void *)PFTEMP,c_envid,addr,PTE_P | PTE_U | PTE_W);
	if(r<0)
	panic("sys_page_map: %e",r);
}

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why do we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
//
static int
duppage(envid_t envid, unsigned pn)
{
	int r;

	// LAB 4: Your code here.
	int perm=PTE_U | PTE_P;
	uintptr_t va=pn*PGSIZE;
	pte_t *mapped_va;
	envid_t c_envid;
	mapped_va=(pte_t *)(((uintptr_t)(uvpt)&0xffc00000)+(va>>12)*4);
	if((*mapped_va & PTE_W) || (*mapped_va & PTE_COW))
	perm |=PTE_COW;
	c_envid=sys_getenvid();
	r=sys_page_map(c_envid,(void *)va,envid,(void *)va,perm);
	if(r<0)
	panic("sys_page_map: %e",r);
	if(perm & PTE_COW)
	{
		r=sys_page_map(c_envid,(void *)va,c_envid,(void *)va,perm);
		if(r<0)
		panic("sys_page_map: %e",r);
	}
	return 0;
}

//
// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use uvpd, uvpt, and duppage.
//   Remember to fix "thisenv" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
envid_t
fork(void)
{
	// LAB 4: Your code here.
	int envid;
	int pg;
	uintptr_t addr;
	extern unsigned char end[];
	extern void _pgfault_upcall(void);
	
	set_pgfault_handler(pgfault);
	envid=sys_exofork();
	if(envid<0)
	panic("sys_exfork: %e",envid);
	if(envid==0)
	{
		thisenv = &envs[ENVX(sys_getenvid())];
		return 0;
	}
	for(pg=0;UTEXT+pg*PGSIZE<(uintptr_t)end;pg++)
	{
		addr=(uintptr_t)uvpt+((UTEXT+pg*PGSIZE)>>12)*4;
		if(*(pte_t *)addr)
		duppage(envid,pg+UTEXT/PGSIZE);
	}
	duppage(envid,(USTACKTOP-PGSIZE)/PGSIZE);
	sys_page_alloc(envid,(void *)(UXSTACKTOP-PGSIZE),PTE_U | PTE_P | PTE_W);
	sys_env_set_pgfault_upcall(envid,_pgfault_upcall);
	sys_env_set_status(envid,ENV_RUNNABLE);
	return envid;
}

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}
