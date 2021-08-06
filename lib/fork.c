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
	uintptr_t ptep=(uintptr_t)addr;
	
	ptep=(uintptr_t)&uvpt[PGNUM(ptep)];
	if(!(err & FEC_WR) || !(*(pte_t *)ptep & PTE_COW))
	panic("pgfault: Bad access to the page, fault va %p\n",utf->utf_fault_va);

	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.

	// LAB 4: Your code here.
	r=sys_page_alloc(0,(void *)PFTEMP,PTE_U | PTE_P | PTE_W);
	if(r<0)
	panic("sys_page_alloc: %e",r);
	addr=(void *)ROUNDDOWN((uintptr_t)addr,PGSIZE);
	memmove((void *)PFTEMP,addr,PGSIZE);
	r=sys_page_map(0,(void *)PFTEMP,0,addr,PTE_P | PTE_U | PTE_W);
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
	pte_t *ptep;
	
	ptep=(pte_t *)&uvpt[PGNUM(va)];
	if(*ptep & PTE_SHARE)
	{
		/*r=sys_page_alloc(0,(void *)PFTEMP,PTE_SYSCALL & *ptep);
		if(r<0)
		panic("sys_page_alloc: %e",r);
		memmove((void *)PFTEMP,(void *)va,PGSIZE);*/
		r=sys_page_map(0,(void *)va,envid,(void *)va,PTE_SYSCALL & *ptep);
		if(r<0)
		panic("sys_page_map: %e",r);
	}
	else
	{
		if((*ptep & PTE_W) || (*ptep & PTE_COW))
		perm |=PTE_COW;
		r=sys_page_map(0,(void *)va,envid,(void *)va,perm);
		if(r<0)
		panic("sys_page_map: %e",r);
		if(perm & PTE_COW)
		{
			r=sys_page_map(0,(void *)va,0,(void *)va,perm);
			if(r<0)
			panic("sys_page_map: %e",r);
		}
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
	uint32_t pg;
	uintptr_t masked_pa;
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
	for(pg=0;pg*PGSIZE<UXSTACKTOP-PGSIZE;pg++)
	{
		if(uvpd[PDX(pg*PGSIZE)])
		{
			masked_pa=uvpt[PGNUM(pg*PGSIZE)];
			if(masked_pa)
			duppage(envid,pg);
		}
	}
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
