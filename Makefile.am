include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_find_channel

mod_LTLIBRARIES = mod_find_channel.la
mod_find_channel_la_SOURCES  = mod_find_channel.c
mod_find_channel_la_CFLAGS   = $(AM_CFLAGS)
mod_find_channel_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_find_channel_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
