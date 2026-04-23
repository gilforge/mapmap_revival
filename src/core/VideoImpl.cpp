/*
 * VideoImpl.cpp
 *
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
#include "VideoImpl.h"
#include <cstring>
#include <iostream>
#include <QElapsedTimer>
#include <QThread>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QMutex>
#include <QCoreApplication>

namespace mmp {

// Direct-to-file logger that bypasses qInstallMessageHandler — useful because
// GStreamer plugins loaded lazily during pipeline creation may overwrite our
// Qt message handler. Opens/closes the file on each call (slow but reliable).
static void mmDirectLog(const QString& msg)
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

// -------- private implementation of VideoImpl -------

bool VideoImpl::hasVideoSupport()
{
  static bool did_print_gst_version = false;
  if (! did_print_gst_version)
  {
    qDebug() << "Using GStreamer version " <<
      GST_VERSION_MAJOR << "." << GST_VERSION_MINOR << "." << GST_VERSION_MICRO << Qt::endl;
    did_print_gst_version = true;
  }
  // TODO: actually check if we have it
  return true;
}

int VideoImpl::getWidth() const
{
  return _width;
//  Q_ASSERT(videoIsConnected());
//  return _padHandlerData.width;
}

int VideoImpl::getHeight() const
{
  return _height;
//  Q_ASSERT(videoIsConnected());
//  return _padHandlerData.height;
}

const uchar* VideoImpl::getBits()
{
  // Reset bits changed.
  _bitsChanged = false;

  // Return data.
  return (hasBits() ? _data : NULL);
}

QString VideoImpl::getUri() const
{
  return _uri;
}

void VideoImpl::setRate(double rate)
{
  if (rate == 0.0)
  {
    qDebug() << "Cannot set rate to zero, ignoring rate " << rate << Qt::endl;
    return;
  }

  // Only update rate if needed.
  if (_rate != rate)
  {
    _rate = rate;

    // Send seek events to activate rate.
    if (_seekEnabled)
      _updateRate();
  }
}

void VideoImpl::setVolume(double volume)
{
  // Only update volume if needed.
  if (_volume != volume)
  {
    _volume = volume;

    // Set volume element property
    if (audioIsSupported())
    {
      g_object_set (_audiovolume0, "mute", (_volume <= 0), NULL);
      g_object_set (_audiovolume0, "volume", _volume, NULL);
    }
    else
      qWarning() << "Cannot change volume cause this video does not support audio." << Qt::endl;
  }
}

void VideoImpl::build()
{
  qDebug() << "Building video impl";
  if (!loadMovie(_uri))
  {
    qDebug() << "Cannot load movie " << _uri << ".";
  }
}

VideoImpl::~VideoImpl()
{
  // Free all resources.
  freeResources();

  // Free mutex locker object.
  delete _mutexLocker;
}

bool VideoImpl::_eos() const
{
  if (_movieReady)
  {
    Q_ASSERT( _appsink0 );
    if (_rate > 0.0)
    {
      gboolean videoEos;
      g_object_get (G_OBJECT (_appsink0), "eos", &videoEos, NULL);
      return (bool) (videoEos);
    }
    else
    {
      /* Obtain the current position, needed for the seek event */
      gint64 position;
      if (!gst_element_query_position (_pipeline, GST_FORMAT_TIME, &position)) {
        g_printerr ("Unable to retrieve current position.\n");
        return false;
      }
      return (position == 0);
    }
  }
  else
    return false;
}

GstFlowReturn VideoImpl::gstNewSampleCallback(GstElement*, VideoImpl *p)
{
  static int callCount = 0;
  static QElapsedTimer rateTimer;
  static int rateWindowStart = 0;
  ++callCount;
  // Log only the first 3 calls to avoid flooding the log file.
  bool doLog = (callCount <= 3);
  if (doLog) mmDirectLog(QString("[gstNewSampleCallback] ENTER call#%1").arg(callCount));

  // Every 60 frames, log the effective arrival rate at the appsink.
  // This tells us whether saccades come from the decoder falling behind
  // (low rate) or from something else in the render loop (high rate).
  if (callCount == 1) {
    rateTimer.start();
    rateWindowStart = 1;
  } else if (callCount - rateWindowStart >= 60) {
    qint64 elapsedMs = rateTimer.restart();
    int frames = callCount - rateWindowStart;
    double fps = frames * 1000.0 / (double)(elapsedMs ? elapsedMs : 1);
    mmDirectLog(QString("[gstNewSampleCallback] appsink rate: %1 frames in %2ms = %3 fps")
                  .arg(frames).arg(elapsedMs).arg(fps, 0, 'f', 2));
    rateWindowStart = callCount;
  }

  // Make it thread-safe.
  p->lockMutex();

  // Get next frame.
  GstSample *sample = gst_app_sink_pull_sample(GST_APP_SINK(p->_appsink0));

  if (doLog) mmDirectLog(QString("[gstNewSampleCallback] pull_sample returned %1")
                           .arg(sample ? "non-NULL" : "NULL"));

  if (!sample) {
    p->unlockMutex();
    return GST_FLOW_OK;
  }

  // Unref last frame.
  p->_freeCurrentSample();

  // Set current frame.
  p->_currentFrameSample = sample;

  // For live sources, video dimensions have not been set, because
  // gstPadAddedCallback is never called. Fix dimensions from first sample /
  // caps we receive.
  if (( p->_width  == -1 ||
        p->_height == -1)) {
    GstCaps *caps = gst_sample_get_caps(sample);
    GstStructure *structure;
    structure = gst_caps_get_structure(caps, 0);
    gst_structure_get_int(structure, "width",  &p->_width);
    gst_structure_get_int(structure, "height", &p->_height);
    if (doLog) mmDirectLog(QString("[gstNewSampleCallback] caps dims %1x%2")
                             .arg(p->_width).arg(p->_height));
  }

  if (doLog) {
    GstCaps *caps = gst_sample_get_caps(sample);
    gchar *capsStr = caps ? gst_caps_to_string(caps) : g_strdup("NULL");
    mmDirectLog(QString("[gstNewSampleCallback] sample caps=%1").arg(capsStr));
    g_free(capsStr);
  }

  // Try to retrieve data bits of frame.
  GstMapInfo& map = p->_mapInfo;
  GstBuffer *buffer = gst_sample_get_buffer( sample );
  if (doLog) mmDirectLog(QString("[gstNewSampleCallback] buffer=%1 size=%2")
                           .arg(buffer ? "non-NULL" : "NULL")
                           .arg(buffer ? gst_buffer_get_size(buffer) : 0));

  if (gst_buffer_map(buffer, &map, GST_MAP_READ))
  {
    p->_currentFrameBuffer = buffer;
    // For debugging:
    //gst_util_dump_mem(map.data, map.size)

    // Retrieve data from map info.
    p->_data = map.data;

    // Bits have changed.
    p->_bitsChanged = true;
    // A new frame reached the appsink: the loop-restart flush has landed,
    // clear the "loop pending" guard so a future EOS can re-trigger a seek.
    p->_loopPending = false;
    if (doLog) mmDirectLog(QString("[gstNewSampleCallback] MAP OK data=%1 size=%2")
                             .arg((quintptr)map.data, 0, 16).arg((qint64)map.size));
  }
  else
  {
    if (doLog) mmDirectLog("[gstNewSampleCallback] gst_buffer_map FAILED");
  }

  p->unlockMutex();

  return GST_FLOW_OK;
}

VideoImpl::VideoImpl() :
_width(-1),
_height(-1),
_duration(0),
_fps(0.0),
_bitrate(0),
_codecName(),
_seekEnabled(false),
_pipeline(NULL),
_queue0(NULL),
_capsfilter0(NULL),
_videoscale0(NULL),
_videoconvert0(NULL),
_appsink0(NULL),
_audioqueue0(NULL),
_audioconvert0(NULL),
_audioresample0(NULL),
_audiovolume0(NULL),
_audiosink0(NULL),
_bus(NULL),
_currentFrameSample(NULL),
_currentFrameBuffer(NULL),
_bitsChanged(false),
_data(NULL),
//_isSeekable(false),
_rate(1.0),
_movieReady(false),
_playState(false),
_uri("")
{
  _mutexLocker = new QMutexLocker<QMutex>(&_mutex);
  _mutexLocker->unlock(); // Start unlocked; lockMutex()/unlockMutex() manage locking explicitly.

  QSettings settings;
  _playInLoop = settings.value("playInLoop", MM::PLAY_IN_LOOP).toBool();
}

void VideoImpl::unloadMovie()
{
  // Reset variables.
  _terminate = false;
  _seekEnabled = false;

  // Un-ready.
  _setMovieReady(false);
  setPlayState(false);

  // Free allocated resources / reinit.
  freeResources();
}

void VideoImpl::freeResources()
{
  // Free resources.
  if (_bus)
  {
    gst_object_unref (GST_OBJECT(_bus));
    _bus = NULL;
  }

  if (_pipeline)
  {
    // Shut down cleanly: PLAYING → PAUSED → READY → NULL.
    // Going directly to NULL can leave the WASAPI audio sink in an
    // inconsistent state (AUDCLNT_E_NOT_INITIALIZED) which emits a thud
    // on the speakers at app close. Each set_state call waits briefly
    // for the transition to complete.
    gst_element_set_state (_pipeline, GST_STATE_PAUSED);
    gst_element_get_state (_pipeline, NULL, NULL, 200 * GST_MSECOND);
    gst_element_set_state (_pipeline, GST_STATE_READY);
    gst_element_get_state (_pipeline, NULL, NULL, 200 * GST_MSECOND);
    gst_element_set_state (_pipeline, GST_STATE_NULL);
    gst_element_get_state (_pipeline, NULL, NULL, 200 * GST_MSECOND);
    gst_object_unref (GST_OBJECT(_pipeline));
    _pipeline = NULL;
  }

  // Free all components.
  _freeElement(&_queue0);
  _freeElement(&_capsfilter0);
  _freeElement(&_videoscale0);
  _freeElement(&_videoconvert0);
  _freeElement(&_appsink0);

  _freeElement(&_audioqueue0);
  _freeElement(&_audioconvert0);
  _freeElement(&_audioresample0);
  _freeElement(&_audiovolume0);
  _freeElement(&_audiosink0);

  qDebug() << "Freeing remaining samples/buffers" << Qt::endl;

  // Frees current sample and buffer.
  _freeCurrentSample();

  // Reset other informations.
  _bitsChanged = false;
  _width = _height = (-1);
  _duration = 0;
  _videoIsConnected = false;
  _audioIsConnected = false;
}

void VideoImpl::resetMovie()
{
  if (_seekEnabled)
  {
    // Debounce: reject loop-restarts that come too close together. After a
    // FLUSH seek, the pipeline clock sync can momentarily break and frames
    // are delivered in a burst, causing the whole clip to play through in
    // a few ms → immediate EOS → another resetMovie → ad infinitum seek
    // storm. The debounce caps the restart rate below any realistic loop
    // duration and breaks the feedback loop unconditionally.
    qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    if (nowMs - _lastLoopResetMs < LOOP_RESET_MIN_INTERVAL_MS)
    {
      mmDirectLog(QString("[resetMovie] debounced (%1ms since last reset)")
                    .arg(nowMs - _lastLoopResetMs));
      return;
    }
    _lastLoopResetMs = nowMs;

    // Mark a loop-restart as in-flight. Cleared in gstNewSampleCallback
    // when a new sample actually arrives, proving the flush completed.
    _loopPending = true;
    if (_rate > 0.0)
    {
      seekTo((guint64) 0);
      // NOTE: _updateRate() issues an additional seek. For a pure loop at
      // unchanged rate this is redundant and doubles the flushes — it is
      // only needed when the playback rate changes. Skipping it here
      // reduces the per-loop work to a single FLUSH seek.
    }
    else
    {
      // NOTE: Untested.
      seekTo(_duration);
    }
  }
  else
  {
    qDebug() << "Seeking not enabled: reloading the movie" << Qt::endl;
    loadMovie(_uri);
  }
}

bool VideoImpl::createVideoComponents()
{
  // Already supported?
  if (videoIsSupported())
    return true;

  // Create the video elements.
  _queue0 = gst_element_factory_make ("queue", "queue0");
  _videoconvert0 = gst_element_factory_make ("videoconvert", "videoconvert0");
  _videoscale0 = gst_element_factory_make ("videoscale", "videoscale0");
  _capsfilter0 = gst_element_factory_make ("capsfilter", "capsfilter0");
  _appsink0 = gst_element_factory_make ("appsink", "appsink0");

  // Verify that they were created.
  if (!_queue0 || !_videoconvert0 || ! _videoscale0 || ! _capsfilter0 || !_appsink0)
  {
    qWarning() << "Not all video elements could be created." << Qt::endl;
    if (! _pipeline) g_printerr("_pipeline");
    if (! _queue0) g_printerr("_queue0");
    if (! _videoconvert0) g_printerr("_videoconvert0");
    if (! _videoscale0) g_printerr("videoscale0");
    if (! _capsfilter0) g_printerr("capsfilter0");
    if (! _appsink0) g_printerr("_appsink0");
    return false;
  }

  // Add them to pipeline.
  gst_bin_add_many (GST_BIN (_pipeline),
                    _queue0, _videoconvert0, _videoscale0, _capsfilter0, _appsink0,
                    NULL);

  // Link.
  if (! gst_element_link_many (_queue0, _videoconvert0, _capsfilter0, _videoscale0, _appsink0, NULL))
  {
    qWarning() << "Could not link video queue, colorspace converter, caps filter, scaler and app sink." << Qt::endl;
    return false;
  }

  // Configure video appsink.
  GstCaps *videoCaps = gst_caps_from_string ("video/x-raw,format=RGBA");
  g_object_set (_capsfilter0, "caps", videoCaps, NULL);

  g_object_set (_appsink0, "emit-signals", TRUE,
                           "max-buffers", 1,     // only the latest frame is kept
                           "drop", TRUE,         // older frames are dropped
                           "sync", TRUE,         // respect pipeline clock → correct playback speed
                           "async", FALSE,       // don't block pipeline state transitions waiting for this sink
                           NULL);

  g_signal_connect (_appsink0, "new-sample", G_CALLBACK (VideoImpl::gstNewSampleCallback), this);
  gst_caps_unref (videoCaps);

  return true;
}

bool VideoImpl::createAudioComponents()
{
  // Already supported?
  if (audioIsSupported())
    return true;

  // Create the audio elements.
  _audioqueue0 = gst_element_factory_make ("queue", "audioqueue0");
  _audioconvert0 = gst_element_factory_make ("audioconvert", "audioconvert0");
  _audioresample0 = gst_element_factory_make ("audioresample", "audioresample0");
  _audiovolume0 = gst_element_factory_make ("volume", "audiovolume0");
  _audiosink0 = gst_element_factory_make ("autoaudiosink", "audiosink0");

  // Verify that they were created.
  if (!_audioqueue0 || !_audioconvert0 || !_audioresample0 || !_audiovolume0 || !_audiosink0)
  {
    qDebug() << "Not all audio elements could be created." << Qt::endl;
    if (! _audioqueue0) g_printerr("_audioqueue0");
    if (! _audioconvert0) g_printerr("_audioconvert0");
    if (! _audioresample0) g_printerr("_audioresample0");
    if (! _audiovolume0) g_printerr("_audiovolume0");
    if (! _audiosink0) g_printerr("_audiosink0");
    return false;
  }

  // Add them to pipeline.
  gst_bin_add_many (GST_BIN (_pipeline),
                    _audioqueue0, _audioconvert0, _audioresample0, _audiovolume0, _audiosink0,
                    NULL);

  // Link.
  if (! gst_element_link_many (_audioqueue0, _audioconvert0, _audioresample0,
                               _audiovolume0, _audiosink0, NULL))
  {
    qDebug() << "Could not link audio queue, converter, resampler and audio sink." << Qt::endl;
    return false;
  }

  // Configure audio appsink.
  // TODO: change from mono to stereo
  //  gchar* audioCapsText = g_strdup_printf ("audio/x-raw-float,channels=1,rate=%d,signed=(boolean)true,width=%d,depth=%d,endianness=BYTE_ORDER",
  //                                          Engine::signalInfo().sampleRate(), (int)(sizeof(Signal_T)*8), (int)(sizeof(Signal_T)*8) );
  //GstCaps* audioCaps = gst_caps_from_string (audioCapsText);
  /*
  GstCaps* audioCaps = gst_caps_from_string ("audio/xraw-float");
  g_object_set (_audioSink, "emit-signals", TRUE,
  "caps", audioCaps,
  "max-buffers", 1,     // only one buffer (the last) is maintained in the queue
  "drop", TRUE,         // ... other buffers are dropped
  "sync", TRUE,
  NULL);
  g_signal_connect (_audioSink, "new-buffer", G_CALLBACK (VideoImpl::gstNewAudioBufferCallback), this);
  gst_caps_unref (audioCaps);
  */
  //  g_free (audioCapsText);

  return true;
}

void VideoImpl::update()
{
  // Check for end-of-stream or terminate.
  if (_eos() || _terminate)
  {
    _setFinished(true);
    // Only trigger resetMovie if no loop-restart is already in flight.
    // Otherwise update() (60fps) re-fires resetMovie many times before the
    // pipeline has finished flushing → seek storm → burst frame delivery.
    if (_playInLoop && !_loopPending)
      resetMovie();
  }
  else
  {
    _setFinished(false);
  }

//  // Check if movie is ready and connected.
//  if (!isReady())
//  {
//    _bitsChanged = false;
//  }
//
  // Check gstreamer messages on bus.
  _checkMessages();
}

 bool VideoImpl::loadMovie(const QString& filename) {
   mmDirectLog(QString("[loadMovie] ENTER uri=%1").arg(filename));
   // Verify if file exists.
   const gchar* filetestpath = (const gchar*) filename.toUtf8().constData();
   if (FALSE == g_file_test(filetestpath, G_FILE_TEST_EXISTS))
   {
     qDebug() << "File " << filename << " does not exist" << Qt::endl;
     mmDirectLog("[loadMovie] file does not exist");
     return false;
   }

   qDebug() << "Opening movie: " << filename << ".";

   // Assign URI.
   _uri = filename;

   // Free previously allocated structures
   unloadMovie();

   // Prepare handler data.
   _videoIsConnected = false;
   _audioIsConnected = false;

   // Create the empty pipeline.
   _pipeline = gst_pipeline_new ( "video-source-pipeline" );
   if (!_pipeline)
   {
     qWarning() << "Pipeline could not be created." << Qt::endl;
     unloadMovie();
     return (-1);
   }

   // Create and link video components.
   if (!createVideoComponents())
   {
     qWarning() << "Video components could not be initialized." << Qt::endl;
     unloadMovie();
     return (-1);
   }

   //setVolume(0);

   // Listen to the bus.
   _bus = gst_element_get_bus (_pipeline);

   // Start playing.

   return true;
 }

bool VideoImpl::setPlayState(bool play)
{
  if (_pipeline == NULL)
  {
    return false;
  }

  // Change state.
  GstStateChangeReturn ret = gst_element_set_state (_pipeline, (play ? GST_STATE_PLAYING : GST_STATE_PAUSED));

//  // Wait until its done.
//  GstStateChangeReturn ret = gst_element_get_state (_pipeline, NULL, NULL, -1);
  if (ret == GST_STATE_CHANGE_FAILURE)
  {
    qDebug() << "Unable to set the pipeline to the playing state." << Qt::endl;
    //unloadMovie(); // <-- calling this created an infinite recursion
    return false;
  }
  else
  {
    _playState = play;
    return true;
  }
}

bool VideoImpl::seekTo(double position)
{
  gint64 duration;
  if (!gst_element_query_duration (_pipeline, GST_FORMAT_TIME, &duration))
  {
    qDebug() << "Cannot get duration of file" << Qt::endl;
    return false;
  }

  // Make sure position is in [0,1].
  position = qBound(0.0, position, 1.0);

  // Seek at position in nanoseconds.
  return seekTo((guint64)(position*duration));
}

bool VideoImpl::seekTo(guint64 positionNanoSeconds)
{
  if (!_pipeline || !_seekEnabled)
  {
    return false;
  }
  else
  {
    lockMutex();

    // Free the current sample and reset.
    _freeCurrentSample();
    _bitsChanged = false;

    // PAUSE → SEEK → PLAY pattern.
    //
    // Seeking a short clip while the pipeline is PLAYING leaves base_time
    // stale: the new segment's frames all become "due in the past" from
    // the clock's point of view, so appsink's sync=TRUE delivers the whole
    // clip in a burst (rotoscopie.mp4 = 1.42s was playing in ~60ms).
    //
    // Going to PAUSED first (and waiting for the state to settle), then
    // seeking with FLUSH, then returning to PLAYING, forces GStreamer to
    // recompute base_time from the current clock so frames pace correctly.
    GstState currentState = GST_STATE_NULL;
    gst_element_get_state(_pipeline, &currentState, NULL, 0);
    bool wasPlaying = (currentState == GST_STATE_PLAYING);

    if (wasPlaying)
    {
      gst_element_set_state(_pipeline, GST_STATE_PAUSED);
      gst_element_get_state(_pipeline, NULL, NULL, 200 * GST_MSECOND);
    }

    // IMPORTANT: seek on the whole _pipeline, NOT on _appsink0.
    // Seeking just the appsink does not propagate the FLUSH / segment event
    // properly to upstream elements. FLUSH clears buffered frames,
    // KEY_UNIT snaps to a keyframe for a clean restart.
    bool result = gst_element_seek_simple(
                    _pipeline, GST_FORMAT_TIME,
                    GstSeekFlags( GST_SEEK_FLAG_FLUSH
                                  | GST_SEEK_FLAG_ACCURATE
                                  | GST_SEEK_FLAG_KEY_UNIT ),
                    positionNanoSeconds);

    if (wasPlaying)
    {
      gst_element_set_state(_pipeline, GST_STATE_PLAYING);
    }

    mmDirectLog(QString("[seekTo] %1ns result=%2 wasPlaying=%3")
                  .arg((qint64)positionNanoSeconds).arg(result).arg(wasPlaying));

    unlockMutex();

    return result;
  }
}

//bool VideoImpl::_preRun()
//{
//  // Check for end-of-stream or terminate.
//  if (_eos() || _terminate)
//  {
//    _setFinished(true);
//    resetMovie();
//  }
//  else
//  {
//    _setFinished(false);
//  }
//  if (!_movieReady ||
//      !_padHandlerData.videoIsConnected)
//  {
//    return false;
//  }
//  return true;
//}

void VideoImpl::_checkMessages()
{
  if (_bus != NULL)
  {
    // Get message.
    GstMessage *msg = gst_bus_timed_pop_filtered(
                        _bus, 0,
                        (GstMessageType) (GST_MESSAGE_STATE_CHANGED | GST_MESSAGE_ERROR | GST_MESSAGE_EOS | GST_MESSAGE_ASYNC_DONE));

    if (msg != NULL)
    {
      GError *err;
      gchar *debug_info;

      switch (GST_MESSAGE_TYPE (msg))
      {
        // Error ////////////////////////////////////////////////
        case GST_MESSAGE_ERROR:
          gst_message_parse_error(msg, &err, &debug_info);
          qWarning() << "Error received from element " << GST_OBJECT_NAME (msg->src) << ": " << err->message << Qt::endl;
          qDebug() << "Debugging information: " << (debug_info ? debug_info : "none") << "." << Qt::endl;
          g_clear_error(&err);
          g_free(debug_info);

          if (!isLive())
          {
            _terminate = true;
          }
          else
          {
            gst_element_set_state (_pipeline, GST_STATE_PAUSED);
            gst_element_set_state (_pipeline, GST_STATE_NULL);
            gst_element_set_state (_pipeline, GST_STATE_READY);
          }
          //        _finish();
          break;

          // End-of-stream ////////////////////////////////////////
        case GST_MESSAGE_EOS:
          // Automatically loop back.
          if (_playInLoop) // Check if repeat mode is on
            resetMovie();
          //        _terminate = true;
          //        _finish();
          break;

          // Pipeline has prerolled/ready to play ///////////////
        case GST_MESSAGE_ASYNC_DONE:
          if (!_isMovieReady())
        {
          // Check if seeking is allowed.
          gint64 start, end;
          GstQuery *query = gst_query_new_seeking (GST_FORMAT_TIME);
          if (gst_element_query (_pipeline, query))
          {
            gst_query_parse_seeking (query, NULL, (gboolean*)&_seekEnabled, &start, &end);
            if (_seekEnabled)
            {
#ifdef VIDEO_IMPL_VERBOSE
              qDebug() << "Seeking is ENABLED from " << start << " to " << end << "." << Qt::endl;
#endif
            }
            else
            {
              qDebug() << "Seeking is DISABLED for this stream." << Qt::endl;
            }
          }
          else
          {
            qWarning() << "Seeking query failed." << Qt::endl;
          }

          gst_query_unref (query);

          // Movie is ready!
#ifdef VIDEO_IMPL_VERBOSE
          qDebug() << "Preroll done: movie is ready." << Qt::endl;
#endif // ifdef
          _setMovieReady(true);
        }

        break;

      case GST_MESSAGE_STATE_CHANGED:
        // We are only interested in state-changed messages from the pipeline.
        if (GST_MESSAGE_SRC (msg) == GST_OBJECT (_pipeline))
        {
          GstState oldState, newState, pendingState;
          gst_message_parse_state_changed(msg, &oldState, &newState, &pendingState);
#ifdef VIDEO_IMPL_VERBOSE
          qDebug() << "Pipeline state for movie " << _uri
                   << " changed from " << gst_element_state_get_name(oldState)
                   << " to " << gst_element_state_get_name(newState) << Qt::endl;
#endif
        }
        break;

      default:
        // We should not reach here.
        qWarning() << "Unexpected message received." << Qt::endl;
        break;
      }
      gst_message_unref(msg);
    }
  }
}

void VideoImpl::_setMovieReady(bool ready)
{
  _movieReady = ready;
}

void VideoImpl::_setFinished(bool finished)
{
  Q_UNUSED(finished);
  //  qDebug() << "Clip " << (finished ? "finished" : "not finished");
}

void  VideoImpl::_updateRate()
{
  // Check different things.
  if (_pipeline == NULL)
  {
    qWarning() << "Cannot set rate: no pipeline!" << Qt::endl;
    return;
  }

  if (!_seekEnabled)
  {
    qWarning() << "Cannot set rate: seek not working" << Qt::endl;
    return;
  }

  if (!_isMovieReady())
  {
    qWarning() << "Movie is not yet ready to play, cannot seek yet." << Qt::endl;
  }

  // Obtain the current position, needed for the seek event.
  gint64 position;
  if (!gst_element_query_position (_pipeline, GST_FORMAT_TIME, &position)) {
    qWarning() << "Unable to retrieve current position." << Qt::endl;
    return;
  }

  // Create the seek event.
  GstEvent *seekEvent;
  if (_rate > 0.0) {
    // Rate is positive (playing the video in normal direction)
    // Set new rate as a first argument. Provide position 0 so that we go to 0:00
    seekEvent = gst_event_new_seek (_rate, GST_FORMAT_TIME, GstSeekFlags( GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE ),
        GST_SEEK_TYPE_SET, position, GST_SEEK_TYPE_NONE, 0); // Go to 0:00
  } else {
    // Rate is negative
    // Set new rate as a first arguemnt. Provide the position we were already at.
    seekEvent = gst_event_new_seek (_rate, GST_FORMAT_TIME, GstSeekFlags( GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE ),
        GST_SEEK_TYPE_SET, 0, GST_SEEK_TYPE_SET, position);
  }

  // Send the seek event to the whole pipeline, not the appsink, so that the
  // pipeline clock and base_time are reset properly (see seekTo() comment).
  if (!gst_element_send_event (_pipeline, seekEvent)) {
    qWarning() << "Cannot perform seek event" << Qt::endl;
  }

  qDebug() << "Current rate: " << _rate << "." << Qt::endl;
}

void VideoImpl::_freeCurrentSample() {
  if (_currentFrameBuffer != NULL)
  {
    gst_buffer_unmap(_currentFrameBuffer, &_mapInfo);
  }

  if (_currentFrameSample != NULL)
  {
    gst_sample_unref(_currentFrameSample);
  }

  _currentFrameSample = NULL;
  _currentFrameBuffer = NULL;
  _data = NULL;
}

void VideoImpl::_freeElement(GstElement** element)
{
  if (*element)
  {
    *element = NULL;
  }
}

void VideoImpl::lockMutex()
{
  _mutexLocker->relock();
}

void VideoImpl::unlockMutex()
{
  _mutexLocker->unlock();
}

bool VideoImpl::waitForNextBits(int timeout, const uchar** bits)
{
  qInfo() << "[waitForNextBits] start, uri=" << _uri
          << "connected=" << videoIsConnected()
          << "timeout=" << timeout << "ms";
  mmDirectLog(QString("[waitForNextBits] START uri=%1 connected=%2 timeout=%3")
                .arg(_uri).arg(videoIsConnected()).arg(timeout));

  QElapsedTimer time;
  time.start();
  while (time.elapsed() < timeout)
  {
    if (_bus != NULL)
    {
      // Poll for diagnostic messages (non-blocking). Include STATE_CHANGED,
      // ASYNC_DONE, STREAM_START, EOS to see the pipeline sequence — they are
      // also handled by _checkMessages() later but that's fine, here we only
      // read and log (plus fail on errors).
      GstMessage *msg = gst_bus_timed_pop_filtered(
          _bus, 0,
          (GstMessageType)(GST_MESSAGE_ERROR | GST_MESSAGE_WARNING | GST_MESSAGE_ELEMENT
                           | GST_MESSAGE_STATE_CHANGED | GST_MESSAGE_ASYNC_DONE
                           | GST_MESSAGE_STREAM_START | GST_MESSAGE_EOS
                           | GST_MESSAGE_BUFFERING));

      if (msg != NULL)
      {
        switch (GST_MESSAGE_TYPE(msg))
        {
          // Hard pipeline error → fail immediately.
          case GST_MESSAGE_ERROR:
          {
            GError *err = nullptr;
            gchar *debug_info = nullptr;
            gst_message_parse_error(msg, &err, &debug_info);
            _loadError = QString("Erreur pipeline : %1").arg(err ? err->message : "inconnue");
            if (debug_info && *debug_info)
              _loadError += QString("\nDétails : %1").arg(debug_info);
            qWarning() << "Pipeline error during loading:" << (err ? err->message : "?") << Qt::endl;
            if (debug_info) qWarning() << "  debug:" << debug_info << Qt::endl;
            mmDirectLog(QString("[waitForNextBits] GST_MESSAGE_ERROR: %1 | debug=%2")
                          .arg(err ? err->message : "?")
                          .arg(debug_info ? debug_info : "none"));
            g_clear_error(&err);
            g_free(debug_info);
            gst_message_unref(msg);
            return false;
          }

          // Warning → log but keep waiting (don't fail).
          case GST_MESSAGE_WARNING:
          {
            GError *warn = nullptr;
            gchar *debug_info = nullptr;
            gst_message_parse_warning(msg, &warn, &debug_info);
            qWarning() << "Pipeline warning during loading:" << (warn ? warn->message : "?") << Qt::endl;
            if (debug_info && *debug_info) qWarning() << "  debug:" << debug_info << Qt::endl;
            mmDirectLog(QString("[waitForNextBits] GST_MESSAGE_WARNING: %1 | debug=%2")
                          .arg(warn ? warn->message : "?")
                          .arg(debug_info ? debug_info : "none"));
            g_clear_error(&warn);
            g_free(debug_info);
            break;
          }

          // State change on the pipeline — log the transition.
          case GST_MESSAGE_STATE_CHANGED:
          {
            GstState oldS, newS, pendS;
            gst_message_parse_state_changed(msg, &oldS, &newS, &pendS);
            mmDirectLog(QString("[waitForNextBits] STATE_CHANGED src=%1 %2 -> %3 (pending %4)")
                          .arg(GST_MESSAGE_SRC(msg) ? GST_OBJECT_NAME(GST_MESSAGE_SRC(msg)) : "?")
                          .arg(gst_element_state_get_name(oldS))
                          .arg(gst_element_state_get_name(newS))
                          .arg(gst_element_state_get_name(pendS)));
            break;
          }

          case GST_MESSAGE_ASYNC_DONE:
            mmDirectLog("[waitForNextBits] ASYNC_DONE (pipeline prerolled)");
            break;

          case GST_MESSAGE_STREAM_START:
            mmDirectLog("[waitForNextBits] STREAM_START");
            break;

          case GST_MESSAGE_EOS:
            mmDirectLog("[waitForNextBits] EOS");
            break;

          case GST_MESSAGE_BUFFERING:
          {
            gint percent = 0;
            gst_message_parse_buffering(msg, &percent);
            mmDirectLog(QString("[waitForNextBits] BUFFERING %1%").arg(percent));
            break;
          }

          // Element message → check for missing-plugin, fail immediately if found.
          case GST_MESSAGE_ELEMENT:
          {
            const GstStructure *s = gst_message_get_structure(msg);
            if (s)
            {
              gchar *structStr = gst_structure_to_string(s);
              qWarning() << "Pipeline element message during loading:" << structStr << Qt::endl;
              mmDirectLog(QString("[waitForNextBits] GST_MESSAGE_ELEMENT: %1").arg(structStr));
              g_free(structStr);

              if (gst_structure_has_name(s, "missing-plugin"))
              {
                gchar *desc = gst_missing_plugin_message_get_description(msg);
                _loadError = QString("Codec manquant : %1\n"
                                     "Conseil : installez gst-libav pour le support H.264/H.265.")
                                     .arg(desc ? desc : "inconnu");
                qWarning() << "Missing plugin:" << (desc ? desc : "?") << Qt::endl;
                mmDirectLog(QString("[waitForNextBits] MISSING-PLUGIN: %1").arg(desc ? desc : "?"));
                g_free(desc);
                gst_message_unref(msg);
                return false;
              }
            }
            break;
          }

          default:
            break;
        }
        gst_message_unref(msg);
      }
    }

    // First frame received.
    if (hasBits() && bitsHaveChanged())
    {
      if (bits)
        *bits = getBits();
      return true;
    }

    // Yield CPU to allow GStreamer callbacks to deliver frames.
    // NOTE: Do NOT call QCoreApplication::processEvents() here — it causes
    // re-entrant repainting during project loading (crash on partial mappings).
    QThread::msleep(10);
  }

  // Timed out — give a specific message based on pipeline state.
  qInfo() << "[waitForNextBits] TIMEOUT after" << timeout << "ms"
          << "connected=" << videoIsConnected()
          << "hasBits=" << hasBits()
          << "bitsChanged=" << bitsHaveChanged();
  mmDirectLog(QString("[waitForNextBits] TIMEOUT after %1ms connected=%2 hasBits=%3 bitsChanged=%4 data=%5")
                .arg(timeout)
                .arg(videoIsConnected())
                .arg(hasBits())
                .arg(bitsHaveChanged())
                .arg((quintptr)_data, 0, 16));

  if (_loadError.isEmpty())
  {
    if (!videoIsConnected())
      _loadError = "Aucun décodeur vidéo disponible pour ce fichier.\n"
                   "Le codec ou le profil vidéo n'est pas supporté par GStreamer.\n"
                   "Conseil : installez gst-libav pour le support H.264/H.265.";
    else
      _loadError = "Aucune image reçue (délai dépassé). "
                   "Le codec ou le profil vidéo n'est peut-être pas supporté.";
  }

  return false;
}

}
