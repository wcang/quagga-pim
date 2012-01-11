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

#include "thread.h"
#include "linklist.h"
#include "vty.h"
#include "command.h"

#include "pim6d.h"
#include "pim6_interface.h"
#include "pim6_sock.h"
#include "pim6_msg.h"
#include "pim6_zebra.h"

extern struct zebra_privs_t pim6d_privs;


/* Install pim6 related commands. */
void
pim6_init (void)
{
  if (pim6_serv_sock() != 0) {
    zlog_err("PIM6 socket initialization failed\n");
    exit(1);
  }
  /* initialize generation id */
  init_gen_id();
  /* initialize interface command, but where is the interface initialization and the relevant thread for hello?  must be those vty read stuff */
  pim6_interface_init();
  pim6_zebra_init();
}
