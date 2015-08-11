# Note: cedarxh264encdir is set in configure

##############################################################################
# TODO: change libgstcedarxh264enc.la to something else, e.g. libmysomething.la     #
##############################################################################
plugin_LTLIBRARIES = libgstcedarxh264enc.la

##############################################################################
# TODO: for the next set of variables, name the prefix if you named the .la, #
#  e.g. libmysomething.la => libmysomething_la_SOURCES                       #
#                            libmysomething_la_CFLAGS                        #
#                            libmysomething_la_LIBADD                        #
#                            libmysomething_la_LDFLAGS                       #
##############################################################################

# sources used to compile this plug-in
libgstcedarxh264enc_la_SOURCES = gstcedarxh264enc.c gstcedarxh264enc.h

# compiler and linker flags used to compile this cedarxh264enc, set in configure.ac
libgstcedarxh264enc_la_CFLAGS = $(GST_CFLAGS)
libgstcedarxh264enc_la_LIBADD = $(GST_LIBS)
libgstcedarxh264enc_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstcedarxh264enc_la_LIBTOOLFLAGS = --tag=disable-static

# headers we need but don't want installed
noinst_HEADERS = gstcedarxh264enc.h
