// User-level IPC library routines

#include <inc/lib.h>

// Receive a value via IPC and return it.
// If 'pg' is nonnull, then any page sent by the sender will be mapped at
//	that address.
// If 'from_env_store' is nonnull, then store the IPC sender's envid in
//	*from_env_store.
// If 'perm_store' is nonnull, then store the IPC sender's page permission
//	in *perm_store (this is nonzero iff a page was successfully
//	transferred to 'pg').
// If the system call fails, then store 0 in *fromenv and *perm (if
//	they're nonnull) and return the error.
// Otherwise, return the value sent by the sender
//
// Hint:
//   Use 'thisenv' to discover the value and who sent it.
//   If 'pg' is null, pass sys_ipc_recv a value that it will understand
//   as meaning "no page".  (Zero is not the right value, since that's
//   a perfectly valid place to map a page.)
int32_t
ipc_recv(envid_t *from_env_store, void *pg, int *perm_store)
{
	// LAB 4: Your code here.
	int32_t result;
	if((uintptr_t)pg<UTOP)
	result=sys_ipc_recv(pg);
	else
	result=sys_ipc_recv((void *)(UTOP));
	if(from_env_store)
	{
		if(!result)
		*from_env_store=thisenv->env_ipc_from;
		else
		*from_env_store=0;
	}
	if(perm_store)
	{
		if(!result)
		*perm_store=thisenv->env_ipc_perm;
		else
		*perm_store=0;
	}
	if(!result)
	result=thisenv->env_ipc_value;
	return result;
}

// Send 'val' (and 'pg' with 'perm', if 'pg' is nonnull) to 'toenv'.
// This function keeps trying until it succeeds.
// It should panic() on any error other than -E_IPC_NOT_RECV.
//
// Hint:
//   Use sys_yield() to be CPU-friendly.
//   If 'pg' is null, pass sys_ipc_try_send a value that it will understand
//   as meaning "no page".  (Zero is not the right value.)
void
ipc_send(envid_t to_env, uint32_t val, void *pg, int perm)
{
	// LAB 4: Your code here.
	int result;
	while(1)
	{
		if((uintptr_t)pg)
		{
			//cprintf("1:%d %d %p %d\n",to_env,val,pg,perm);
			result=sys_ipc_try_send(to_env,val,pg,perm);
		}
		else
		{
			//cprintf("2:%d %d %d %p %d\n",thisenv->env_id,to_env,val,pg,perm);
			result=sys_ipc_try_send(to_env,val,(void *)UTOP,perm);
		}
		if(result<0 && result!=-E_IPC_NOT_RECV)
		panic("sys_ipc_try_send: %e",result);
		else if(result>=0)
		break;
		sys_yield();
	}
}

// Find the first environment of the given type.  We'll use this to
// find special environments.
// Returns 0 if no such environment exists.
envid_t
ipc_find_env(enum EnvType type)
{
	int i;
	for (i = 0; i < NENV; i++)
		if (envs[i].env_type == type)
			return envs[i].env_id;
	return 0;
}