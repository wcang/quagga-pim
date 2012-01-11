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

#include "memory.h"
#include "if.h"
#include "log.h"
#include "command.h"
#include "thread.h"
#include "prefix.h"
#include "plist.h"
#include "vty.h"

#include "pim.h"
#include "pim_util.h"
#include "pim6_msg.h"
#include "pim6_interface.h"
#include "pim6_neighbor.h"
#include "pim6_sock.h"

static struct cmd_node interface_node =
{
  INTERFACE_NODE,
  "%s(config-if)# ",
  1 /* VTYSH */
};

static int
config_write_pim6_interface (struct vty *vty)
{
  struct listnode * i;
  struct pim6_interface * pi;
  struct interface * ifp;

  for (ALL_LIST_ELEMENTS_RO (iflist, i, ifp))
  {
    pi = (struct pim6_interface *) ifp->info;

    if (pi == NULL)
      continue;

    vty_out(vty, "interface %s%s", ifp->name, VTY_NEWLINE);
    
    if (ifp->desc)
      vty_out(vty, " description %s%s", ifp->desc, VTY_NEWLINE);

    if (!pi->enabled) {
      vty_out(vty, " no ipv6 pim%s", VTY_NEWLINE);
    }
    else { 
      vty_out(vty, " ipv6 pim hello-interval %u%s", pi->hello_interval, VTY_NEWLINE);
      vty_out(vty, " ipv6 pim dr-priority %u%s", pi->dr_priority, VTY_NEWLINE);
    }

    /* TODO: support ipv6 pim join-prune-interval */
    vty_out (vty, "!%s", VTY_NEWLINE);
  }
  return 0;
}

static struct in6_addr *
pim6_interface_get_linklocal_address(struct interface *ifp)
{
  struct listnode * n;
  struct connected * c;

  /* for each connected address */
  for (ALL_LIST_ELEMENTS_RO (ifp->connected, n, c)) {
    /* if family not AF_INET6, ignore */
    if (c->address->family != AF_INET6)
      continue;

    /* linklocal scope check */
    if (IN6_IS_ADDR_LINKLOCAL (&c->address->u.prefix6)) {
      return &c->address->u.prefix6;
    }
  }

  return NULL;
}

void
pim6_interface_connected_update(struct interface *ifp)
{
  struct pim6_interface * pi;

  pi = (struct pim6_interface *) ifp->info;
  
  if (pi == NULL)
    return;

  pi->local_addr = pim6_interface_get_linklocal_address(ifp);

  if (pi->local_addr == NULL) {
    THREAD_OFF(pi->thread_hello_timer);
    pim6_leave_allpim6routers(ifp->ifindex);
    return;
  }

  if (pi->enabled) {
    THREAD_OFF(pi->thread_hello_timer);
    pim6_join_allpim6routers(ifp->ifindex);
    pi->thread_hello_timer = thread_execute(master, pim6_hello_send, ifp, 0);
  }
}


/* show specified interface structure */
static int
pim6_interface_show (struct vty *vty, struct interface *ifp)
{
  struct pim6_interface * pi;
  char addr_str[40];
  struct in6_addr * local_addr;
  
  /* FIXME: need to deal with the case when the interface is still offline */
  pi = (struct pim6_interface *) ifp->info;
  
  if (pi == NULL) {
    vty_out(vty, "%-20s%-4s%-6d%-7d%d%s", ifp->name, "off", 0, PIM_DEF_HELLO_INTERVAL, PIM_DEF_DR_PRIOR, VTY_NEWLINE);
    /* crap, this is wrong because the struct in6_addr is returned */
    local_addr = pim6_interface_get_linklocal_address(ifp);

    if (local_addr == NULL) {
      strcpy(addr_str, "::");
    }
    else {
      strcpy(addr_str, in6_addr2str(local_addr));
    }

    vty_out(vty, "Address: %s%s", addr_str, VTY_NEWLINE);
    vty_out(vty, "DR     : not elected%s", VTY_NEWLINE);
  }
  else {
    /* FIXME: Get the neighbour count right, for now let's put 0 for neighbour count */
    vty_out(vty, "%-20s%-4s%-6d%-7d%d%s", ifp->name, "on", 0, pi->hello_interval, pi->dr_priority, VTY_NEWLINE);
    vty_out(vty, "Address: %s%s", in6_addr2str(pi->local_addr), VTY_NEWLINE);
    /* FIXME: print "this system" if this interface is selected as DR */
    vty_out(vty, "DR     : not elected%s", VTY_NEWLINE);
  }

  return 0;
}

static inline void 
show_ipv6_pim_interface_header(struct vty * vty)
{
  vty_out(vty, "Interface          PIM  Nbr   Hello  DR%s", VTY_NEWLINE);
  vty_out(vty, "                        Count Intvl  Prior%s", VTY_NEWLINE);
}

/* show interface */
DEFUN (show_ipv6_pim_interface,
       show_ipv6_pim_interface_ifname_cmd,
       "show ipv6 pim interface IFNAME",
       SHOW_STR
       IP6_STR
       PIM_STR
       INTERFACE_STR
       IFNAME_STR
       )
{
  struct interface *ifp;
  struct listnode *i;

  zlog_debug("show ipv6 pim interface");

  if (argc) {
    ifp = if_lookup_by_name (argv[0]);
    if (ifp == NULL) {
      vty_out (vty, "No such Interface: %s%s", argv[0],
               VTY_NEWLINE);
      return CMD_WARNING;
    }

    show_ipv6_pim_interface_header(vty);
    pim6_interface_show (vty, ifp);
  }
  else {
    show_ipv6_pim_interface_header(vty);
    
    for (ALL_LIST_ELEMENTS_RO (iflist, i, ifp))
      pim6_interface_show (vty, ifp);
  }

  return CMD_SUCCESS;
}

ALIAS (show_ipv6_pim_interface,
       show_ipv6_pim_interface_cmd,
       "show ipv6 pim interface",
       SHOW_STR
       IP6_STR
       PIM_STR
       INTERFACE_STR
       )

static inline int pim6_neighbor_cmp(void * va, void * vb)
{
  struct pim6_neighbor * pna = (struct pim6_neighbor *) va;
  struct pim6_neighbor * pnb = (struct pim6_neighbor *) vb;
  return memcmp(&pna->addr, &pnb->addr, sizeof(pnb->addr));
}


/* Create new pim interface structure 
 * Not sure if this can be made generic for IPv4 and IPv6
 */
static struct pim6_interface *
pim6_interface_create(struct interface *ifp)
{
  struct pim6_interface * pi;

  zlog_info("Creating pim6_interface for %s ifindex %d", ifp->name, ifp->ifindex);
  pi = (struct pim6_interface *) XCALLOC (MTYPE_PIM6_IF, sizeof (struct pim6_interface));

  if (!pi) {
    zlog_err ("Can't malloc pim6_interface %s for ifindex %d", ifp->name, ifp->ifindex);
    return NULL;
  }

  pi->enabled = 1;  /* by default PIM is enabled */
  pi->hello_interval = PIM_DEF_HELLO_INTERVAL;
  pi->dr_priority = PIM_DEF_DR_PRIOR;
  pi->local_addr =  pim6_interface_get_linklocal_address(ifp);
  pi->neighbor_list = list_new();
  pi->neighbor_list->cmp = pim6_neighbor_cmp;
  ifp->info = pi;
  pi->interface = ifp;
  
  if (if_is_up(ifp)) {
    zlog_debug("Interface %s is up", ifp->name);
    pim6_join_allpim6routers(ifp->ifindex);
  }

  return pi;
}


DEFUN (ipv6_pim,
       ipv6_pim_cmd,
       "ipv6 pim",
       IP6_STR
       PIM_STR
       "<cr>\n"
       )
{
  struct pim6_interface * pi;
  struct interface * ifp;

  zlog_debug("ipv6 pim");

  ifp = (struct interface *) vty->index;
  assert (ifp);

  pi = (struct pim6_interface *) ifp->info;
  if (pi == NULL)
    pi = pim6_interface_create(ifp);
  assert(pi);
  pi->enabled = 1;
  pi->thread_hello_timer = thread_execute(master, pim6_hello_send, ifp, 0);
  return CMD_SUCCESS;
}


DEFUN (no_ipv6_pim,
       no_ipv6_pim_cmd,
       "no ipv6 pim",
       NO_STR
       IP6_STR
       PIM_STR
       "<cr>\n"
       )
{
  struct pim6_interface * pi;
  struct interface * ifp;

  zlog_debug("no ipv6 pim"); 
  ifp = (struct interface *) vty->index;
  assert (ifp);

  pi = (struct pim6_interface *) ifp->info;

  if (pi == NULL)
    pi = pim6_interface_create(ifp);
  
  assert(pi);
  pi->enabled = 0;
  /* Leave all IPv6 PIM multicast group on this interface */
  pim6_leave_allpim6routers(ifp->ifindex);
  /* Stop any pending PIM Hello sending */
  THREAD_OFF(pi->thread_hello_timer);
  /* Let's send a PIM Hello with 0 Holdtime to leave immediately */
  pi->hello_interval = 0;
  pi->thread_hello_timer = thread_execute(master, pim6_hello_send, ifp, 0);
  /* reset all values to default */
  pi->hello_interval = PIM_DEF_HELLO_INTERVAL;
  pi->dr_priority = PIM_DEF_DR_PRIOR; 
  return CMD_SUCCESS;
}


DEFUN (ipv6_pim_hellointerval,
       ipv6_pim_hellointerval_cmd,
       "ipv6 pim hello-interval <1-3600>",
       IP6_STR
       PIM_STR
       "PIM neighbor Hello announcement interval\n"
       "Hello interval in seconds\n"
       )
{
  struct pim6_interface * pi;
  struct interface * ifp;
  unsigned long int interval;

  zlog_debug("ipv6 pim hello-interval");
  ifp = (struct interface *) vty->index;
  assert (ifp);

  pi = (struct pim6_interface *) ifp->info;
  if (pi == NULL)
    pi = pim6_interface_create(ifp);
  assert (pi);

  interval = strtol (argv[0], NULL, 10);

  if (interval > 3600 || interval == 0) {
    vty_out (vty, "Interval %lu is out of range%s", interval, VTY_NEWLINE);
    return CMD_WARNING;
  }

  pi->enabled = 1;

  if (pi->hello_interval == interval)
    return CMD_SUCCESS;
  
  THREAD_OFF(pi->thread_hello_timer);
  pi->hello_interval = interval; 
  pi->thread_hello_timer =  thread_execute(master, pim6_hello_send, ifp, 0);
  return CMD_SUCCESS;
}

DEFUN (no_ipv6_pim_hellointerval,
       no_ipv6_pim_hellointerval_cmd,
       "no ipv6 pim hello-interval",
       NO_STR
       IP6_STR
       PIM_STR
       "PIM neighbor Hello announcement interval\n"
       )
{
  struct pim6_interface * pi;
  struct interface *ifp;

  zlog_debug("no ipv6 pim hello-interval");
  ifp = (struct interface *) vty->index;
  assert (ifp);

  pi = (struct pim6_interface *) ifp->info;

  /* we don't care if it wasn't configured previously or if the default was used */
  if (pi == NULL || pi->hello_interval == PIM_DEF_HELLO_INTERVAL)
    return CMD_SUCCESS;

  pi->hello_interval = PIM_DEF_HELLO_INTERVAL;
  pi->thread_hello_timer = thread_execute(master, pim6_hello_send, ifp, 0);
  return CMD_SUCCESS;
}


DEFUN (ipv6_pim_priority,
       ipv6_pim_priority_cmd,
       "ipv6 pim dr-priority <0-4294967295>",
       IP6_STR
       PIM_STR
       "PIM Hello DR priority\n"
       "Hello DR priority, preference given to larger value\n"
       )
{
  struct pim6_interface * pi;
  struct interface * ifp;
  unsigned long int priority;

  zlog_debug("ipv6 pim dr-priority");
  ifp = (struct interface *) vty->index;
  assert (ifp);

  pi = (struct pim6_interface *) ifp->info;
  if (pi == NULL)
    pi = pim6_interface_create(ifp);
  assert(pi);

  priority = strtol (argv[0], NULL, 10);

  if (priority > UINT32_MAX) {
    vty_out (vty, "Priority %lu is out of range%s", priority, VTY_NEWLINE);
    return CMD_WARNING;
  }
 
  pi->enabled = 1;

  if (pi->dr_priority == priority)
    return CMD_SUCCESS;
  
  pi->dr_priority = priority; 
  pi->thread_hello_timer = thread_execute(master, pim6_hello_send, ifp, 0);
  return CMD_SUCCESS;
}

DEFUN (no_ipv6_pim_priority,
       no_ipv6_pim_priority_cmd,
       "no ipv6 pim dr-priority",
       NO_STR
       IP6_STR
       PIM_STR
       "PIM Hello DR priority\n"
       )
{
  struct pim6_interface * pi;
  struct interface *ifp;

  zlog_debug("no ipv6 pim dr-priority");
  ifp = (struct interface *) vty->index;
  assert (ifp);

  pi = (struct pim6_interface *) ifp->info;

  /* we don't care if it wasn't configured previously or if the default was used */
  if (pi == NULL || pi->dr_priority == PIM_DEF_DR_PRIOR)
    return CMD_SUCCESS;

  pi->dr_priority = PIM_DEF_DR_PRIOR;
  pi->thread_hello_timer = thread_execute(master, pim6_hello_send, ifp, 0);

  return CMD_SUCCESS;
}


void
pim6_interface_init (void)
{
  /* Install interface node. */
  install_node (&interface_node, config_write_pim6_interface);

  install_element (VIEW_NODE, &show_ipv6_pim_interface_cmd);
  install_element (VIEW_NODE, &show_ipv6_pim_interface_ifname_cmd);
  install_element (ENABLE_NODE, &show_ipv6_pim_interface_cmd);
  install_element (ENABLE_NODE, &show_ipv6_pim_interface_ifname_cmd);

  install_element (CONFIG_NODE, &interface_cmd);
  install_default (INTERFACE_NODE);
  install_element (INTERFACE_NODE, &interface_desc_cmd);
  install_element (INTERFACE_NODE, &no_interface_desc_cmd);
  install_element (INTERFACE_NODE, &ipv6_pim_cmd);
  install_element (INTERFACE_NODE, &no_ipv6_pim_cmd);
  install_element (INTERFACE_NODE, &ipv6_pim_hellointerval_cmd);
  install_element (INTERFACE_NODE, &no_ipv6_pim_hellointerval_cmd);
  install_element (INTERFACE_NODE, &ipv6_pim_priority_cmd);
  install_element (INTERFACE_NODE, &no_ipv6_pim_priority_cmd);
}

struct pim6_interface * pim6_interface_lookup_by_ifindex (int ifindex)
{
  struct interface *ifp;

  ifp = if_lookup_by_index (ifindex);

  if (ifp == NULL)
    return (struct pim6_interface *) NULL;

  return (struct pim6_interface *) ifp->info;
}

