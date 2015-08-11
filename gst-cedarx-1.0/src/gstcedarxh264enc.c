/*
 * GStreamer
 * Copyright (C) 2005 Thomas Vander Stichele <thomas@apestaart.org>
 * Copyright (C) 2005 Ronald S. Bultje <rbultje@ronald.bitfreak.net>
 * Copyright (C) 2015 root <<user@hostname.org>>
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
 * Alternatively, the contents of this file may be used under the
 * GNU Lesser General Public License Version 2.1 (the "LGPL"), in
 * which case the following provisions apply instead of the ones
 * mentioned above:
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/**
 * SECTION:element-cedarxh264enc
 *
 * FIXME:Describe cedarxh264enc here.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v -m fakesrc ! cedarxh264enc ! fakesink silent=TRUE
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <string.h>
#include <gst/gst.h>
#include <gst/video/video.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include "vencoder.h"
//#include "log.h"

#include "gstcedarxh264enc.h"

GST_DEBUG_CATEGORY_STATIC (gst_cedarxh264enc_debug);
#define GST_CAT_DEFAULT gst_cedarxh264enc_debug


char * encodings[] = { "baseline", "main", "high" }; 


VencBaseConfig baseConfig; 
VencAllocateBufferParam bufferParam;
VideoEncoder *pVideoEnc;
VencHeaderData sps_pps_data;
VencInputBuffer inputBuffer;
VencOutputBuffer outputBuffer;
unsigned int bitrate;
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
  PROP_BITRATE,
  PROP_SPS
};

/* the capabilities of the inputs and outputs.
 *
 * describe the real formats here.
 */
static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (
		"video/x-raw, "
			"format = (string) NV12, "
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
			"profile = (string) { main , high }"
	)
    );

#define gst_cedarxh264enc_parent_class parent_class
G_DEFINE_TYPE (Gstcedarxh264enc, gst_cedarxh264enc, GST_TYPE_ELEMENT);

static void gst_cedarxh264enc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_cedarxh264enc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static GstStateChangeReturn gst_cedarxh264enc_change_state (GstElement *element, GstStateChange transition);

static gboolean gst_cedarxh264enc_sink_event (GstPad * pad, GstObject * parent, GstEvent * event);
static GstFlowReturn gst_cedarxh264enc_chain (GstPad * pad, GstObject * parent, GstBuffer * buf);


static gboolean alloc_cedar_bufs(Gstcedarxh264enc *cedarelement)
{
	//clear the two parameter structs
	memset (&baseConfig, 0, sizeof (VencBaseConfig));
	memset (&bufferParam, 0, sizeof (VencAllocateBufferParam)); 

	int src_width = cedarelement->width;
	int src_height = cedarelement->height;

	baseConfig.nInputWidth = src_width;
	baseConfig.nInputHeight = src_height;
	baseConfig.nStride = src_width;

	int dst_width = cedarelement->width;
	int dst_height = cedarelement->height;

	baseConfig.nDstWidth = dst_width;
	baseConfig.nDstHeight = dst_height;
	baseConfig.eInputFormat = VENC_PIXEL_YUV420SP;


	//set the space for t Y and UV arrays , one is WxH the other is WxH/2
	bufferParam.nSizeY = baseConfig.nInputWidth * baseConfig.nInputHeight;
	bufferParam.nSizeC = baseConfig.nInputWidth * baseConfig.nInputHeight / 2;
	bufferParam.nBufferNum = 4;

	//* h264 param
	VencH264Param h264Param;
	h264Param.bEntropyCodingCABAC = 1;
	if (cedarelement->bitrate!=0) {
		h264Param.nBitrate = cedarelement->bitrate;	/* bps */
	} else {
		h264Param.nBitrate = 4 * 1024 * 1024;	/* bps */
	}
	bitrate = h264Param.nBitrate; //keep track of what the HW was set to last
	cedarelement->bitrate=bitrate; //keep track of what user set last
	h264Param.nFramerate = 30;	/* fps */
        if (cedarelement->keyframe!=0) {
		h264Param.nFramerate = cedarelement->keyframe;
	}
	h264Param.nCodingMode = VENC_FRAME_CODING;
	int codecType = VENC_CODEC_H264;

	h264Param.nMaxKeyInterval = h264Param.nFramerate;
	h264Param.sProfileLevel.nProfile = VENC_H264ProfileMain;
        if (cedarelement->profile_idc!=0) {
		h264Param.sProfileLevel.nProfile = cedarelement->profile_idc;
	}
	cedarelement->profile_idc=h264Param.sProfileLevel.nProfile;
	h264Param.sProfileLevel.nLevel = VENC_H264Level31;
        if (cedarelement->level_idc!=0) {
		h264Param.sProfileLevel.nLevel = cedarelement->level_idc;
	}
	cedarelement->level_idc=h264Param.sProfileLevel.nLevel;
	//how far we are willing the QP to vary?
	h264Param.sQPRange.nMinqp = 10;
	h264Param.sQPRange.nMaxqp = 40;

	//make the actual encoder object
	pVideoEnc = VideoEncCreate (codecType);
	VideoEncSetParameter (pVideoEnc, VENC_IndexParamH264Param, &h264Param);

	//set two parameters found in example
	int value=0;
	VideoEncSetParameter (pVideoEnc, VENC_IndexParamIfilter, &value);

	value = 0;			//degree
	VideoEncSetParameter (pVideoEnc, VENC_IndexParamRotation, &value);

	//set cyclic intrarefresh - default nBlockNumber?
	VencCyclicIntraRefresh sIntraRefresh;
	sIntraRefresh.bEnable = 1;
        sIntraRefresh.nBlockNumber = 10;
	VideoEncSetParameter (pVideoEnc, VENC_IndexParamH264CyclicIntraRefresh, &sIntraRefresh);
/*
//from https://github.com/leviathanch/media-codec/blob/fe9c4029af2d772a5a07cbdeb8daf1cbf0ab3d15/sunxi-cedarx/openmax/venc/omx_venc.cpp
case OMX_IndexParamVideoIntraRefresh:
{
	OMX_VIDEO_PARAM_INTRAREFRESHTYPE* pComponentParam = (OMX_VIDEO_PARAM_INTRAREFRESHTYPE*)paramData;
	if(pComponentParam->nPortIndex == 1 && pComponentParam->eRefreshMode == OMX_VIDEO_IntraRefreshCyclic)
	{
		int mbWidth, mbHeight;
		mbWidth = (m_sInPortDef.format.video.nFrameWidth + 15)/16;
		mbHeight = (m_sInPortDef.format.video.nFrameHeight + 15)/16;
		m_vencCyclicIntraRefresh.bEnable = 1;
		m_vencCyclicIntraRefresh.nBlockNumber = mbWidth*mbHeight/pComponentParam->nCirMBs;
		logd("m_vencCyclicIntraRefresh.nBlockNumber: %d", m_vencCyclicIntraRefresh.nBlockNumber);
	}
break;			
}
*/

  	VideoEncInit (pVideoEnc, &baseConfig);
        VideoEncGetParameter(pVideoEnc, VENC_IndexParamH264SPSPPS, &sps_pps_data); //THIS NEEDS TO STAY HERE! OR EVERYTHING BREAKS...
        assert(sps_pps_data.nLength>0);
                //logd("sps_pps_data.nLength: %d", sps_pps_data.nLength);
                //for(head_num=0; head_num<sps_pps_data.nLength; head_num++)
                //        logd("the sps_pps :%02x\n", *(sps_pps_data.pBuffer+head_num));

	AllocInputBuffer (pVideoEnc, &bufferParam);
	return TRUE;
}

/* GObject vmethod implementations */

static gboolean
gst_cedarxh264enc_query (GstPad    *pad,
		         GstObject *parent,
		         GstQuery  *query)
{
  gboolean ret;
  //Gstcedarxh264enc *filter = GST_CEDARXH264ENC (parent);
  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CAPS:
      {
      //g_print("GST_QUERY_CAPS %s\n",pad==filter->srcpad ? "SRC" : "SINK");
      GstCaps *tcaps  = gst_pad_get_pad_template_caps (pad);
      /*gchar *capsstr; 
      capsstr = gst_caps_to_string (tcaps);
      g_print ("caps: %s\n", capsstr); 
      g_free (capsstr); */
      gst_query_set_caps_result (query, tcaps);
      ret = TRUE;
      } 
      break;
    default:
      /* just call the default handler */
      ret = gst_pad_query_default (pad, parent, query);
      break;
  }
  return ret;
}

/* initialize the cedarxh264enc's class */
static void
gst_cedarxh264enc_class_init (Gstcedarxh264encClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  gobject_class->set_property = gst_cedarxh264enc_set_property;
  gobject_class->get_property = gst_cedarxh264enc_get_property;

  gstelement_class->change_state = gst_cedarxh264enc_change_state;

  gst_element_class_set_details_simple(gstelement_class,
    "cedarxh264enc",
    "Cedarx H264 Encoder",
    "H264 Encoder plugin for Cedarx",
    "Misko Dzamba <misko@cs.toronto.edu> ");

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&src_factory));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&sink_factory));

  g_object_class_install_property (gobject_class, PROP_SILENT,
      g_param_spec_boolean ("silent", "Silent", "Produce verbose output ?",
          FALSE, G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, PROP_KEYFRAME,
      g_param_spec_int ("keyframe", "keyframe", "keyframe rate",1,60,
          25, G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, PROP_PROFILE_IDC,
      g_param_spec_int ("profile_idc", "profile_idc", "profile_idc",1,254,
          VENC_H264ProfileMain, G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, PROP_LEVEL_IDC,
      g_param_spec_int ("level_idc", "level_idc", "level_idc",1,254,
          VENC_H264Level31, G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, PROP_BITRATE,
      g_param_spec_int ("bitrate", "bitrate", "bitrate", 10*1024,6 * 1024 * 1024 ,
          4 * 1024 *1024 , G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, PROP_SPS,
      g_param_spec_int ("sps", "sps", "sps",0,1,
          1, G_PARAM_READWRITE));
}

/* initialize the new element
 * instantiate pads and add them to element
 * set pad calback functions
 * initialize instance structure
 */
static void
gst_cedarxh264enc_init (Gstcedarxh264enc * filter)
{
  pVideoEnc=NULL;
  filter->keyframe=0;
  filter->profile_idc=VENC_H264ProfileMain;
  filter->level_idc=VENC_H264Level31;
  filter->bitrate=0;
  //sink pad
  filter->sinkpad = gst_pad_new_from_static_template (&sink_factory, "sink");
  gst_pad_set_event_function (filter->sinkpad,
                              GST_DEBUG_FUNCPTR(gst_cedarxh264enc_sink_event));
  gst_pad_set_chain_function (filter->sinkpad,
                              GST_DEBUG_FUNCPTR(gst_cedarxh264enc_chain));
  GST_PAD_SET_PROXY_CAPS (filter->sinkpad);
  gst_element_add_pad (GST_ELEMENT (filter), filter->sinkpad);

  //src pad
  filter->srcpad = gst_pad_new_from_static_template (&src_factory, "src");
  GST_PAD_SET_PROXY_CAPS (filter->srcpad);
  gst_element_add_pad (GST_ELEMENT (filter), filter->srcpad);

  //query on srcpad
  gst_pad_set_query_function (filter->srcpad,gst_cedarxh264enc_query);
  gst_pad_set_query_function (filter->sinkpad,gst_cedarxh264enc_query);

  filter->silent = FALSE;
}

static void
gst_cedarxh264enc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  Gstcedarxh264enc *filter = GST_CEDARXH264ENC (object);

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
    case PROP_BITRATE:
      filter->bitrate = g_value_get_int(value);
      break;
    case PROP_SPS:
      filter->write_sps_pps=1;
      filter->write_keyframe=1;
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_cedarxh264enc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  Gstcedarxh264enc *filter = GST_CEDARXH264ENC (object);

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
    case PROP_BITRATE:
      g_value_set_int (value, filter->bitrate);
      break;
    case PROP_SPS:
      g_value_set_int (value, filter->write_sps_pps);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/* GstElement vmethod implementations */


/* this function handles sink events */
//http://gstreamer.freedesktop.org/data/doc/gstreamer/head/pwg/html/section-nego-usecases.html
static gboolean
gst_cedarxh264enc_sink_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  gboolean ret=FALSE;
  Gstcedarxh264enc *filter;
  filter = GST_CEDARXH264ENC (parent);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CAPS:
    {
      GstCaps * caps;
      gst_event_parse_caps (event, &caps);
      if (pad == filter->sinkpad) {
	        GstPad * otherpad = (pad == filter->srcpad) ? filter->sinkpad : filter->srcpad;
		GstVideoInfo info;

		//lets get info from current caps
		gst_video_info_init(&info);
		gst_video_info_from_caps(&info, caps);
		filter->width=info.width;
		filter->height=info.height;	
	

		float level=filter->level_idc;
		level/=10;
		char level_str_l[10];
		sprintf(level_str_l, "%0.1f", level);
		char * level_str = strdup(level_str_l);
		fprintf(stderr, "%d %f %s\n",filter->level_idc, level , level_str);
		
		char * profile = encodings[0];
		if (filter->profile_idc==77) {
			profile=encodings[1];
		} else if (filter->profile_idc==100) {
			profile=encodings[2];
		}

		GstCaps * othercaps = gst_caps_copy (gst_pad_get_pad_template_caps(filter->srcpad));
		gst_caps_set_simple (othercaps,
			"width", G_TYPE_INT, info.width,
			"height", G_TYPE_INT, info.height,
			"framerate", GST_TYPE_FRACTION, info.fps_n, info.fps_d,
			"profile", G_TYPE_STRING,profile,
			"level", G_TYPE_STRING, level_str, 
			NULL);
		
		ret = gst_pad_set_caps (otherpad, othercaps);
		gst_caps_unref(othercaps);
      } else {
      //ret = gst_pad_push_event (filter->srcpad, event);
        ret = gst_pad_event_default (pad, parent, event);
      }
      break;
    }
    default:
      ret = gst_pad_event_default (pad, parent, event);
      break;
  }
  return ret;
}


/* chain function
 * this function does the actual processing
 */
static GstFlowReturn
gst_cedarxh264enc_chain (GstPad * pad, GstObject * parent, GstBuffer * buf)
{
  Gstcedarxh264enc *filter;
  GstBuffer *outbuf;

  filter = GST_CEDARXH264ENC (parent);

  //if we have not initialized the engine, do it now
  if (pVideoEnc==NULL) {
	alloc_cedar_bufs(filter);

  }

  GstMapInfo in_map, out_map;

  //map the input
  gst_buffer_map (buf, &in_map, GST_MAP_READ);

  //fprintf(stderr,"input mapped\n");
  if (G_UNLIKELY (in_map.size <= 0)) {
    // TODO: needed?
    GST_WARNING("Received empty buffer?");
    outbuf = gst_buffer_new();
    //gst_buffer_set_caps(outbuf, GST_PAD_CAPS(filter->srcpad));
    GST_BUFFER_TIMESTAMP(outbuf) = GST_BUFFER_TIMESTAMP(buf);
    return gst_pad_push (filter->srcpad, outbuf);
  }


  //if we force write keyframe
  if (filter->write_keyframe==1) {
    int value=1;
    VideoEncSetParameter (pVideoEnc, VENC_IndexParamForceKeyFrame, &value);
    filter->write_keyframe=0;
  }

  //if user set bitrate and HW bitrate is out of sync, sync it
  if (bitrate!=filter->bitrate) {
    int value=bitrate;
    VideoEncSetParameter (pVideoEnc, VENC_IndexParamBitrate, &value);
    bitrate=filter->bitrate;
  }

  GetOneAllocInputBuffer (pVideoEnc, &inputBuffer);

  char * y_buf = in_map.data;
  char * c_buf = in_map.data + baseConfig.nInputWidth * baseConfig.nInputHeight;

  memcpy(inputBuffer.pAddrVirY, y_buf, baseConfig.nInputWidth * baseConfig.nInputHeight);
  memcpy(inputBuffer.pAddrVirC, c_buf, (baseConfig.nInputWidth * baseConfig.nInputHeight )/2);

  //no idea what this does...
  inputBuffer.bEnableCorp = 0;
  inputBuffer.sCropInfo.nLeft = 240;
  inputBuffer.sCropInfo.nTop = 240;
  inputBuffer.sCropInfo.nWidth = 240;
  inputBuffer.sCropInfo.nHeight = 240;

  FlushCacheAllocInputBuffer (pVideoEnc, &inputBuffer);

  AddOneInputBuffer (pVideoEnc, &inputBuffer);
  VideoEncodeOneFrame (pVideoEnc);

  AlreadyUsedInputBuffer (pVideoEnc, &inputBuffer);
  ReturnOneAllocInputBuffer (pVideoEnc, &inputBuffer);

  GetOneBitstreamFrame (pVideoEnc, &outputBuffer);
  
  int keyframe = outputBuffer.nFlag & 1; //is this a keyframe?

  //attach SPS to each keyframe
  if (keyframe==1 || filter->write_sps_pps==1) {
	  outbuf = gst_buffer_new_and_alloc(outputBuffer.nSize0+sps_pps_data.nLength);
	  gst_buffer_map (outbuf, &out_map, GST_MAP_WRITE);
	  VideoEncGetParameter (pVideoEnc, VENC_IndexParamH264SPSPPS, &sps_pps_data);
	  memcpy(out_map.data, sps_pps_data.pBuffer,sps_pps_data.nLength);
	  memcpy(out_map.data+sps_pps_data.nLength, outputBuffer.pData0, outputBuffer.nSize0);
	  filter->write_sps_pps=0;
  } else {
	  outbuf = gst_buffer_new_and_alloc(outputBuffer.nSize0);
	  gst_buffer_map (outbuf, &out_map, GST_MAP_WRITE);
	  memcpy(out_map.data, outputBuffer.pData0, outputBuffer.nSize0);
  }

  if (outputBuffer.nSize1) {
	fprintf(stderr,"Uh oh...!\n");
  }
  FreeOneBitStreamFrame (pVideoEnc, &outputBuffer);
  GST_BUFFER_TIMESTAMP(outbuf) = GST_BUFFER_TIMESTAMP(buf);

  gst_buffer_unmap (buf, &in_map);
  gst_buffer_unmap (outbuf, &out_map);
  gst_buffer_unref (buf);
  GstFlowReturn r =  gst_pad_push (filter->srcpad, outbuf);

  return r;
}

static GstStateChangeReturn
	gst_cedarxh264enc_change_state (GstElement *element, GstStateChange transition)
{
	GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
	Gstcedarxh264enc *cedarelement = GST_CEDARXH264ENC(element);
	switch(transition) {
		case GST_STATE_CHANGE_NULL_TO_READY:
			g_print("CEDAR | NULL -> READY\n");
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
			g_print("CEDARX | READY -> NULL \n");
			if (pVideoEnc!=NULL) {
				ReleaseAllocInputBuffer(pVideoEnc);
				VideoEncUnInit(pVideoEnc);
				VideoEncDestroy(pVideoEnc);
				pVideoEnc=NULL;
			}
			//h264enc_free(encoder);
			break;
		default:
			// silence compiler warning...
			break;
	}
	
	return ret;
}

/* entry point to initialize the plug-in
 * initialize the plug-in itself
 * register the element factories and other features
 */
static gboolean
cedarxh264enc_init (GstPlugin * cedarxh264enc)
{
  /* debug category for fltering log messages
   *
   * exchange the string 'Template cedarxh264enc' with your description
   */
  GST_DEBUG_CATEGORY_INIT (gst_cedarxh264enc_debug, "cedarxh264enc",
      0, "Cedarx H264 cedarxh264enc");

  return gst_element_register (cedarxh264enc, "cedarxh264enc", GST_RANK_NONE,
      GST_TYPE_CEDARXH264ENC);
}

/* PACKAGE: this is usually set by autotools depending on some _INIT macro
 * in configure.ac and then written into and defined in config.h, but we can
 * just set it ourselves here in case someone doesn't use autotools to
 * compile this code. GST_PLUGIN_DEFINE needs PACKAGE to be defined.
 */
#ifndef PACKAGE
#define PACKAGE "myfirstcedarxh264enc"
#endif

/* gstreamer looks for this structure to register cedarxh264encs
 *
 * exchange the string 'Template cedarxh264enc' with your cedarxh264enc description
 */
GST_PLUGIN_DEFINE (
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    cedarxh264enc,
    "CEDRUS cedarxh264enc",
    cedarxh264enc_init,
    VERSION,
    "LGPL",
    "GStreamer",
    "http://gstreamer.net/"
)
