#include <netinet/in.h>

#include <zebra.h>

#include "linklist.h"
#include "memory.h"
#include "log.h"

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
pim6_neighbor_create (struct pim6_interface * pi, struct in6_addr * addr)
{
  struct pim6_neighbor * pn;

  pn = (struct pim6_neighbor *) XMALLOC (MTYPE_PIM6_NEIGHBOR, sizeof (struct pim6_neighbor));

  if (pn == NULL) {
    zlog_warn ("neighbor: malloc failed");
    return NULL;
  }

  memset(pn, 0, sizeof(struct pim6_neighbor));
  pn->pi = pi;
  quagga_gettime(QUAGGA_CLK_MONOTONIC, &pn->uptime);
  listnode_add_sort(pi->neighbor_list, pn);
  return pn;
}

void
pim6_neighbor_delete(struct pim6_neighbor * pn)
{
  listnode_delete(pn->pi->neighbor_list, pn);
  THREAD_OFF (pn->thread_expiry_timer);
  XFREE (MTYPE_PIM6_NEIGHBOR, pn);
}
