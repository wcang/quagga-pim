/*
 * Copyright (C) 2011 Ang Way Chuang
 *
 * This file is part of GNU Zebra.
 *
 * GNU Zebra is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * GNU Zebra is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNU Zebra; see the file COPYING.  If not, write to the 
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330, 
 * Boston, MA 02111-1307, USA.  
 */

#include <zebra.h>

#include "privs.h"
#include "sockopt.h"
#include "log.h"

#include "pim.h"
#include "pim6_msg.h"
#include "pim6_sock.h"
#include "pim6d.h"
#include "pim_util.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>


struct in6_addr allpim6routers;

int pim6_sock;  /* RAW socket that handles PIM traffic */ 

/* Make ospf6d's server socket. */
int
pim6_serv_sock (void)
{
  if (pim6d_privs.change (ZPRIVS_RAISE))
    zlog_err ("%s: could not raise privs", __FUNCTION__);

  pim6_sock = socket (AF_INET6, SOCK_RAW, IPPROTO_PIM);
  if (pim6_sock < 0) {
    zlog_warn ("Network: can't create PIM6 socket.");
    if (pim6d_privs.change (ZPRIVS_LOWER))
      zlog_err ("%s: could not lower privs after socket failure", __FUNCTION__);
    return -1;
  }

  if (pim6d_privs.change (ZPRIVS_LOWER))
      zlog_err ("%s: could not lower privs", __FUNCTION__);

  /* set socket options */
  sockopt_reuseaddr (pim6_sock);
  setsockopt_ipv6_multicast_loop(pim6_sock, 0);
  setsockopt_ipv6_pktinfo(pim6_sock, 1);
  /* ask the kernel to perform checksum for us with IPv6 pseudo header.
   * the offset of the checksum at byte 2 in PIM header */
  setsockopt_ipv6_checksum(pim6_sock, 2);

  /* setup global in6_addr, allpim6routers for later use */
  inet_pton (AF_INET6, ALLPIM6ROUTERS, &allpim6routers);
  thread_add_read (master, pim6_receive, NULL, pim6_sock);
  return 0;
}

void
pim6_join_allpim6routers (u_int ifindex)
{
  struct ipv6_mreq mreq6;
  int retval;
  zlog_debug("joining ALL PIM6 router on interface index %u", ifindex);
  assert (ifindex);
  mreq6.ipv6mr_interface = ifindex;
  memcpy(&mreq6.ipv6mr_multiaddr, &allpim6routers, sizeof(struct in6_addr));

  retval = setsockopt (pim6_sock, IPPROTO_IPV6, IPV6_JOIN_GROUP,
                       &mreq6, sizeof (mreq6));

  if (retval < 0)
    zlog_err ("Network: Join AllPIM6Routers on ifindex %d failed: %s",
              ifindex, safe_strerror (errno));
}

void
pim6_leave_allpim6routers(u_int ifindex)
{
  struct ipv6_mreq mreq6;

  assert (ifindex);
  mreq6.ipv6mr_interface = ifindex;
  memcpy (&mreq6.ipv6mr_multiaddr, &allpim6routers,
          sizeof (struct in6_addr));

  if (setsockopt(pim6_sock, IPPROTO_IPV6, IPV6_LEAVE_GROUP,
                  &mreq6, sizeof (mreq6)) < 0)
    zlog_warn ("Network: Leave AllPIM6Routers on ifindex %d Failed: %s",
               ifindex, safe_strerror (errno));
}

int
pim6_recvmsg(struct in6_addr *src, struct in6_addr *dst,
               unsigned int *ifindex, unsigned char * buf, unsigned int len)
{
  int retval;
  struct msghdr rmsghdr;
  struct cmsghdr *rcmsgp;
  u_char cmsgbuf[CMSG_SPACE(sizeof (struct in6_pktinfo))];
  struct in6_pktinfo *pktinfo;
  struct sockaddr_in6 src_sin6;
  struct iovec iovector[2];
  
  /* initialize */
  iovector[0].iov_base = buf;
  iovector[0].iov_len = ntohs(len);
  iovector[1].iov_base = NULL;
  iovector[1].iov_len = 0;

  rcmsgp = (struct cmsghdr *)cmsgbuf;
  pktinfo = (struct in6_pktinfo *)(CMSG_DATA(rcmsgp));
  memset (&src_sin6, 0, sizeof (struct sockaddr_in6));

  /* receive control msg */
  rcmsgp->cmsg_level = IPPROTO_IPV6;
  rcmsgp->cmsg_type = IPV6_PKTINFO;
  rcmsgp->cmsg_len = CMSG_LEN (sizeof (struct in6_pktinfo));

  /* receive msg hdr */
  memset (&rmsghdr, 0, sizeof (rmsghdr));
  rmsghdr.msg_iov = iovector;
  rmsghdr.msg_iovlen = 2;
  rmsghdr.msg_name = (caddr_t) &src_sin6;
  rmsghdr.msg_namelen = sizeof (struct sockaddr_in6);
  rmsghdr.msg_control = (caddr_t) cmsgbuf;
  rmsghdr.msg_controllen = sizeof (cmsgbuf);

  retval = recvmsg(pim6_sock, &rmsghdr, 0);
  if (retval < 0)
    zlog_warn ("recvmsg failed: %s", safe_strerror (errno));
  else if (retval == len)
    zlog_warn ("recvmsg read full buffer size: %d", retval);

  /* source address */
  assert (src);
  memcpy (src, &src_sin6.sin6_addr, sizeof (struct in6_addr));

  /* destination address */
  if (ifindex)
    *ifindex = pktinfo->ipi6_ifindex;
  if (dst)
    memcpy (dst, &pktinfo->ipi6_addr, sizeof (struct in6_addr));

  return retval;
}


int
pim6_sendmsg(struct in6_addr *src, struct in6_addr *dst,
            unsigned int ifindex, unsigned char * buf, unsigned int len)
{
  int retval;
  struct msghdr smsghdr;
  struct cmsghdr *scmsgp;
  u_char cmsgbuf[CMSG_SPACE(sizeof (struct in6_pktinfo))];
  struct in6_pktinfo *pktinfo;
  struct sockaddr_in6 dst_sin6;
  struct iovec iovector[2];
  
  assert (dst);

  /* initialize */
  iovector[0].iov_base = buf;
  iovector[0].iov_len = len;
  iovector[1].iov_base = NULL;
  iovector[1].iov_len = 0;

  /* send message */
  scmsgp = (struct cmsghdr *)cmsgbuf;
  pktinfo = (struct in6_pktinfo *)(CMSG_DATA(scmsgp));
  memset (&dst_sin6, 0, sizeof (struct sockaddr_in6));

  /* source address */
  pktinfo->ipi6_ifindex = ifindex;

  if (src) {
    zlog_debug("Source address: %s", in6_addr2str(src));
    memcpy(&pktinfo->ipi6_addr, src, sizeof (struct in6_addr));
  }
  else {
    memset(&pktinfo->ipi6_addr, 0, sizeof (struct in6_addr));
  }

  /* destination address */
  dst_sin6.sin6_family = AF_INET6;
#ifdef SIN6_LEN
  dst_sin6.sin6_len = sizeof (struct sockaddr_in6);
#endif /*SIN6_LEN*/
  memcpy (&dst_sin6.sin6_addr, dst, sizeof (struct in6_addr));
#ifdef HAVE_SIN6_SCOPE_ID
  dst_sin6.sin6_scope_id = ifindex;
#endif

  /* send control msg */
  scmsgp->cmsg_level = IPPROTO_IPV6;
  scmsgp->cmsg_type = IPV6_PKTINFO;
  scmsgp->cmsg_len = CMSG_LEN (sizeof (struct in6_pktinfo));
  /* send msg hdr */
  memset (&smsghdr, 0, sizeof (smsghdr));
  smsghdr.msg_iov = iovector;
  smsghdr.msg_iovlen = 2;
  smsghdr.msg_name = (caddr_t) &dst_sin6;
  smsghdr.msg_namelen = sizeof (struct sockaddr_in6);
  smsghdr.msg_control = (caddr_t) cmsgbuf;
  smsghdr.msg_controllen = sizeof (cmsgbuf);

  retval = sendmsg(pim6_sock, &smsghdr, 0);
  if (retval != len)
    zlog_warn ("sendmsg failed: ifindex: %d: %s (%d)",
               ifindex, safe_strerror (errno), errno);

  return retval;
}
