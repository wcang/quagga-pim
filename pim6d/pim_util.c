#include "pim_util.h"

#include <stdio.h>

static char addr_buf[40];

/* non re-entry safe function to convert IPv6 address to string */
char *  in6_addr2str(struct in6_addr * addr)
{
  return inet_ntop(AF_INET6, addr, addr_buf, sizeof(addr_buf)); 
}


char * time2str(struct timeval * tv, char * time_buf, int len)
{
  if (tv->tv_sec / 60 / 60 / 24) 
      snprintf(time_buf, len, "%ldd%02ld:%02ld:%02ld", tv->tv_sec / 60 / 60 / 24,
         tv->tv_sec / 60 / 60 % 24, tv->tv_sec / 60 % 60, tv->tv_sec % 60);
  else
      snprintf(time_buf, len, "%02ld:%02ld:%02ld", tv->tv_sec / 60 / 60 , tv->tv_sec / 60 % 60, tv->tv_sec % 60);

  return time_buf;
}


struct timeval time_sub(struct timeval * a, struct timeval * b)
{
  struct timeval diff;

  diff.tv_sec = a->tv_sec - b->tv_sec;

  if (b->tv_usec > a->tv_usec) {
    diff.tv_sec--;
    a->tv_usec += 1000000; 
  }

  diff.tv_usec = a->tv_usec - b->tv_usec;
  return diff;
}

