/*
 * MainApplication.cpp
 *
 * (c) 2014 Sofian Audry -- info(@)sofianaudry(.)com
 * (c) 2014 Alexandre Quessy -- alexandre(@)quessy(.)net
 * (c) 2016 Dame Diongue -- baydamd(@)gmail(.)com
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

#include "MainApplication.h"

#include <QSurfaceFormat>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QMutex>

namespace mmp {

// ---------------------------------------------------------------------------
// Lightweight file logger — writes all qDebug/qWarning to mapmap_debug.log.
// Useful on Windows where GUI apps don't emit to stdout.
// ---------------------------------------------------------------------------
static QFile   *s_logFile   = nullptr;
static QMutex   s_logMutex;

static void mmMessageHandler(QtMsgType type, const QMessageLogContext& /*ctx*/, const QString& msg)
{
  QMutexLocker lock(&s_logMutex);
  if (!s_logFile || !s_logFile->isOpen()) return;

  QTextStream out(s_logFile);
  const char* prefix = "[D]";
  if      (type == QtWarningMsg)  prefix = "[W]";
  else if (type == QtCriticalMsg) prefix = "[C]";
  else if (type == QtFatalMsg)    prefix = "[F]";

  out << QDateTime::currentDateTime().toString("hh:mm:ss.zzz")
      << " " << prefix << " " << msg << "\n";
  out.flush();
  s_logFile->flush(); // force OS flush to disk
}

MainApplication::MainApplication(int &argc, char *argv[])
  : QApplication(argc, argv)
{
  // --- File logger (debug) -------------------------------------------------
  // On Windows, qDebug() in GUI apps goes to OutputDebugString (not stdout).
  // This handler writes everything to mapmap_debug.log next to the exe.
  s_logFile = new QFile(QCoreApplication::applicationDirPath() + "/mapmap_debug.log");
  if (s_logFile->open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
    // Write a known sentinel so we can verify the file is reachable.
    QTextStream startMsg(s_logFile);
    startMsg << "=== MapMap Debug Log Started at "
             << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")
             << " ===\n";
    startMsg << "appDir: " << QCoreApplication::applicationDirPath() << "\n";
    s_logFile->flush();
    qInstallMessageHandler(mmMessageHandler);
  }
  // -------------------------------------------------------------------------

  // Set default OpenGL surface format with VSync enabled.
  // This helps prevent flickering on Intel integrated GPUs.
  QSurfaceFormat format = QSurfaceFormat::defaultFormat();
  format.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
  format.setSwapInterval(1); // VSync on
  QSurfaceFormat::setDefaultFormat(format);
#ifdef Q_OS_WIN32
  // Set GStreamer plugins path on Windows
  QString appDir = QCoreApplication::applicationDirPath();
  QString pluginPath = appDir + "/lib/gstreamer-1.0";

  // Override the compiled-in system plugin path with a non-existent directory
  // so that a system-wide GStreamer installation (e.g. C:\Program Files\gstreamer)
  // does not conflict with our portable plugins.
  qputenv("GST_PLUGIN_SYSTEM_PATH_1_0", "C:\\NoGStreamerHere");
  qputenv("GST_PLUGIN_SYSTEM_PATH",     "C:\\NoGStreamerHere");
  qputenv("GST_PLUGIN_PATH", pluginPath.toLocal8Bit());

  // Prepend app directory to PATH so GStreamer DLLs are found
  QString currentPath = qgetenv("PATH");
  QString newPath = appDir + ";" + currentPath;
  qputenv("PATH", newPath.toLocal8Bit());

  // Portable mode: store settings in the application directory
  QSettings::setDefaultFormat(QSettings::IniFormat);
  QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, appDir);

  // --- GStreamer debug logging ------------------------------------------
  // Route GST_DEBUG to a file next to the exe. Level 3 gives us FIXME+ERROR
  // + WARNING messages from all categories — enough to diagnose negotiation
  // failures, blocked state changes, decoder issues, etc. Without this,
  // GStreamer writes to stderr which Windows GUI apps discard.
  qputenv("GST_DEBUG", "3");
  qputenv("GST_DEBUG_FILE", QFile::encodeName(appDir + "/mapmap_gst.log"));
  qputenv("GST_DEBUG_NO_COLOR", "1");
#endif

  // Initialize GStreamer.
  gst_init (NULL, NULL);

  // Some GStreamer Qt-integration plugins may call qInstallMessageHandler()
  // during gst_init(), overwriting ours. Reinstall it here to be sure.
  qInstallMessageHandler(mmMessageHandler);
  qInfo() << "GStreamer initialized. Log handler active.";

  // Hardware video decoders (D3D11, MediaFoundation) output NV12 frames in
  // GPU memory (D3D11 textures), which our CPU-based appsink cannot access.
  // On multi-GPU systems, GStreamer registers device-specific variants
  // (e.g. d3d11h264device0dec, d3d11h264device1dec) that cannot be listed
  // statically. We therefore iterate ALL registered video decoder factories
  // and disable any that belong to a hardware-acceleration plugin.
  {
    GstRegistry *registry = gst_registry_get();

    // Plugins that produce GPU-memory buffers incompatible with our pipeline.
    // NOTE: d3d12 was added in GStreamer 1.26 — without it, marche2.mp4 / feu.mp4
    // get decoded by d3d12h264dec which outputs D3D12Memory buffers that our
    // CPU-based appsink cannot map, resulting in a silent pipeline (no frames
    // ever delivered even though decodebin reports the pad as linked).
    const char* hwPlugins[] = {
      "d3d11", "d3d12", "mediafoundation", "dxva2",
      "va", "vaapi", "nvcodec", "qsv", "amfcodec",
      nullptr
    };

    GList *allDecoders = gst_element_factory_list_get_elements(
        GST_ELEMENT_FACTORY_TYPE_DECODER | GST_ELEMENT_FACTORY_TYPE_MEDIA_VIDEO,
        GST_RANK_MARGINAL);

    for (GList *l = allDecoders; l != NULL; l = l->next) {
      GstElementFactory *factory = GST_ELEMENT_FACTORY(l->data);
      GstPluginFeature   *feat   = GST_PLUGIN_FEATURE(factory);
      const gchar *pluginName    = gst_plugin_feature_get_plugin_name(feat);
      if (!pluginName) continue;

      for (int i = 0; hwPlugins[i]; i++) {
        if (g_str_equal(pluginName, hwPlugins[i])) {
          qInfo() << "Disabling hardware decoder (GPU memory):"
                  << gst_plugin_feature_get_name(feat)
                  << "from plugin" << pluginName;
          gst_plugin_feature_set_rank(feat, GST_RANK_NONE);
          break;
        }
      }
    }
    gst_plugin_feature_list_free(allDecoders);
  }

  // Set application information.
  setApplicationName(MM::APPLICATION_NAME);
  setApplicationVersion(MM::VERSION);
  setOrganizationName(MM::ORGANIZATION_NAME);
  setOrganizationDomain(MM::ORGANIZATION_DOMAIN);
}

MainApplication::~MainApplication()
{
  // Deinitialize GStreamer.
  gst_deinit();

  // Close log file.
  qInstallMessageHandler(nullptr); // restore default handler
  if (s_logFile) {
    s_logFile->close();
    delete s_logFile;
    s_logFile = nullptr;
  }
}

bool MainApplication::notify(QObject *receiver, QEvent *event)
{
  try
  {
    return QApplication::notify(receiver, event);
  }
  catch (std::exception &ex)
  {
    qDebug() << "std::exception was caught: " << ex.what() << Qt::endl;
    qDebug() << "event type: " << event->type() << Qt::endl;
  }

  return false;
}

}
