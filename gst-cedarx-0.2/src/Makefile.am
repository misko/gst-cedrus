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

#                        -L/home/bz/tmp/out/lib/full-package-name/ \
#                        -Wl,--no-as-needed  \
#                        -lcedar_vencoder -lcedar_common -lcedar_base -lcedar_vdecoder\
#                        -I../../libs/base/include/ \
#                        -I../../libs/vencoder/include/ \
#                        -I../../SOURCE/base/include \
#                        -I../../SOURCE/vencoder/include \
#                        -L/usr/local/lib/full-package-name/ \
#                        -o minima


# sources used to compile this plug-in
libgstcedarxh264enc_la_SOURCES = gstcedarxh264enc.c gstcedarxh264enc.h

# compiler and linker flags used to compile this cedarxh264enc, set in configure.ac
libgstcedarxh264enc_la_CFLAGS = $(GST_CFLAGS) -I./include
libgstcedarxh264enc_la_LIBADD = $(GST_LIBS) -L/usr/local/lib/full-package-name/  -lgstvideo-$(GST_API_VERSION)
libgstcedarxh264enc_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS) -lcedar_vencoder -lcedar_common -lcedar_base -lcedar_vdecoder  -I./include
libgstcedarxh264enc_la_LIBTOOLFLAGS = --tag=disable-static

# headers we need but don't want installed
noinst_HEADERS = gstcedarxh264enc.h
