#include "ns.h"

extern union Nsipc nsipcbuf;

void
input(envid_t ns_envid)
{
	binaryname = "ns_input";

	// LAB 6: Your code here:
	// 	- read a packet from the device driver
	//	- send it to the network server
	// Hint: When you IPC a page to the network server, it will be
	// reading from it for a while, so don't immediately receive
	// another packet in to the same physical page.
	char buf[1024];
	int r,size;
	static int queue_i=0;
	struct jif_pkt *pkt;
	while(true)
	{
		while(true)
		{
			r=sys_e1000_rx(buf,&size);
			if(!r)
			break;
		}
		pkt=(struct jif_pkt *)(REQVA+queue_i*PGSIZE);
		queue_i=(queue_i+1)%QUEUE_SIZE;
		r=sys_page_alloc(0,pkt,PTE_U | PTE_P | PTE_W);
		if(r<0)
		panic("sys_page_alloc: %e",r);
		memmove(pkt->jp_data,buf,size);
		pkt->jp_len=size;
		ipc_send(ns_envid,NSREQ_INPUT,pkt,PTE_U | PTE_P | PTE_W);
	}
}
