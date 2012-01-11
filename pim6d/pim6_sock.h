#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <zebra.h>

#define ALLPIM6ROUTERS "ff02::d"

extern struct in6_addr allpim6routers; 

extern int pim6_sock;  /* RAW socket that handles PIM traffic */ 

int pim6_serv_sock(void);

void pim6_join_allpim6routers(u_int ifindex);

void pim6_leave_allpim6routers(u_int ifindex);

int
pim6_recvmsg (struct in6_addr *src, struct in6_addr *dst,
               unsigned int *ifindex, unsigned char * buf, unsigned int len);

int
pim6_sendmsg(struct in6_addr *src, struct in6_addr *dst,
            unsigned int ifindex, unsigned char * buf, unsigned int len);

