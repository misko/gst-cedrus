/*
 * Cedar H264 Encoder Plugin
 * Copyright (C) 2014 Enrico Butera <ebutera@users.sourceforge.net>
 * 
 * Byte stream utils:
 * Copyright (c) 2014 Jens Kuske <jenskuske@gmail.com>
 * 
 * Gst template code:
 * Copyright (C) 2005 Thomas Vander Stichele <thomas@apestaart.org>
 * Copyright (C) 2005 Ronald S. Bultje <rbultje@ronald.bitfreak.net>
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/**
 * SECTION:element-cedar_h264enc
 *
 * H264 Encoder plugin using CedarX hardware engine
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -ve videotestsrc ! cedar_h264enc ! h264parse ! matroskamux ! filesink location="cedar.mkv"
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <string.h>
#include <unistd.h>
#include <gst/gst.h>

#include "gstcedarh264enc.h"
#include "h264enc.h"
#include "ve.h"

GST_DEBUG_CATEGORY_STATIC (gst_cedarh264enc_debug);
#define GST_CAT_DEFAULT gst_cedarh264enc_debug

#define CEDAR_OUTPUT_BUF_SIZE	(1* 1024 * 1024)


struct h264enc_params params;
h264enc * encoder = NULL;

/* Filter signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_SILENT,
  PROP_KEYFRAME,
  PROP_PROFILE_IDC,
  PROP_LEVEL_IDC,
  PROP_QP
};

/* the capabilities of the inputs and outputs.
 *
 * describe the real formats here.
 */
static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (
		"video/x-raw-yuv, "
			"format = (fourcc) NV12, "
			"width = (int) [16,1920], "
			"height = (int) [16,1080]"
			/*"framerate=(fraction)[1/1,25/1]"*/
    )
    );

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (
		"video/x-h264, "
			"stream-format = (string) byte-stream, "
			"alignment = (string) nal, "
			"profile = (string) { main }"
	)
    );

GST_BOILERPLATE (Gstcedarh264enc, gst_cedarh264enc, GstElement,
    GST_TYPE_ELEMENT);

static void gst_cedarh264enc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_cedarh264enc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_cedarh264enc_set_caps (GstPad * pad, GstCaps * caps);
static GstFlowReturn gst_cedarh264enc_chain (GstPad * pad, GstBuffer * buf);

static GstStateChangeReturn
	gst_cedarh264enc_change_state (GstElement *element, GstStateChange transition);


static gboolean alloc_cedar_bufs(Gstcedarh264enc *cedarelement)
{
	params.src_width = (cedarelement->width + 15) & ~15;
	params.width = cedarelement->width;
	params.src_height = (cedarelement->height + 15) & ~15;
	params.height = cedarelement->height;
	params.src_format = H264_FMT_NV12;
	params.profile_idc = cedarelement->profile_idc;
	params.level_idc = cedarelement->level_idc;
	params.entropy_coding_mode = H264_EC_CABAC;
	params.qp = cedarelement->qp;
	fprintf(stderr,"CEDAR has %d keyframe, profile %d, level %d, qp %d\n",cedarelement->keyframe,cedarelement->profile_idc, cedarelement->level_idc, cedarelement->qp);
	params.keyframe_interval = cedarelement->keyframe;
	
        encoder = h264enc_new(&params);
	
 	cedarelement->output_buf = h264enc_get_bytestream_buffer(encoder);
        cedarelement->input_buf = h264enc_get_input_buffer(encoder);
	
	return TRUE;
}

/* GObject vmethod implementations */

static void
gst_cedarh264enc_base_init (gpointer gclass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (gclass);

  gst_element_class_set_details_simple(element_class,
    "cedar_h264enc",
    "CedarX H264 Encoder",
    "H264 Encoder Plugin for CedarX hardware",
    "Enrico Butera <ebutera@users.berlios.de> - hacked by Misko for P-frames");

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&src_factory));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&sink_factory));
}

/* initialize the cedar_h264enc's class */
static void
gst_cedarh264enc_class_init (Gstcedarh264encClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  
  gobject_class->set_property = gst_cedarh264enc_set_property;
  gobject_class->get_property = gst_cedarh264enc_get_property;
  
  gstelement_class->change_state = gst_cedarh264enc_change_state;

  g_object_class_install_property (gobject_class, PROP_SILENT,
      g_param_spec_boolean ("silent", "Silent", "Produce verbose output ?",
          FALSE, G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, PROP_KEYFRAME,
      g_param_spec_int ("keyframe", "keyframe", "keyframe rate",1,60,
          25, G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, PROP_PROFILE_IDC,
      g_param_spec_int ("profile_idc", "profile_idc", "profile_idc",1,254,
          77, G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, PROP_LEVEL_IDC,
      g_param_spec_int ("level_idc", "level_idc", "level_idc",1,254,
          41, G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, PROP_QP,
      g_param_spec_int ("qp", "qp", "qp",1,254,
          30, G_PARAM_READWRITE));
}

/* initialize the new element
 * instantiate pads and add them to element
 * set pad calback functions
 * initialize instance structure
 */
static void
gst_cedarh264enc_init (Gstcedarh264enc * filter,
    Gstcedarh264encClass * gclass)
{
  filter->sinkpad = gst_pad_new_from_static_template (&sink_factory, "sink");
  gst_pad_set_setcaps_function (filter->sinkpad,
                                GST_DEBUG_FUNCPTR(gst_cedarh264enc_set_caps));
  gst_pad_set_chain_function (filter->sinkpad,
                              GST_DEBUG_FUNCPTR(gst_cedarh264enc_chain));

  filter->srcpad = gst_pad_new_from_static_template (&src_factory, "src");
  gst_pad_use_fixed_caps(filter->srcpad);

  gst_element_add_pad (GST_ELEMENT (filter), filter->sinkpad);
  gst_element_add_pad (GST_ELEMENT (filter), filter->srcpad);
  filter->silent = FALSE;
  filter->keyframe = 25;
  filter->profile_idc = 71;
  filter->level_idc = 41;
  filter->qp = 30;
}

static void
gst_cedarh264enc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  Gstcedarh264enc *filter = GST_CEDAR_H264ENC (object);

  switch (prop_id) {
    case PROP_SILENT:
      filter->silent = g_value_get_boolean (value);
      break;
    case PROP_KEYFRAME:
      filter->keyframe = g_value_get_int(value);
      break;
    case PROP_PROFILE_IDC:
      filter->profile_idc = g_value_get_int(value);
      break;
    case PROP_LEVEL_IDC:
      filter->level_idc = g_value_get_int(value);
      break;
    case PROP_QP:
      filter->qp = g_value_get_int(value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_cedarh264enc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  Gstcedarh264enc *filter = GST_CEDAR_H264ENC (object);

  switch (prop_id) {
    case PROP_SILENT:
      g_value_set_boolean (value, filter->silent);
      break;
    case PROP_KEYFRAME:
      g_value_set_int (value, filter->keyframe);
      break;
    case PROP_PROFILE_IDC:
      g_value_set_int (value, filter->profile_idc);
      break;
    case PROP_LEVEL_IDC:
      g_value_set_int (value, filter->level_idc);
      break;
    case PROP_QP:
      g_value_set_int (value, filter->qp);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/* GstElement vmethod implementations */

/* this function handles the link with other elements */
static gboolean
gst_cedarh264enc_set_caps (GstPad * pad, GstCaps * caps)
{
	Gstcedarh264enc *filter;
	GstPad *otherpad;
	GstCaps *othercaps;

	filter = GST_CEDAR_H264ENC (gst_pad_get_parent (pad));
	otherpad = (pad == filter->srcpad) ? filter->sinkpad : filter->srcpad;
  
	if (pad == filter->sinkpad) {
		int ret;
		int fps_num, fps_den;
		
		gst_video_format_parse_caps(caps, NULL, &filter->width, &filter->height);
		gst_video_parse_caps_framerate(caps, &fps_num, &fps_den);
		
		othercaps = gst_caps_copy (gst_pad_get_pad_template_caps(filter->srcpad));
		gst_caps_set_simple (othercaps,
			"width", G_TYPE_INT, filter->width,
			"height", G_TYPE_INT, filter->height,
			"framerate", GST_TYPE_FRACTION, fps_num, fps_den,
			"profile", G_TYPE_STRING, filter->profile_idc==66 ? "baseline" : "main" , NULL);
		if (filter->profile_idc!=66 && filter->profile_idc!=77) {
			fprintf(stderr,"please use either baseline or main\n");
			exit(1);
		}
		
		gst_object_unref (filter);
		ret = gst_pad_set_caps (otherpad, othercaps);
		gst_caps_unref(othercaps);
		
		return ret;
	}

	gst_object_unref (filter);
	return gst_pad_set_caps (otherpad, caps);
}

/* chain function
 * this function does the actual processing
 */
static GstFlowReturn
gst_cedarh264enc_chain (GstPad * pad, GstBuffer * buf)
{
	//fprintf(stderr,"Start encode\n");
	Gstcedarh264enc *filter;
	GstBuffer *outbuf;

	filter = GST_CEDAR_H264ENC (GST_OBJECT_PARENT (pad));

	if (!filter->input_buf || !filter->output_buf) {
		if (!alloc_cedar_bufs(filter)) {
			GST_ERROR("Cannot allocate cedar buffers");
			return GST_FLOW_ERROR;
		}
	}
	
	if (!GST_BUFFER_DATA(buf)) {
		// TODO: needed?
		GST_WARNING("Received empty buffer");
		outbuf = gst_buffer_new();
		gst_buffer_set_caps(outbuf, GST_PAD_CAPS(filter->srcpad));
		GST_BUFFER_TIMESTAMP(outbuf) = GST_BUFFER_TIMESTAMP(buf);
		
		return gst_pad_push (filter->srcpad, outbuf);
	}
	
	memcpy(filter->input_buf, GST_BUFFER_DATA(buf), GST_BUFFER_SIZE(buf));
	h264enc_encode_picture(encoder);	

	// TODO: use gst_pad_alloc_buffer
	//fprintf(stderr,"LENGTH OF ENCODED %u\n",encoder->bytestream_length);
	outbuf = gst_buffer_new_and_alloc(encoder->bytestream_length);
	gst_buffer_set_caps(outbuf, GST_PAD_CAPS(filter->srcpad));
	memcpy(GST_BUFFER_DATA(outbuf), filter->output_buf, GST_BUFFER_SIZE(outbuf));
	GST_BUFFER_TIMESTAMP(outbuf) = GST_BUFFER_TIMESTAMP(buf);
	gst_buffer_unref(buf);
	//fprintf(stderr,"End encode\n");
	GstFlowReturn r =  gst_pad_push (filter->srcpad, outbuf);
	//fprintf(stderr,"End encode x2\n");
	return r;
}

static GstStateChangeReturn
	gst_cedarh264enc_change_state (GstElement *element, GstStateChange transition)
{
	GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
	Gstcedarh264enc *cedarelement = GST_CEDAR_H264ENC(element);
	
	switch(transition) {
		case GST_STATE_CHANGE_NULL_TO_READY:
			if (!ve_open()) {
				GST_ERROR("Cannot open VE");
				return GST_STATE_CHANGE_FAILURE;
			}
			
			
			break;
		case GST_STATE_CHANGE_READY_TO_PAUSED:
			break;
		case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
			break;
		default:
			// silence compiler warning...
			break;
	}
	
	ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
	if (ret == GST_STATE_CHANGE_FAILURE)
		return ret;

	switch (transition) {
		case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
			break;
		case GST_STATE_CHANGE_PAUSED_TO_READY:
			break;
		case GST_STATE_CHANGE_READY_TO_NULL:
			cedarelement->width = cedarelement->height = 0;
			cedarelement->tile_w = cedarelement->tile_w2 = cedarelement->tile_h = cedarelement->tile_h2 = 0;
			cedarelement->mb_w = cedarelement->mb_h = cedarelement->plane_size = 0;
			//cedarelement->ve_regs = NULL;
			ve_close();
			//h264enc_free(encoder);
			break;
		default:
			// silence compiler warning...
			break;
	}
	
	return ret;
}

/* entry point to initialize the plug-in */
static gboolean
cedar_h264enc_init (GstPlugin * cedar_h264enc)
{
  // debug category for fltering log messages
  GST_DEBUG_CATEGORY_INIT (gst_cedarh264enc_debug, "cedar_h264enc",
      0, "CedarX H264 Encoder");

  return gst_element_register (cedar_h264enc, "cedar_h264enc", GST_RANK_NONE,
      GST_TYPE_CEDAR_H264ENC);
}

/* PACKAGE: this is usually set by autotools depending on some _INIT macro
 * in configure.ac and then written into and defined in config.h, but we can
 * just set it ourselves here in case someone doesn't use autotools to
 * compile this code. GST_PLUGIN_DEFINE needs PACKAGE to be defined.
 */
#ifndef PACKAGE
#define PACKAGE "myfirstcedar_h264enc"
#endif

// gstreamer looks for this structure to register cedar_h264encs
GST_PLUGIN_DEFINE (
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    "cedar_h264enc",
    "CedarX H264 Encoder",
    cedar_h264enc_init,
    VERSION,
    "LGPL",
    "Sunxi",
    "http://github.com/ebutera/gst-plugin-cedar"
)
