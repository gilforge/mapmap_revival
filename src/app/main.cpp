// NOTE: To run, it is recommended not to be in Compiz or Beryl, they have shown some instability.

#include <iostream>
#include <QTranslator>
#include <QDebug>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QOpenGLContext>
#include <QFile>

#include "MM.h"
#include "MainWindow.h"
#include "MainApplication.h"

#include "MetaObjectRegistry.h"

#include <stdlib.h>
#include <iostream>

MM_USE_NAMESPACE

static void set_env_vars_if_needed()
{
#ifdef __MACOSX_CORE__
  std::cout << "OS X detected. Set environment for GStreamer support." << std::endl;
  if (0 == setenv("GST_PLUGIN_PATH", "/Library/Frameworks/GStreamer.framework/Libraries", 1))
      std::cout << " * GST_PLUGIN_PATH=/Library/Frameworks/GStreamer.framework/Libraries" << std::endl;
  if (0 == setenv("GST_DEBUG", "2", 1))
      std::cout << " * GST_DEBUG=2" << std::endl;
#endif // __MACOSX_CORE__
}

// This class is just used to provide sleep functionalities in the main() method.
class I : public QThread
{
public:
  static void sleep(unsigned long secs) {
    QThread::sleep(secs);
  }
  static void msleep(unsigned long msecs) {
    QThread::msleep(msecs);
  }
  static void usleep(unsigned long usecs) {
    QThread::usleep(usecs);
  }
};

void initRegistry()
{
  MetaObjectRegistry& registry = MetaObjectRegistry::instance();

  // Paints.
  registry.add<Video>();
  registry.add<Image>();
  registry.add<Color>();

  // Mappings.
  registry.add<TextureMapping>();
  registry.add<ColorMapping>();

  // Shapes.
  registry.add<Quad>();
  registry.add<Mesh>();
  registry.add<MM_PREPEND_NAMESPACE(Ellipse)>();
  registry.add<Triangle>();
}

// File logger for crash debugging
static FILE* g_logFile = nullptr;

// Intercept all logging message and display it in the console
void logMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
  // Write to file for crash debugging
  if (g_logFile) {
    fprintf(g_logFile, "%s\n", msg.toUtf8().constData());
    fflush(g_logFile);
  }
  ConsoleWindow::console()->printMessage(type, context, msg);
}

int main(int argc, char *argv[])
{
  set_env_vars_if_needed();

  // Initialize meta-object registry.
  initRegistry();

  // CRITICAL: share OpenGL resources (textures, buffers) across ALL
  // QOpenGLWidget contexts in the application — including those in
  // separate top-level windows like OutputGLWindow. Without this, Qt
  // only shares contexts within the same top-level window, so the
  // textureId allocated for a video paint in MainWindow's source/
  // destination canvases is invalid in the output window's context.
  // Result: the HDMI output binds an undefined texture and never
  // receives glTexImage2D updates (because bitsHaveChanged() is reset
  // by the first canvas to render), producing the "sometimes fluid,
  // sometimes stuttering" symptom on the projector.
  // MUST be set BEFORE QApplication is constructed.
  QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);

  MainApplication app(argc, argv);

  // Open crash log file (flushed on every write for crash debugging).
  // Rotate to crash_log.txt.prev first so that, if the next run crashes early
  // (e.g. flickering on second launch), the diagnostics from the broken run
  // are still available for inspection.
  QString logPath     = QCoreApplication::applicationDirPath() + "/crash_log.txt";
  QString prevLogPath = QCoreApplication::applicationDirPath() + "/crash_log.txt.prev";
  if (QFile::exists(logPath)) {
    QFile::remove(prevLogPath);
    QFile::rename(logPath, prevLogPath);
  }
  g_logFile = fopen(logPath.toUtf8().constData(), "w");

  // Install message handler
  // after QGuiApplication has been instanciated
  qInstallMessageHandler(logMessageHandler);

  QCommandLineParser parser;
  parser.setApplicationDescription("Video mapping editor");

  // --help option
  const QCommandLineOption helpOption = parser.addHelpOption();

  // --version option
  const QCommandLineOption versionOption = parser.addVersionOption();

  // --fullscreen option
  QCommandLineOption fullscreenOption(QStringList() << "F" << "fullscreen",
    "Display the output window and make it fullscreen.");
  parser.addOption(fullscreenOption);

  // --file option
  QCommandLineOption fileOption(QStringList() << "f" << "file",
    "Load project from <file>.", "file", "");
  parser.addOption(fileOption);

  // --reset-settings option
  QCommandLineOption resetSettingsOption(QStringList() << "R" << "reset-settings",
    "Reset MapMap settings, such as GUI properties.");
  parser.addOption(resetSettingsOption);

  // --osc-port option
  QCommandLineOption oscPortOption(QStringList() << "p" << "osc-port",
    "Use OSC port number <osc-port>.", "osc-port", "");
  parser.addOption(oscPortOption);

  // --lang option
  QCommandLineOption localeOption(QStringList() << "l" << "lang",
    "Use language <lang>.", "lang", "");
  parser.addOption(localeOption);

  // --frame-rate option
  QCommandLineOption frameRateOption(QStringList() << "r" << "frame-rate",
    "Use a framerate of <frame-rate> per second.", "frame-rate", QString::number(MM::DEFAULT_FRAMES_PER_SECOND));
  parser.addOption(frameRateOption);

  // Positional argument: file
  parser.addPositionalArgument("file", "Load project from that file.");

  parser.process(app);
  if (parser.isSet(versionOption) || parser.isSet(helpOption))
  {
    return 0;
  }
  if (parser.isSet(resetSettingsOption))
  {
    Util::eraseSettings();
  }

  // IMPORTANT: Translator must be set *before* the MainWindow is created for it to work.
  QSettings settings;
  // Get language from command line or user settings
  QString lang = parser.value("lang").isEmpty()
                 ? settings.value("language").toString()
                 : parser.value("lang");

  QTranslator qtTranslator;
  QTranslator appTranslator;
  if (MM::SUPPORTED_LANGUAGES.contains(lang))
  {
#ifdef Q_OS_WIN32
    qtTranslator.load(QString("qt_%1").arg(lang),
                      QApplication::applicationDirPath().append("/translations"));
#else
    qtTranslator.load(QString("qtbase_%1").arg(lang),
                      QLibraryInfo::path(QLibraryInfo::TranslationsPath));
#endif
    app.installTranslator(&qtTranslator);

    appTranslator.load(QString(":/translations_mapmap_%1").arg(lang));
    app.installTranslator(&appTranslator);
  }
  else {
    qWarning() << "Unrecognized/unsupported language: " << lang;
  }

  // Check for OpenGL support
  if (!QOpenGLContext::supportsThreadedOpenGL())
  {
    qWarning("Warning: Threaded OpenGL not supported on this system.");
  }

  // Splash screen removed (fork).

  // Create window.
  MainWindow* win = MainWindow::window();
  // Add custom font
  int id = QFontDatabase::addApplicationFont(":/base-font");
  QString family = QFontDatabase::applicationFontFamilies(id).at(0);
  app.setFont(QFont(family, 11, QFont::Normal));

  // Load stylesheet.
  QFile stylesheet(":/stylesheet");
  stylesheet.open(QFile::ReadOnly);
  app.setStyleSheet(QLatin1String(stylesheet.readAll()));

  // read positional argument:
  const QStringList args = parser.positionalArguments();
  QString projectFileValue = QString();

  // read the file option value: (overrides the positional argument)
  projectFileValue = parser.value("file");
  // read the first positional argument:
  if (! args.isEmpty())
  {
    projectFileValue = args.first();
  }

  // finally, load the project file.
  if (projectFileValue != "")
  {
    win->loadFile(projectFileValue);
  }

  QString oscPortValue = parser.value("osc-port");
  if (oscPortValue != "")
    win->setOscPort(oscPortValue);

  bool optionOk;
  qreal fps = parser.value("frame-rate").toDouble(&optionOk);
  if (optionOk)
    win->setFramesPerSecond(fps);
  else
    qFatal("Invalid option <frame-rate>.");

  // Splash screen removed (fork).

  // Launch program.
  win->show();

  if (parser.isSet(fullscreenOption))
  {
    win->startFullScreen();
  }

  // Start app.
  int result = app.exec();

  delete win;
  return result;
}
