#include <QtCore/QtGlobal>

// On Windows, we build QOSC as part of the application (not as a separate DLL)
// so we don't need dllimport/dllexport decorations
#if defined(WIN32) || defined(_WIN32)
#  define QOSC_EXPORT
#elif defined(QOSC_LIBRARY)
#  define QOSC_EXPORT Q_DECL_EXPORT
#else
#  define QOSC_EXPORT Q_DECL_IMPORT
#endif
