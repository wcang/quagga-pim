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
#include <stdint.h>
#include <time.h>

#include "linklist.h"
#include "sockunion.h"
#include "thread.h"
#include "if.h"

#define PIM_DEF_HELLO_INTERVAL  30  /* 30 seconds for default hello interval */
#define PIM_DEF_DR_PRIOR        1   /* Default DR priority for DR is 1 */
#define PIM_DEF_JP_INTERVAL     60  /* Default Join/Prune interval is 60 seconds, not sure how this works yet. 
                                       Maybe related to LAN Prune Delay */

struct pim6_interface {
  /* PIM enabled */
  uint8_t enabled;
  /* PIM Hello interval in seconds */
  uint16_t hello_interval;
  /* DR priority of this interface */
  uint32_t dr_priority;
  /* add some linked-list of neighbours here */
  struct list * neighbor_list;
  /* local address of this I/F used for this interface (link local for IPv6) */
  struct in6_addr * local_addr;
  /* thread to send PIM hello message */
  struct thread * thread_hello_timer;
  /* interface information */
  struct interface * interface;
};


void pim6_interface_cmd_init (void);

struct pim6_interface * pim6_interface_lookup_by_ifindex (int ifindex);

void pim6_interface_connected_update(struct interface *ifp);

