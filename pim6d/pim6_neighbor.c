#include <netinet/in.h>

#include <zebra.h>

#include "linklist.h"
#include "memory.h"
#include "log.h"
#include "vty.h"
#include "command.h"

#include "pim_util.h"
#include "pim6_neighbor.h"
#include "pim6_interface.h"


struct pim6_neighbor *
pim6_neighbor_lookup(struct pim6_interface * pi, struct in6_addr * addr)
{
  struct listnode * n;
  struct pim6_neighbor * pn;

  for (ALL_LIST_ELEMENTS_RO(pi->neighbor_list, n, pn)) {
    if (IN6_ARE_ADDR_EQUAL(&pn->addr, addr))
      return pn;
  }

  return NULL;
}

/* create pim_neighbor */
struct pim6_neighbor *
pim6_neighbor_create(struct pim6_interface * pi, struct in6_addr * addr)
{
  struct pim6_neighbor * pn;

  pn = (struct pim6_neighbor *) XMALLOC(MTYPE_PIM6_NEIGHBOR, sizeof (struct pim6_neighbor));

  if (pn == NULL) {
    zlog_warn ("neighbor: malloc failed");
    return NULL;
  }

  memset(pn, 0, sizeof(struct pim6_neighbor));
  pn->pi = pi;
  memcpy(&pn->addr, addr, sizeof(*addr));
  quagga_gettime(QUAGGA_CLK_MONOTONIC, &pn->uptime);
  listnode_add_sort(pi->neighbor_list, pn);
  return pn;
}

void
pim6_neighbor_delete(struct pim6_neighbor * pn)
{
  listnode_delete(pn->pi->neighbor_list, pn);
  THREAD_OFF(pn->thread_expiry_timer);
  XFREE (MTYPE_PIM6_NEIGHBOR, pn);
}


static inline void 
show_ipv6_pim_neighbor_header(struct vty * vty)
{
  vty_out(vty, "PIM Neighbor Table%s", VTY_NEWLINE);
  vty_out(vty, "Mode: B - Bidir Capable, G - GenID Capable%s", VTY_NEWLINE);
  vty_out(vty, "Neighbor Address           Interface          Uptime    Expires  Mode DR pri%s", VTY_NEWLINE);
}


/* show specified interface's neighbor */
static void
pim6_neighbor_show (struct vty *vty, struct interface *ifp)
{
  struct pim6_interface * pi;
  struct listnode * n;
  struct pim6_neighbor * pn;
  struct timeval now, uptime, expiry;
  char uptime_buf[40], expiry_buf[40];

  /* FIXME: need to deal with the case when the interface is still offline */
  pi = (struct pim6_interface *) ifp->info;
 
  if (pi == NULL || !pi->enabled)
    return;

  quagga_gettime(QUAGGA_CLK_MONOTONIC, &now);

  for (ALL_LIST_ELEMENTS_RO(pi->neighbor_list, n, pn)) {
    uptime = time_sub(&now, &pn->uptime);
    expiry = time_sub(&pn->expiry, &now); 
    /* FIXME: DR and mode still hasn't been done properly */
    vty_out(vty, "%-27s%-19s%-10s%-9s%-5s%-3s%u%s", in6_addr2str(&pn->addr), ifp->name, 
        time2str(&uptime, uptime_buf, sizeof(uptime_buf)), time2str(&expiry, expiry_buf, sizeof(expiry_buf)), 
        "Mode", "DR", pn->dr_priority, VTY_NEWLINE);
  }

  return;
}

/* show pim neighbor */
DEFUN (show_ipv6_pim_neighbor,
       show_ipv6_pim_neighbor_ifname_cmd,
       "show ipv6 pim neighbor IFNAME",
       SHOW_STR
       IP6_STR
       PIM_STR
       "PIM neighbor information\n"
       IFNAME_STR
       )
{
  struct interface *ifp;
  struct listnode *i;

  zlog_debug("show ipv6 pim neighbor");

  if (argc) {
    ifp = if_lookup_by_name(argv[0]);
    if (ifp == NULL) {
      vty_out (vty, "No such Interface: %s%s", argv[0],
               VTY_NEWLINE);
      return CMD_WARNING;
    }

    show_ipv6_pim_neighbor_header(vty);
    pim6_neighbor_show(vty, ifp);
  }
  else {
    show_ipv6_pim_neighbor_header(vty);
    
    for (ALL_LIST_ELEMENTS_RO (iflist, i, ifp))
      pim6_neighbor_show (vty, ifp);
  }

  return CMD_SUCCESS;
}


ALIAS (show_ipv6_pim_neighbor,
       show_ipv6_pim_neighbor_cmd,
       "show ipv6 pim neighbor",
       SHOW_STR
       IP6_STR
       PIM_STR
       "PIM neighbor information\n"
       )



void
pim6_neighbor_cmd_init(void)
{
  install_element(VIEW_NODE, &show_ipv6_pim_neighbor_cmd);
  install_element(VIEW_NODE, &show_ipv6_pim_neighbor_ifname_cmd);
  install_element(ENABLE_NODE, &show_ipv6_pim_neighbor_cmd);
  install_element(ENABLE_NODE, &show_ipv6_pim_neighbor_ifname_cmd);
}

