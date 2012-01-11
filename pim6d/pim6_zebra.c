#include <zebra.h>

#include "log.h"
#include "vty.h"
#include "command.h"
#include "zclient.h"
#include "prefix.h"
#include "if.h"
#include "stream.h"
#include "thread.h"

#include "pim.h"
#include "pim6_interface.h"
#include "pim6_zebra.h"
#include "pim6_msg.h"
#include "pim6_sock.h"

/* information about zebra. */
struct zclient *zclient = NULL;


/* Inteface addition message from zebra. */
static int
pim6_zebra_if_add (int command, struct zclient *zclient, zebra_size_t length)
{
  struct interface * ifp;
  struct pim6_interface * pi;

  ifp = zebra_interface_add_read (zclient->ibuf);
  zlog_debug ("Zebra Interface add: %s index %d",
		ifp->name, ifp->ifindex);
  
  pi = (struct pim6_interface *) ifp->info;

  if (if_is_up(ifp) && pi && pi->enabled) {
    zlog_debug("Interface %s is up", ifp->name);
    thread_execute(master, pim6_hello_send, ifp, 0);
    pim6_join_allpim6routers(ifp->ifindex);
  }
 
  return 0;
}

static int
pim6_zebra_if_del (int command, struct zclient *zclient, zebra_size_t length)
{
  struct interface *ifp;

  if (!(ifp = zebra_interface_state_read(zclient->ibuf)))
    return 0;

  /* TODO: send pim6_hello_send with zero hold time */
  if (if_is_up(ifp))
    zlog_warn ("Zebra: got delete of %s, but interface is still up", ifp->name);

  zlog_debug ("Zebra Interface delete: %s index %d",
		ifp->name, ifp->ifindex);

#if 0
  /* Why is this commented out? */
  ospf6_interface_if_del (ifp);
#endif /*0*/

  ifp->ifindex = IFINDEX_INTERNAL;
  return 0;
}

  
static int
pim6_zebra_if_state_update (int command, struct zclient *zclient,
                             zebra_size_t length)
{
  struct interface *ifp;

  ifp = zebra_interface_state_read (zclient->ibuf);
  if (ifp == NULL)
    return 0;
  
  zlog_debug ("Zebra Interface state change: "
              "%s index %d flags %llx metric %d",
    ifp->name, ifp->ifindex, (unsigned long long)ifp->flags, 
    ifp->metric);
  /* TODO: do something */
  //ospf6_interface_state_update (ifp);
  return 0;
}

static int
pim6_zebra_if_address_update_add (int command, struct zclient *zclient,
                                   zebra_size_t length)
{
  struct connected *c;
  char buf[128];

  c = zebra_interface_address_read (ZEBRA_INTERFACE_ADDRESS_ADD, zclient->ibuf);
  if (c == NULL)
    return 0;

  zlog_debug ("Zebra Interface address add: %s %5s %s/%d",
  c->ifp->name, prefix_family_str (c->address),
  inet_ntop (c->address->family, &c->address->u.prefix,
       buf, sizeof (buf)), c->address->prefixlen);

  if (c->address->family == AF_INET6)
    pim6_interface_connected_update(c->ifp);

  return 0;
}


static int
pim6_zebra_if_address_update_delete (int command, struct zclient *zclient,
                               zebra_size_t length)
{
  struct connected *c;
  char buf[128];

  c = zebra_interface_address_read (ZEBRA_INTERFACE_ADDRESS_DELETE, zclient->ibuf);
  if (c == NULL)
    return 0;

  zlog_debug ("Zebra Interface address delete: %s %5s %s/%d",
  c->ifp->name, prefix_family_str (c->address),
  inet_ntop (c->address->family, &c->address->u.prefix,
       buf, sizeof (buf)), c->address->prefixlen);

  if (c->address->family == AF_INET6)
    pim6_interface_connected_update(c->ifp);

  return 0;
}


static int
pim6_zebra_read_ipv6 (int command, struct zclient *zclient,
                       zebra_size_t length)
{
  struct stream *s;
  struct zapi_ipv6 api;
  unsigned long ifindex;
  struct prefix_ipv6 p;
  struct in6_addr *nexthop;

  s = zclient->ibuf;
  ifindex = 0;
  nexthop = NULL;
  memset (&api, 0, sizeof (api));

  /* Type, flags, message. */
  api.type = stream_getc (s);
  api.flags = stream_getc (s);
  api.message = stream_getc (s);

  /* IPv6 prefix. */
  memset (&p, 0, sizeof (struct prefix_ipv6));
  p.family = AF_INET6;
  p.prefixlen = stream_getc (s);
  stream_get (&p.prefix, s, PSIZE (p.prefixlen));

  /* Nexthop, ifindex, distance, metric. */
  if (CHECK_FLAG (api.message, ZAPI_MESSAGE_NEXTHOP))
    {
      api.nexthop_num = stream_getc (s);
      nexthop = (struct in6_addr *)
        malloc (api.nexthop_num * sizeof (struct in6_addr));
      stream_get (nexthop, s, api.nexthop_num * sizeof (struct in6_addr));
    }
  if (CHECK_FLAG (api.message, ZAPI_MESSAGE_IFINDEX))
    {
      api.ifindex_num = stream_getc (s);
      ifindex = stream_getl (s);
    }
  if (CHECK_FLAG (api.message, ZAPI_MESSAGE_DISTANCE))
    api.distance = stream_getc (s);
  else
    api.distance = 0;
  if (CHECK_FLAG (api.message, ZAPI_MESSAGE_METRIC))
    api.metric = stream_getl (s);
  else
    api.metric = 0;

  //if (IS_OSPF6_DEBUG_ZEBRA (RECV))
    {
      char prefixstr[128], nexthopstr[128];
      prefix2str ((struct prefix *)&p, prefixstr, sizeof (prefixstr));
      if (nexthop)
        inet_ntop (AF_INET6, nexthop, nexthopstr, sizeof (nexthopstr));
      else
        snprintf (nexthopstr, sizeof (nexthopstr), "::");

      zlog_debug ("Zebra Receive route %s: %s %s nexthop %s ifindex %ld",
		  (command == ZEBRA_IPV6_ROUTE_ADD ? "add" : "delete"),
		  zebra_route_string(api.type), prefixstr, nexthopstr, ifindex);
    }
 
  /*if (command == ZEBRA_IPV6_ROUTE_ADD)
    ospf6_asbr_redistribute_add (api.type, ifindex, (struct prefix *) &p,
                                 api.nexthop_num, nexthop);
  else
    ospf6_asbr_redistribute_remove (api.type, ifindex, (struct prefix *) &p);
*/
  if (CHECK_FLAG (api.message, ZAPI_MESSAGE_NEXTHOP))
    free (nexthop);

  return 0;
}

/* Zebra configuration write function. */
static int
config_write_pim6_zebra (struct vty *vty)
{
  if (! zclient->enable)
    {
      vty_out (vty, "no router zebra%s", VTY_NEWLINE);
      vty_out (vty, "!%s", VTY_NEWLINE);
    }
  else if (! zclient->redist[ZEBRA_ROUTE_PIM6])
    {
      vty_out (vty, "router zebra%s", VTY_NEWLINE);
      vty_out (vty, " no redistribute pim6%s", VTY_NEWLINE);
      vty_out (vty, "!%s", VTY_NEWLINE);
    }
  return 0;
}
  
DEFUN (show_zebra,
       show_zebra_cmd,
       "show zebra",
       SHOW_STR
       "Zebra information\n")
{
  int i;
  if (zclient == NULL)
    {
      vty_out (vty, "Not connected to zebra%s", VTY_NEWLINE);
      return CMD_SUCCESS;
    }

  vty_out (vty, "Zebra Infomation%s", VTY_NEWLINE);
  vty_out (vty, "  enable: %d fail: %d%s",
           zclient->enable, zclient->fail, VTY_NEWLINE);
  vty_out (vty, "  redistribute default: %d%s", zclient->redist_default,
           VTY_NEWLINE);
  vty_out (vty, "  redistribute:");
  for (i = 0; i < ZEBRA_ROUTE_MAX; i++)
    {
      if (zclient->redist[i])
        vty_out (vty, " %s", zebra_route_string(i));
    }
  vty_out (vty, "%s", VTY_NEWLINE);
  return CMD_SUCCESS;
}


DEFUN (router_zebra,
       router_zebra_cmd,
       "router zebra",
       "Enable a routing process\n"
       "Make connection to zebra daemon\n")
{
  vty->node = ZEBRA_NODE;
  zclient->enable = 1;
  zclient_start (zclient);
  return CMD_SUCCESS;
}


DEFUN (no_router_zebra,
       no_router_zebra_cmd,
       "no router zebra",
       NO_STR
       "Configure routing process\n"
       "Disable connection to zebra daemon\n")
{
  zclient->enable = 0;
  zclient_stop (zclient);
  return CMD_SUCCESS;
}
  

DEFUN (redistribute_pim6,
       redistribute_pim6_cmd,
       "redistribute pim6",
       "Redistribute control\n"
       "PIM6 route\n")
{
  if (zclient->redist[ZEBRA_ROUTE_PIM6])
    return CMD_SUCCESS;

  zclient->redist[ZEBRA_ROUTE_PIM6] = 1;

  /* TODO: send pim6 route to zebra route table, need to look at Balaji's work */
  return CMD_SUCCESS;
}


DEFUN (no_redistribute_pim6,
       no_redistribute_pim6_cmd,
       "no redistribute pim6",
       NO_STR
       "Redistribute control\n"
       "PIM6 route\n")
{
  if (! zclient->redist[ZEBRA_ROUTE_PIM6])
    return CMD_SUCCESS;

  zclient->redist[ZEBRA_ROUTE_PIM6] = 0;

  /* TODO: send pim6 route to zebra route table, need to look at Balaji's work */
  return CMD_SUCCESS;
}

/* Zebra node structure. */
static struct cmd_node zebra_node =
{
  ZEBRA_NODE,
  "%s(config-zebra)# ",
};


void
pim6_zebra_init (void)
{
  /* Allocate zebra structure. */
  zclient = zclient_new ();
  zclient_init(zclient, ZEBRA_ROUTE_PIM6);
  zclient->router_id_update = NULL;
  zclient->interface_add = pim6_zebra_if_add;
  zclient->interface_delete = pim6_zebra_if_del;
  zclient->interface_up = pim6_zebra_if_state_update;
  zclient->interface_down = pim6_zebra_if_state_update;
  zclient->interface_address_add = pim6_zebra_if_address_update_add;
  zclient->interface_address_delete = pim6_zebra_if_address_update_delete;
  zclient->ipv4_route_add = NULL;
  zclient->ipv4_route_delete = NULL;
  zclient->ipv6_route_add = pim6_zebra_read_ipv6;
  zclient->ipv6_route_delete = pim6_zebra_read_ipv6;

  /* redistribute connected route by default */
  /* ospf6_zebra_redistribute (ZEBRA_ROUTE_CONNECT); */

  /* Install zebra node. */
  install_node (&zebra_node, config_write_pim6_zebra);

  /* Install command element for zebra node. */
  install_element (VIEW_NODE, &show_zebra_cmd);
  install_element (ENABLE_NODE, &show_zebra_cmd);
  install_element (CONFIG_NODE, &router_zebra_cmd);
  install_element (CONFIG_NODE, &no_router_zebra_cmd);

  install_default (ZEBRA_NODE);
  install_element (ZEBRA_NODE, &redistribute_pim6_cmd);
  install_element (ZEBRA_NODE, &no_redistribute_pim6_cmd);

  return;
}


