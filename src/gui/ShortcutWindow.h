/*
 * ShortcutWindow.h
 *
 * (c) 2020 Dame Diongue -- baydamd(@)gmail(.)com
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

#ifndef SHORTCUTWINDOW_H
#define SHORTCUTWINDOW_H

#ifdef NO_WEBENGINE
#include <QTextBrowser>
#else
#include <QWebEngineView>
#include <QWebEnginePage>
#include <QWebEngineSettings>
#endif

#include <QFile>
// QTextCodec removed in Qt6 - use QString::fromUtf8 instead
#include <QFontDatabase>

#include "MM.h"

namespace mmp {

#ifdef NO_WEBENGINE
class ShortcutWindow : public QTextBrowser
#else
class ShortcutWindow : public QWebEngineView
#endif
{
  Q_OBJECT
public:
  ShortcutWindow();
  ~ShortcutWindow() {}

private:

  // Constantes
  static const int SHORTCUT_WINDOW_WIDTH = 960;
  static const int SHORTCUT_WINDOW_HEIGHT = 640;

};

}

#endif // SHORTCUTWINDOW_H
