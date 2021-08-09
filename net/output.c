#include "ns.h"

extern union Nsipc nsipcbuf;

void
output(envid_t ns_envid)
{
	binaryname = "ns_output";

	// LAB 6: Your code here:
	// 	- read a packet from the network server
	//	- send the packet to the device driver
	struct jif_pkt *pkt;
	int size,r;
	void *buf;
	while(true)
	{
		r=ipc_recv(NULL,&nsipcbuf,NULL);
		if(r==NSREQ_OUTPUT)
		{
			pkt=(struct jif_pkt *)&nsipcbuf;
			size=pkt->jp_len;
			buf=pkt->jp_data;
			sys_e1000_try_tx(buf,size);
		}
	}
}
