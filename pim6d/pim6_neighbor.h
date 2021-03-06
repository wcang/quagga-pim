/*
 * Copyright (C) 2012 Ang Way Chuang
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

#ifndef PIM6_NEIGHBOR_H
#define PIM6_NEIGHBOR_H

#include <time.h>
#include <netinet/in.h>

#include <zebra.h>

#include "linklist.h"
#include "sockunion.h"

#define PIM_NEIGH_DR_FLAG  0x1
#define PIM_NEIGH_GENID_FLAG 0x2
#define PIM_NEIGH_BIDIR_FLAG 0x4

struct pim6_neighbor {
  /* used to indicate which option is set by neighbor */
  uint8_t flags;
  /* Neighbor holdtime */
  uint16_t holdtime;
  /* DR priority information */ 
  uint32_t dr_priority;
  /* generation id stored in network byte order */
  uint32_t gen_id;
  /* primary address (link local for IPv6) */
  struct in6_addr addr;
  /* relative when the neighbour is started */
  struct timeval uptime;
  /* relative time the neighbour is going to be expired */
  struct timeval expiry;
  /* secondary address list stored using sockunion */
  struct list sec_addr;
  /* monitor if the neighbor is inactive */
  struct thread * thread_expiry_timer;
  /* the pim interface where this neighbor corresponds to */
  struct pim6_interface * pi;
};

struct pim6_neighbor *
pim6_neighbor_lookup(struct pim6_interface * pi, struct in6_addr * addr);

struct pim6_neighbor *
pim6_neighbor_create (struct pim6_interface * pi, struct in6_addr * addr);

void
pim6_neighbor_delete (struct pim6_neighbor * pn);

void
pim6_neighbor_cmd_init(void);


#endif
