/*
 * VideoUriDecodeBinImpl.cpp
 *
 * (c) 2016 Vasilis Liaskovitis -- vliaskov@gmail.com
 * (c) 2013 Sofian Audry -- info(@)sofianaudry(.)com
 * (c) 2013 Alexandre Quessy -- alexandre(@)quessy(.)net
 * (c) 2012 Jean-Sebastien Senecal
 * (c) 2004 Mathieu Guindon, Julien Keable
 *           Based on code from Drone http://github.com/sofian/drone
 *           Based on code from the GStreamer Tutorials http://docs.gstreamer.com/display/GstSDK/Tutorials
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "VideoUriDecodeBinImpl.h"
#include <cstring>
#include <iostream>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QMutex>
#include <QCoreApplication>

namespace mmp {

// Same helper as in VideoImpl.cpp — duplicated on purpose so we don't have to
// refactor a header just to share this diagnostic log writer.
static void mmDirectLog2(const QString& msg)
{
  static QMutex mutex;
  QMutexLocker lock(&mutex);
  QFile f(QCoreApplication::applicationDirPath() + "/mapmap_direct.log");
  if (f.open(QIODevice::Append | QIODevice::Text)) {
    QTextStream out(&f);
    out << QDateTime::currentDateTime().toString("hh:mm:ss.zzz")
        << " " << msg << "\n";
    out.flush();
  }
}

VideoUriDecodeBinImpl::VideoUriDecodeBinImpl() :
_uridecodebin0(NULL)
{
}

void VideoUriDecodeBinImpl::gstPadAddedCallback(GstElement *src, GstPad *newPad, VideoUriDecodeBinImpl* p)
{
  Q_UNUSED(src);

  GstPad *sinkPad = NULL;

  // Check the new pad's type.
  GstCaps *newPadCaps = gst_pad_query_caps (newPad, NULL);
  GstStructure *newPadStruct = gst_caps_get_structure (newPadCaps, 0);
  const gchar *newPadType   = gst_structure_get_name (newPadStruct);
  gchar *newPadStructStr = gst_structure_to_string(newPadStruct);
  // Always log pad info — essential for codec diagnosis.
  qInfo() << "pad-added: '" << GST_PAD_NAME(newPad) << "' type=" << newPadType
          << " caps=" << newPadStructStr;
  mmDirectLog2(QString("[pad-added] name='%1' type='%2' caps=%3")
                 .arg(GST_PAD_NAME(newPad))
                 .arg(newPadType)
                 .arg(newPadStructStr));
  g_free(newPadStructStr);

  bool isVideoPad = g_str_has_prefix (newPadType, "video/x-raw");
  bool isAudioPad = g_str_has_prefix (newPadType, "audio/x-raw");

  // Check for video pads.
  if (isVideoPad)
  {
    sinkPad = gst_element_get_static_pad (p->_queue0, "sink");
    gst_structure_get_int(newPadStruct, "width",  &p->_width);
    gst_structure_get_int(newPadStruct, "height", &p->_height);
  }

  // Check for audio pads.
  else if (isAudioPad)
  {
    if (!p->createAudioComponents())
    {
      qWarning() << "Problem creating audio components." << Qt::endl;
      goto exit;
    }
    sinkPad = gst_element_get_static_pad (p->_audioqueue0, "sink");
  }

  // Other types: ignore.
  else {
    qDebug() << "  It has type '" << newPadType << "' which is not raw audio/video: ignored." << Qt::endl;
    goto exit;
  }


  // If our converter is already linked, we have nothing to do here.
  if (gst_pad_is_linked (sinkPad))
  {
    // Best prefixes.
    if (isVideoPad || isAudioPad)
    {
      qDebug() << "  Found a better pad." << Qt::endl;
      GstPad* oldPad = gst_pad_get_peer(sinkPad);
      gst_pad_unlink(oldPad, sinkPad);
      g_object_unref(oldPad);
    }
    else
    {
#ifdef VIDEO_IMPL_VERBOSE
      qDebug() << "  We are already linked: ignoring." << Qt::endl;
#endif
      goto exit;
    }
  }

  // Attempt the link.
  if (GST_PAD_LINK_FAILED (gst_pad_link (newPad, sinkPad)))
  {
    qInfo() << "  pad-added: link FAILED for type '" << newPadType << "'.";
    mmDirectLog2(QString("[pad-added] link FAILED type='%1'").arg(newPadType));
    goto exit;
  }
  else
  {
    if (isVideoPad)
    {
      p->videoConnect();
      qInfo() << "  pad-added: video link OK.";
      mmDirectLog2("[pad-added] video link OK");
    }
    else if (isAudioPad)
    {
      p->audioConnect();
      qInfo() << "  pad-added: audio link OK.";
      mmDirectLog2("[pad-added] audio link OK");
    }
    else
      qWarning() << "Error: this pad is neither valid audio or video." << Qt::endl;
  }

exit:
  // Unreference the new pad's caps, if we got them.
  if (newPadCaps != NULL)
  {
    gst_caps_unref (newPadCaps);
  }

  // Unreference the sink pad.
  if (sinkPad != NULL)
  {
    gst_object_unref (sinkPad);
  }
}

bool VideoUriDecodeBinImpl::loadMovie(const QString& path) {
  VideoImpl::loadMovie(path);

  _uridecodebin0 = gst_element_factory_make("uridecodebin", NULL);

  if ( !_uridecodebin0)
  {
    qWarning() << "Not all elements could be created." << Qt::endl;
    unloadMovie();
    return (-1);
  }

  // Build the pipeline. Note that we are NOT linking the source at this
  // point. We will do it later.
  gst_bin_add_many (GST_BIN (_pipeline),
      _uridecodebin0,
      NULL);

  // Process URI.
  QByteArray ba = path.toLocal8Bit();
  gchar *filename_tmp = g_strdup((gchar*) path.toUtf8().constData());
  gchar* uri = (gchar*) path.toUtf8().constData();
  if (! gst_uri_is_valid(uri))
  {
    // Try to convert filename to URI.
    GError* error = NULL;
    qDebug() << "Calling gst_filename_to_uri : " << uri << Qt::endl;
    uri = gst_filename_to_uri(filename_tmp, &error);
    if (error)
    {
      qDebug() << "Filename to URI error: " << error->message << Qt::endl;
      g_clear_error(&error);
      gst_object_unref (uri);
      freeResources();
      return false;
    }
  }
  g_free(filename_tmp);

// Connect to the pad-added signal
  // Extract meta info.
  GError* error = NULL;
  GstDiscoverer* discoverer = gst_discoverer_new(5*GST_SECOND, &error);
  if (!discoverer)
  {
    qDebug() << "Error creating discoverer: " << error->message << Qt::endl;
    g_clear_error (&error);
    return false;
  }

  GstDiscovererInfo* info = gst_discoverer_discover_uri(discoverer, uri, &error);

  if (!info)
  {
    qDebug() << "Error getting discoverer info: " << error->message << Qt::endl;
    g_clear_error (&error);
    return false;
  }

  GstDiscovererResult result = gst_discoverer_info_get_result(info);

  switch (result) {
    case GST_DISCOVERER_URI_INVALID:
      _loadError = QString("Fichier invalide ou URI incorrecte : %1").arg(uri);
      qDebug() << _loadError << Qt::endl;
      break;
    case GST_DISCOVERER_ERROR:
      _loadError = QString("Erreur GStreamer : %1").arg(error ? error->message : "inconnue");
      qDebug() << _loadError << Qt::endl;
      break;
    case GST_DISCOVERER_TIMEOUT:
      _loadError = "Délai dépassé lors de l'analyse du fichier vidéo.";
      qDebug() << _loadError << Qt::endl;
      break;
    case GST_DISCOVERER_BUSY:
      _loadError = "GStreamer occupé, réessayez.";
      qDebug() << _loadError << Qt::endl;
      break;
    case GST_DISCOVERER_MISSING_PLUGINS: {
      const GstStructure *s = gst_discoverer_info_get_misc(info);
      gchar *str = s ? gst_structure_to_string(s) : g_strdup("inconnu");
      _loadError = QString("Codec vidéo non supporté. Plugin GStreamer manquant : %1\n"
                           "Conseil : installez gst-libav pour le support H.264/H.265.").arg(str);
      qDebug() << _loadError << Qt::endl;
      g_free(str);
      break;
    }
    case GST_DISCOVERER_OK:
      _loadError.clear();
      qDebug() << "Discovered '" << uri << "'" << Qt::endl;
      break;
  }

  g_clear_error(&error);

  if (result != GST_DISCOVERER_OK) {
    return false;
  }

  // Gather info from video.
  GList *videoStreams = gst_discoverer_info_get_video_streams (info);
  if (!videoStreams)
  {
    qDebug() << "This URI does not contain any video streams" << Qt::endl;
    return false;
  }

  // Retrieve meta-info.
  GstDiscovererVideoInfo *vinfo = (GstDiscovererVideoInfo*)videoStreams->data;
  _width    = gst_discoverer_video_info_get_width(vinfo);
  _height   = gst_discoverer_video_info_get_height(vinfo);
  _duration = gst_discoverer_info_get_duration(info);
  _seekEnabled = gst_discoverer_info_get_seekable(info);

  // Framerate.
  guint fpsNum = gst_discoverer_video_info_get_framerate_num(vinfo);
  guint fpsDen = gst_discoverer_video_info_get_framerate_denom(vinfo);
  _fps = (fpsDen > 0) ? (double)fpsNum / fpsDen : 0.0;

  // Bitrate (use max-bitrate if bitrate is 0).
  _bitrate = gst_discoverer_video_info_get_bitrate(vinfo);
  if (_bitrate == 0)
    _bitrate = gst_discoverer_video_info_get_max_bitrate(vinfo);

  // Codec name from stream caps.
  GstCaps *caps = gst_discoverer_stream_info_get_caps(
                    GST_DISCOVERER_STREAM_INFO(vinfo));
  if (caps) {
    gchar *capsStr = gst_caps_to_string(caps);
    // Extract just the first token (e.g. "video/x-h264" → "H.264")
    QString raw = QString::fromUtf8(capsStr);
    raw = raw.section(',', 0, 0).trimmed(); // keep first cap field only
    // Pretty-print common codec names.
    if      (raw.contains("x-h264"))  _codecName = "H.264";
    else if (raw.contains("x-h265"))  _codecName = "H.265 (HEVC)";
    else if (raw.contains("x-vp8"))   _codecName = "VP8";
    else if (raw.contains("x-vp9"))   _codecName = "VP9";
    else if (raw.contains("x-av1"))   _codecName = "AV1";
    else if (raw.contains("x-xvid") || raw.contains("x-divx")) _codecName = "MPEG-4";
    else if (raw.contains("x-theora")) _codecName = "Theora";
    else                               _codecName = raw;
    g_free(capsStr);
    gst_caps_unref(caps);
  }

  // Log video specs for diagnosis (codec, resolution, fps, duration, bitrate).
  mmDirectLog2(QString("[VideoSpecs] codec=%1 %2x%3 fps=%4 duration=%5s bitrate=%6kbps seekable=%7")
                 .arg(_codecName)
                 .arg(_width).arg(_height)
                 .arg(_fps, 0, 'f', 2)
                 .arg(_duration / (double)GST_SECOND, 0, 'f', 2)
                 .arg(_bitrate / 1000)
                 .arg(_seekEnabled));

  // Free everything.
  g_object_unref(discoverer);
  gst_discoverer_info_unref(info);
  gst_discoverer_stream_info_list_free(videoStreams);

  // Connect pad signal.
  g_signal_connect (_uridecodebin0, "pad-added", G_CALLBACK (VideoUriDecodeBinImpl::gstPadAddedCallback), this);

  // Set uri of decoder.
  g_object_set (_uridecodebin0, "uri", uri, NULL);

  mmDirectLog2(QString("[VideoUriDecodeBin::loadMovie] setPlayState(true) uri=%1").arg(uri));
  bool playOk = setPlayState(true);
  mmDirectLog2(QString("[VideoUriDecodeBin::loadMovie] setPlayState returned %1").arg(playOk));

  return true;
}

VideoUriDecodeBinImpl::~VideoUriDecodeBinImpl()
{
}

}
