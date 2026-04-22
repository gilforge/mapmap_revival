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

namespace mmp {

MainApplication::MainApplication(int &argc, char *argv[])
  : QApplication(argc, argv)
{
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
#endif

  // Initialize GStreamer.
  gst_init (NULL, NULL);

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
