# Note: myfilterdir is set in configure

##############################################################################
# TODO: change libgstmyfilter.la to something else, e.g. libmysomething.la     #
##############################################################################
plugin_LTLIBRARIES = libgstcedarh264enc.la #libgstmyfilter.la

##############################################################################
# TODO: for the next set of variables, name the prefix if you named the .la, #
#  e.g. libmysomething.la => libmysomething_la_SOURCES                       #
#                            libmysomething_la_CFLAGS                        #
#                            libmysomething_la_LIBADD                        #
#                            libmysomething_la_LDFLAGS                       #
##############################################################################

# sources used to compile this plug-in
libgstcedarh264enc_la_SOURCES = gstcedarh264enc.c gstcedarh264enc.h ve.c ve.h h264enc.c h264enc.h #gstmyfilter.c gstmyfilter.h

# compiler and linker flags used to compile this myfilter, set in configure.ac
libgstcedarh264enc_la_CFLAGS = $(GST_CFLAGS)
libgstcedarh264enc_la_LIBADD = $(GST_LIBS) -lgstvideo-$(GST_API_VERSION)
libgstcedarh264enc_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstcedarh264enc_la_LIBTOOLFLAGS = --tag=disable-static

# headers we need but don't want installed
noinst_HEADERS = gstcedarh264enc.h #gstmyfilter.h
