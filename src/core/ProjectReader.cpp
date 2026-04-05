/*
 * ProjectReader.cpp
 *
 * (c) 2013 Sofian Audry -- info(@)sofianaudry(.)com
 * (c) 2013 Alexandre Quessy -- alexandre(@)quessy(.)net
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
#include "ProjectReader.h"
#include <sstream>
#include <iostream>
#include <string>

namespace mmp {

ProjectReader::ProjectReader(MainWindow *window) : _window(window)
{
}

bool ProjectReader::isValidVersion(const QString& versionString)
{
    QRegularExpression re(MM::SUPPORTED_FILE_VERSIONS);
    QRegularExpressionMatch match = re.match(versionString);
    return match.hasMatch();
}

bool ProjectReader::readFile(QIODevice *device)
{
  QString errorStr;
  int errorLine;
  int errorColumn;

  QDomDocument doc;
  if (!doc.setContent(device, false, &errorStr, &errorLine, &errorColumn)) {
    std::cerr << "Error: Parse error at line " << errorLine << ", "
              << "column " << errorColumn << ": "
              << qPrintable(errorStr) << std::endl;
    return false;
  }

  QDomElement root = doc.documentElement();
  QString projectVersion = root.attribute("version");
  // The handling of the version number will get fancier as we go.
  if (root.tagName() != "project") {
    _xml.raiseError(QObject::tr("The contents of this file does not look like a MapMap project."));
    return false;
  } else if (! this->isValidVersion(projectVersion)) {
    _xml.raiseError(
        QObject::tr("The version of MapMap %1 used to save this file is not readable by this MapMap version %2.").arg(
            projectVersion, MM::VERSION));
    return false;
  }

  parseProject(root);

  return (! _xml.hasError() );
}

QString ProjectReader::errorString() const
{
  return QObject::tr("%1\nLine %2, column %3")
    .arg(_xml.errorString())
    .arg(_xml.lineNumber())
    .arg(_xml.columnNumber());
}


void ProjectReader::parseProject(const QDomElement& project)
{
  qDebug() << "[ProjectReader] === parseProject START ===";
  // TODO: this is dangerous if we have
  MappingManager& manager = _window->getMappingManager();
  manager.clearAll();

  QDomElement paints = project.firstChildElement(ProjectLabels::PAINTS);
  QDomElement mappings = project.firstChildElement(ProjectLabels::MAPPINGS);

  // Parse paints.
  int paintIndex = 0;
  QDomNode paintNode = paints.firstChild();
  while (!paintNode.isNull())
  {
    qDebug() << "[ProjectReader] Parsing paint #" << paintIndex;
    Paint::ptr paint = parsePaint(paintNode.toElement());

    if (paint.isNull())
    {
      qDebug() << "[ProjectReader] Problem creating paint #" << paintIndex << Qt::endl;
    }
    else
    {
      qDebug() << "[ProjectReader] Paint created OK, adding to manager...";
      manager.addPaint(paint);
      qDebug() << "[ProjectReader] addPaint OK, adding paint item to window...";
      _window->addPaintItem(paint->getId(), paint->getIcon(), paint->getName());
      qDebug() << "[ProjectReader] addPaintItem OK";

      // Locate media file if not found
      if (paint->getSourceType() == Paint::SourceType::Video)
      {
        QSharedPointer<Video> media = qSharedPointerCast<Video>(paint);
        Q_CHECK_PTR(media);
        qDebug() << "[ProjectReader] Video URI: " << media->getUri();
        if (!_window->fileExists(media->getUri()))
        {
          qDebug() << "[ProjectReader] File not found, locating...";
          media->setUri(_window->locateMediaFile(media->getUri(), false));
        }
      }
      if (paint->getSourceType() == Paint::SourceType::Image)
      {
        QSharedPointer<Image> image = qSharedPointerCast<Image>(paint);
        Q_CHECK_PTR(image);
        qDebug() << "[ProjectReader] Image URI: " << image->getUri();
        if (!_window->fileExists(image->getUri()))
        {
          qDebug() << "[ProjectReader] File not found, locating...";
          image->setUri(_window->locateMediaFile(image->getUri(), true));
        }
      }
    }
    paintIndex++;
    paintNode = paintNode.nextSibling();
  }
  qDebug() << "[ProjectReader] All paints parsed (" << paintIndex << " total)";

  // Parse mappings.
  int mappingIndex = 0;
  QDomNode mappingNode = mappings.firstChild();
  QVector<Mapping::ptr> allMappings;
  while (!mappingNode.isNull())
  {
    qDebug() << "[ProjectReader] Parsing mapping #" << mappingIndex;
    Mapping::ptr mapping = parseMapping(mappingNode.toElement());
    if (mapping.isNull())
    {
      qDebug() << "[ProjectReader] Problem creating mapping #" << mappingIndex << Qt::endl;
    }
    else
    {
      allMappings.push_back(mapping);
    }

    mappingIndex++;
    mappingNode = mappingNode.nextSibling();
  }
  qDebug() << "[ProjectReader] All mappings parsed (" << mappingIndex << " total)";

  // Add all mappings in reverse order.
  int addIndex = 0;
  for (QVector<Mapping::ptr>::const_reverse_iterator it = allMappings.rbegin();
          it != allMappings.rend(); ++it)
  {
    qDebug() << "[ProjectReader] Adding mapping #" << addIndex << " to manager...";
    manager.addMapping(*it);
    qDebug() << "[ProjectReader] addMapping OK, adding mapping item to window...";
    _window->addMappingItem((*it)->getId());
    qDebug() << "[ProjectReader] addMappingItem OK";
    addIndex++;
  }
  qDebug() << "[ProjectReader] === parseProject DONE ===";
}

Paint::ptr ProjectReader::parsePaint(const QDomElement& paintElem)
{
  QString className = Serializable::classNameCleanToReal(paintElem.attribute(ProjectLabels::CLASS_NAME));
  int id            = paintElem.attribute(ProjectLabels::ID, QString::number(NULL_UID)).toInt();

  qDebug() << "[parsePaint] className:" << className << " id:" << id;

  const QMetaObject* metaObject = MetaObjectRegistry::instance().getMetaObject(className);
  if (metaObject)
  {
    qDebug() << "[parsePaint] metaObject found, calling newInstance...";
    // Create new instance.
    Paint::ptr paint (qobject_cast<Paint*>(metaObject->newInstance( Q_ARG(int, id)) ));

    if (paint.isNull())
    {
      qDebug() << "[parsePaint] FAILED: newInstance returned null for" << className;
      return Paint::ptr();
    }
    else
      qDebug() << "[parsePaint] newInstance OK, id:" << paint->getId();

    qDebug() << "[parsePaint] calling paint->read()...";
    paint->read(paintElem);
    qDebug() << "[parsePaint] paint->read() done";

    return paint;
  }

  else
  {
    qDebug() << "[parsePaint] FAILED: no metaObject for" << className;
    _xml.raiseError(QObject::tr("Unable to create paint of type '%1'.").arg(className));
    return Paint::ptr();
  }
}

Mapping::ptr ProjectReader::parseMapping(const QDomElement& mappingElem)
{
  // Get attributes.
  QString className = Serializable::classNameCleanToReal(mappingElem.attribute(ProjectLabels::CLASS_NAME));
  int id            = mappingElem.attribute(ProjectLabels::ID, QString::number(NULL_UID)).toInt();

  qDebug() << "[parseMapping] className:" << className << " id:" << id;

  const QMetaObject* metaObject = MetaObjectRegistry::instance().getMetaObject(className);
  if (metaObject)
  {
    qDebug() << "[parseMapping] metaObject found, calling newInstance...";
    // Create new instance.
    Mapping::ptr mapping (qobject_cast<Mapping*>(metaObject->newInstance( Q_ARG(int, id)) ));
    if (mapping.isNull())
    {
      qDebug() << "[parseMapping] FAILED: newInstance returned null for" << className;
      return Mapping::ptr();
    }
    qDebug() << "[parseMapping] newInstance OK, calling mapping->read()...";
    mapping->read(mappingElem);
    qDebug() << "[parseMapping] mapping->read() done";

    return mapping;
  }

  else
  {
    qDebug() << "[parseMapping] FAILED: no metaObject for" << className;
    _xml.raiseError(QObject::tr("Unable to create paint of type '%1'.").arg(className));
    return Mapping::ptr();
  }
}

}
