#include "pim_util.h"


static char addr_buf[40];

/* non re-entry safe function to convert IPv6 address to string */
char *  in6_addr2str(struct in6_addr * addr)
{
  return inet_ntop(AF_INET6, addr, addr_buf, sizeof(addr_buf)); 
}


