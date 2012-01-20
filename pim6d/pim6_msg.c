/* vim: set smartindent sw=2 tabstop=2 expandtab */
#include <stdlib.h>
#include <net/if.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <time.h>
#include <netinet/in.h>
#include <stdlib.h>

#include <zebra.h>

#include "sockunion.h"
#include "sockopt.h"
#include "stream.h"
#include "memory.h"
#include "prefix.h"
#include "thread.h"
#include "log.h"
#include "if.h"

#include "pim.h"
#include "pim6_msg.h"
#include "pim6_sock.h"
#include "pim6_interface.h"
#include "pim6_neighbor.h"
#include "pim_util.h"

#define iobuflen 1500

static uint8_t recvbuf[iobuflen];

static uint8_t sendbuf[iobuflen]; 

static uint32_t gen_id;

void 
init_gen_id(void)
{
  srand48(time(NULL));
  gen_id = lrand48();
}


/* check header, if everything is okay return 1 */
static inline int pim6_msg_sane_hdr(struct pim_header * ph, unsigned int len)
{
  /* For IPv6, checksum is performed by the kernel */
  return !(len <= sizeof(*ph) || ph->version != PIM_VERSION || ph->type > PIM_TYPE_MAX);
}

/* decode Encoded-Unicast Address */
/*static union sockunion * pim_msg_dec_ucast_addr(struct stream * s)
{
  uint8_t family, type;
  union sockunion * su = NULL;
  
  family = stream_getc(s);
  type = stream_getc(s);
*/
  /* check for native encoding */
/*  if (type != 0) {
    return NULL;
  }

  if (family == AF_IPV6) {
    su = XMALLOC(MTYPE_TMP, sizeof(*su));
    sockunion_family(su) = AF_INET6;
    stream_get(&su->sin6.sin6_addr, s, sizeof(su->sin6.sin6_addr)); 
  }
  else if (family == AF_IPV4) {
    su = XMALLOC(MTYPE_TMP, sizeof(*su));
    sockunion_family(su) = AF_INET;
    stream_get(&su->sin.sin_addr, s, sizeof(su->sin.sin_addr)); 
  }

  return su;
}*/

/* decode Encoded-Group Address */
/*static struct encoded_addr * pim_msg_dec_group_addr(struct stream * s)
{
  uint8_t family, type;
  struct encoded_addr * ea = NULL;
  
  family = stream_getc(s);
  type = stream_getc(s);
*/
  /* check for native encoding */
/*  if (type != 0) {
    return NULL;
  }

  if (family == AF_IPV6) {
    ea = XMALLOC(MTYPE_TMP, sizeof(*ea));
    ea->prefix.family = AF_INET6;
    ea->mask = stream_getc(s);
    ea->prefix.prefixlen = stream_getc(s);
    stream_get(&ea->prefix.u.prefix6, s, sizeof(ea->prefix.u.prefix6));
  */  /* be very strict, don't accept anything with prefix length less than 8 as that is the minimum for all group */
/*    
    if (ea->mask < 8 || ea->mask > 128 || !IN6_IS_ADDR_MULTICAST(&ea->prefix.u.prefix6)) {
      XFREE(MTYPE_TMP, ea);
      ea = NULL;
    }
  }
  else if (family == AF_IPV4) {
    ea = XMALLOC(MTYPE_TMP, sizeof(*ea));
    ea->prefix.family = AF_INET;
    ea->mask = stream_getc(s);
    ea->prefix.prefixlen = stream_getc(s);
    stream_get(&ea->prefix.u.prefix4, s, sizeof(ea->prefix.u.prefix4)); 
  */  /* be very strict, don't accept anything with prefix length less than 4 as that is the minimum for all group */
/*    if (ea->mask < 4 || ea->mask > 32 || !IN_MULTICAST(&ea->prefix.u.prefix4)) {
      XFREE(MTYPE_TMP, ea);
      ea = NULL;
    }
  }

  return ea;
}
*/

/* decode Encoded-Source Address */
/*static struct encoded_addr * pim_msg_dec_src_addr(struct stream * s)
{
  uint8_t family, type;
  struct encoded_addr * ea = NULL;
  
  family = stream_getc(s);
  type = stream_getc(s);
*/
  /* check for native encoding */
/*  if (type != 0) {
    return NULL;
  }

  if (family == AF_IPV6) {
    ea = XMALLOC(MTYPE_TMP, sizeof(*ea));
    ea->prefix.family = AF_INET6;
    ea->mask = stream_getc(s);
    ea->prefix.prefixlen = stream_getc(s);
    stream_get(&ea->prefix.u.prefix6, s, sizeof(ea->prefix.u.prefix6));

    if (ea->mask > 128) {
      goto fail_exit;
    }
  }
  else if (family == AF_IPV4) {
    ea = XMALLOC(MTYPE_TMP, sizeof(*ea));
    ea->prefix.family = AF_INET;
    ea->mask = stream_getc(s);
    ea->prefix.prefixlen = stream_getc(s);
    stream_get(&ea->prefix.u.prefix4, s, sizeof(ea->prefix.u.prefix4)); 

    if (ea->mask > 32) {
      goto fail_exit;
    }
  }
  else {
    return NULL;
  }

  if ((ea->mask & ENC_SRC_FLAG_WC) && !(ea->mask & ENC_SRC_FLAG_RPT))
    goto fail_exit;

  return ea;
fail_exit:
  XFREE(MTYPE_TMP, ea);
  return NULL;
}*/


static int
expire_neighbor(struct thread *thread)
{
  struct pim6_neighbor * pn;
  struct pim6_interface * pi;

  pn = (struct pim6_neighbor *) THREAD_ARG(thread);
  THREAD_OFF(pn->thread_expiry_timer);
  zlog_debug("PIM neighbor %s on interface %s expired", in6_addr2str(&pn->addr), pn->pi->interface->name);
  pi = pn->pi;
  pim6_neighbor_delete(pn);
  pim6_interface_reelect_dr(pi);
  return 0;
}


static void
pim6_hello_recv(struct in6_addr *src, struct in6_addr *dst,
                  struct pim6_interface * pi, unsigned char * msg,
                  unsigned int msg_len)
{
  uint8_t new_neigh = 0;
  uint8_t neigh_changed = 0;
  uint8_t need_elect = 0;
  struct pim_tlv * tlv; 
  uint16_t current_len;
  struct pim6_neighbor * pn;

  zlog_info("Processing PIM Hello from %s", in6_addr2str(src));   
  pn = pim6_neighbor_lookup(pi, src);
  
  if (pn == NULL) {
    pn = pim6_neighbor_create(pi, src);
    new_neigh = 1;

    if (pn == NULL) {
      zlog_err("Unable to create neighbor for %s on interface %s", in6_addr2str(src), pi->interface->name);
      return;
    } 
  }
  
  THREAD_OFF(pn->thread_expiry_timer);
  tlv = (struct pim_tlv *) msg;

  while (msg_len > sizeof(*tlv)) {
    tlv->type = ntohs(tlv->type);
    tlv->length = ntohs(tlv->length);
    current_len = tlv->length + 4;

    if (msg_len < current_len) {
      zlog_err("PIM Hello TLV truncated. Expected length is %u but only %u is available\n", 
          current_len, msg_len);
      return;
    }

    switch (tlv->type) {
      case HELLO_TYPE_HOLDTIME:
        if (tlv->length != 2) {
          zlog_err("Invalid length %u PIM Hello HoldTime option\n", tlv->length);
          goto fail_exit;
        }
       
        pn->holdtime = ntohs(*(uint16_t *) tlv->value); 
        quagga_gettime(QUAGGA_CLK_MONOTONIC, &pn->expiry);
        time_inc(&pn->expiry, pn->holdtime);
        break;
      case HELLO_TYPE_LAN_PRUNE_DELAY:
        /* TODO */
        break;
      case HELLO_TYPE_BIDIR:
        if (tlv->length != 0) {
          zlog_err("Invalid length %u PIM Bidirectional option", tlv->length);
          goto fail_exit;
        }
        pn->flags |= PIM_NEIGH_BIDIR_FLAG;
        break;
      case HELLO_TYPE_DR_PRIORITY:
        if (tlv->length != 4) {
          zlog_err("Invalid length %u PIM Hello DR Priority option\n", tlv->length);
          goto fail_exit;
        }

        pn->flags |= PIM_NEIGH_DR_FLAG;
        pn->dr_priority = ntohl(*(uint32_t *) tlv->value); 
        break;
      case HELLO_TYPE_GENID:
        if (tlv->length != 4) {
          zlog_err("Invalid length %u PIM Hello Generation ID option\n", tlv->length);
          goto fail_exit;
        }
        
        if (!(pn->flags & PIM_NEIGH_GENID_FLAG) || (!new_neigh && pn->gen_id != *((uint32_t *) tlv->value)))
          neigh_changed = 1;

        /* TODO: need to propogate the change if the generation id is changed */
        pn->gen_id = *((uint32_t *) tlv->value);
        pn->flags |= PIM_NEIGH_GENID_FLAG;
        break;
      case HELLO_TYPE_ADDR_LIST:
        /* TODO */
        break;
      default:
        zlog_warn("Unrecognized option type %u for PIM Hello TLV\n", tlv->type);
        break;
    }

    msg_len -= current_len;
    tlv = (struct pim_tlv *) ((caddr_t) tlv + current_len);
  }

  /* this neighbor doesn't send DR priority */
  if (!(pn->flags & PIM_NEIGH_DR_FLAG)) {
    if (!pi->dr_absent)
      need_elect = 1;
    pi->dr_absent = 1;
  }

  /*TODO: do something if neighbor is changed */
  if (new_neigh || neigh_changed || need_elect) {
    /* re-elect the DR */
    pim6_interface_reelect_dr(pi);
  }
 
  /* TODO: if new neighbor is detected, send hello immediately so that PIM neighbor can recognize
   * us immediately
   */

  /* reschedule expiry timer if the expiry time wasn't infinity */
  if (pn->holdtime != 0xffff) {
    pn->thread_expiry_timer = thread_add_timer(master, expire_neighbor, pn, pn->holdtime);
  }

  return;
fail_exit:
  if (new_neigh) {
    pim6_neighbor_delete(pn);
  }
}


/* receive join/prune message and process them */
static void
pim6_jp_recv(struct in6_addr *src, struct in6_addr *dst,
                  struct pim6_interface * pi, unsigned char * msg,
                  unsigned int msg_len)
{
  uint8_t num_group;
  uint16_t holdtime;
  uint16_t current_len;
  uint16_t num_join;
  uint16_t num_prune;
  struct pim6_neighbor * pn;
  struct pim6_enc_uni_addr * upstream_neigh;
  struct pim6_enc_grp_addr * grp_addr;
  struct pim6_enc_src_addr * src_addr;

  zlog_info("Processing PIM Join Prune from %s", in6_addr2str(src));   
  pn = pim6_neighbor_lookup(pi, src);
  
  if (pn == NULL) {
    zlog_err("%s is not in our neighbor list yet", in6_addr2str(src));
    return;
  }

  if (msg_len < sizeof(struct pim6_enc_uni_addr) + 4) {
    zlog_err("PIM Join/Prune message is less than the minimum required length. Discard message");
    return;
  }

  upstream_neigh = (struct pim6_enc_uni_addr *) msg;
  zlog_debug("family %u type %u upstream neighbor: %s", upstream_neigh->family, upstream_neigh->type,
      in6_addr2str(&upstream_neigh->address));
  zlog_debug("my address: %s", in6_addr2str(pi->local_addr));
  

  if (!IN6_ARE_ADDR_EQUAL(&upstream_neigh->address, pi->local_addr)) {
    /* TODO: For fast convergence, we need to monitor Join/Prune even if we aren't the target.
     * For simplicity, we should disregard it for now
     */
    return;
  }
    
  msg += sizeof(*upstream_neigh) + 1; /* skip the reserved byte */
  num_group = *msg;
  msg++;
  holdtime = ntohs(*((uint16_t *) msg));
  msg += 2;
  zlog_debug("num group %u holdtime %u", num_group, holdtime);
  msg_len -= sizeof(struct pim6_enc_uni_addr) + 4;
  
  /* TODO: sanity check to ensure that there is no duplication of group/source address */
  while (num_group) {
    if (msg_len < sizeof(struct pim6_enc_grp_addr) + 4) {
      zlog_err("PIM Join/Prune message group record truncated. Stop processing");
      return;
    }

    grp_addr = (struct pim6_enc_grp_addr *) msg;
    /* TODO: do something about group */
    if (grp_addr->family != AF_IPV6) {
      zlog_err("IPv6 address is expected for group address, but non IPv6 address is received. Discard remaining message");
      return;
    }

    if (grp_addr->type != 0) {
      zlog_err("Unable to decode non-default IPv6 group address. Discard remaining message");
      return;
    }

    zlog_debug("Group address: %s/%u Bidirectional: %u Zone: %u", in6_addr2str(&grp_addr->address), grp_addr->mask_len,
       grp_addr->bidirectional, grp_addr->zone); 

    msg += sizeof(struct pim6_enc_grp_addr);
    num_join = ntohs(*(uint16_t *) msg);
    msg += 2;
    num_prune = ntohs(*(uint16_t *) msg);
    msg += 2;
    msg_len -= sizeof(struct pim6_enc_grp_addr) + 4;

    if (msg_len < (num_join + num_prune) * sizeof(struct pim6_enc_src_addr)) {
      zlog_err("PIM Join/Prune message source list truncated. Stop processing");
      return;
    }

    msg_len -= (num_join + num_prune) * sizeof(struct pim6_enc_src_addr);

    while (num_join) {
      src_addr = (struct pim6_enc_src_addr *) msg;

      if (src_addr->type != 0) {
        zlog_err("Unable to decode non-default join source address type. Discard remaining message");
        return;
      }

      /* TODO: print or do something */
      if (src_addr->family == AF_IPV6) {
        zlog_debug("Joined source: %s sparse: %u wildcard: %u rpt: %u", in6_addr2str(&src_addr->address),
            src_addr->sparse, src_addr->wildcard, src_addr->rpt);
      }
      else {
        zlog_err("IPv6 address is expected for joined source address, but non IPv6 address is received. Discard remaining message");
        return;
      }

      msg += sizeof(struct pim6_enc_src_addr);
      num_join--;
    }

    while (num_prune) {
      src_addr = (struct pim6_enc_src_addr *) msg;
      /* TODO: print or do something */
      if (src_addr->type != 0) {
        zlog_err("Unable to decode non-default pruned source address type. Discard remaining message");
        return;
      }

      /* TODO: print or do something */
      if (src_addr->family == AF_IPV6) {
        zlog_debug("Pruned source: %s sparse: %u wildcard: %u rpt: %u", in6_addr2str(&src_addr->address),
            src_addr->sparse, src_addr->wildcard, src_addr->rpt);
      }
      else {
        zlog_err("IPv6 address is expected for pruned source address, but non IPv6 address is received. Discard remaining message");
        return;
      }

      msg += sizeof(struct pim6_enc_src_addr);
      num_prune--;
    }

    num_group--;
  }
}


int
pim6_receive(struct thread *thread)
{
  unsigned int len;
  struct in6_addr src, dst;
  unsigned int ifindex;
  struct pim6_interface * pi;
  struct pim_header * ph;

  zlog_debug("pim6_receive");
  /* reschedule read thread */
  thread_add_read(master, pim6_receive, NULL, pim6_sock);

  /* initialize */
  memset (&src, 0, sizeof (src));
  memset (&dst, 0, sizeof (dst));
  ifindex = 0;
  memset (recvbuf, 0, iobuflen);
  
  /* receive message */
  len = pim6_recvmsg (&src, &dst, &ifindex, recvbuf, iobuflen);
  if (len > iobuflen) {
    zlog_err ("Excess message read");
    return 0;
  }

  pi = pim6_interface_lookup_by_ifindex (ifindex);
  
  if (pi == NULL) {
    zlog_debug ("Message received on disabled interface");
    return 0;
  }

  ph = (struct pim_header *) recvbuf;
  zlog_debug("Received length %d", len);
  zlog_debug("pim version %u type %u reserved %u cksum %u", ph->version, ph->type, ph->reserved, ph->checksum); 
  
  if (!pim6_msg_sane_hdr(ph, len))
    return 0;

  len -= sizeof(*ph);

  switch (ph->type) {
  case PIM_TYPE_HELLO:
    pim6_hello_recv(&src, &dst, pi, (unsigned char *) (ph + 1), len);
    break;
  case PIM_TYPE_REGISTER:
    zlog_warn("PIM Register not implemented yet\n");
    break;
  case PIM_TYPE_REGISTER_STOP:
    zlog_warn("PIM Register Stop not implemented yet\n");
    break;
  case PIM_TYPE_JOIN_PRUNE:
    pim6_jp_recv(&src, &dst, pi, (unsigned char *) (ph + 1), len);
    break;
  case PIM_TYPE_ASSERT:
    zlog_warn("PIM Assert not implemented yet\n");
    break;
  case PIM_TYPE_GRAFT:
    zlog_warn("PIM Graft not implemented yet\n");
    break;
  case PIM_TYPE_GRAFT_ACK:
    zlog_warn("PIM Graft Acknowledgement not implemented yet\n");
    break;
  case PIM_TYPE_CAND_RP_ADV:
    zlog_warn("PIM Candidate RP Advertisement not implemented yet\n");
    break;
  }
  return 0;
}


int
pim6_hello_send(struct thread *thread)
{
  uint16_t holdtime;
  struct interface * ifp;
  struct pim6_interface * pi;
  struct pim_header * ph;
  struct pim_tlv * tlv;
  unsigned int offset;

  ifp = (struct interface *) THREAD_ARG (thread);
  pi = (struct pim6_interface *) ifp->info;
  THREAD_OFF(pi->thread_hello_timer);
  pi->thread_hello_timer = (struct thread *) NULL;
  zlog_debug("pim6_hello_send on interface %s", ifp->name);
  
  if ((!pi->enabled && pi->hello_interval != 0) || !pi->local_addr) {
    zlog_warn("Possible error. PIM is not enabled or local address is not set on interface %s", ifp->name);
    return 0;
  }

  if (!if_is_up(ifp)) {
    zlog_debug("Interface %s is not up. Not sending PIM Hello", ifp->name);
    return 0;
  }

  /* reschedule for next round of PIM hello */
  holdtime = 3.5 * pi->hello_interval;
  /* if hello interval is 0, then we are leaving. Therefore, no rescheduling 
   * of PIM Hello when we are leaving
   */
  if (pi->hello_interval)
    pi->thread_hello_timer = thread_add_timer(master, pim6_hello_send, 
        ifp, pi->hello_interval);

  memset(sendbuf, 0, iobuflen);
  ph = (struct pim_header *) sendbuf;
  ph->version = PIM_VERSION;
  ph->type = PIM_TYPE_HELLO;
  offset = sizeof(*ph);
  tlv = (struct pim_tlv *) (sendbuf + offset);
  /* holdtime option */
  tlv->type = htons(HELLO_TYPE_HOLDTIME);
  tlv->length = htons(2);
  *((uint16_t *) tlv->value) = htons(holdtime);
  offset += 6;
  tlv = (struct pim_tlv *) (sendbuf + offset);
  /* DR priority option */
  tlv->type = htons(HELLO_TYPE_DR_PRIORITY);
  tlv->length = htons(4);
  *((uint32_t *) tlv->value) = htonl(pi->dr_priority);
  offset += 8;
  tlv = (struct pim_tlv *) (sendbuf + offset);
  /* FIXME: Need to add LAN Prune Delay */
  /* Generation ID */
  tlv->type = htons(HELLO_TYPE_GENID);
  tlv->length = htons(4);
  *((uint32_t *) tlv->value) = htonl(gen_id);
  offset += 8;
  /* FIXME: Add support for secondary addresses */

  pim6_sendmsg(pi->local_addr, &allpim6routers, ifp->ifindex, sendbuf, offset);
  return 0;
}
