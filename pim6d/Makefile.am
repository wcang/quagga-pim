## Process this file with automake to produce Makefile.in.

INCLUDES = @INCLUDES@ -I.. -I$(top_srcdir) -I$(top_srcdir)/lib 
DEFS = @DEFS@ -DSYSCONFDIR=\"$(sysconfdir)/\"
INSTALL_SDATA=@INSTALL@ -m 600

AM_CFLAGS = $(PICFLAGS)
AM_LDFLAGS = $(PILDFLAGS)

noinst_LIBRARIES = libpim.a
sbin_PROGRAMS = pim6d

libpim_a_SOURCES = \
	pim6d.c pim6_interface.c pim6_sock.c pim6_msg.c pim6_neighbor.c pim_util.c pim6_zebra.c

noinst_HEADERS = \
	pim.h pim6d.h pim6_interface.h pim6_sock.h pim6_msg.h pim6_neighbor.h pim_util.h pim6_zebra.h

pim6d_SOURCES = pim6_main.c $(libpim_a_SOURCES)
pim6d_LDADD = ../lib/libzebra.la @LIBCAP@ @LIBM@

