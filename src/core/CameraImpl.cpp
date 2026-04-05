/*
 * CameraImpl.cpp
 *
 * (c) 2019 Dame Diongue -- baydamd(@)gmail(.)com
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

#include "CameraImpl.h"
#include <QMessageBox>

namespace mmp {

CameraImpl::CameraImpl() :
  _camera(nullptr),
  _captureSession(nullptr),
  _cameraSurface(nullptr)
{

}

CameraImpl::~CameraImpl()
{
  if (_camera) {
    _camera->stop();
    delete _camera;
  }
  delete _captureSession;
  delete _cameraSurface;
}

bool CameraImpl::loadMovie(const QString &deviceName)
{
  VideoImpl::loadMovie(deviceName);

  // Find the camera device by id
  QCameraDevice selectedDevice;
  for (const QCameraDevice &dev : QMediaDevices::videoInputs()) {
    if (QString::fromUtf8(dev.id()) == deviceName) {
      selectedDevice = dev;
      break;
    }
  }

  if (selectedDevice.isNull()) {
    // Fallback to default camera
    selectedDevice = QMediaDevices::defaultVideoInput();
  }

  _camera = new QCamera(selectedDevice);
  _cameraSurface = new CameraSurface();
  _captureSession = new QMediaCaptureSession();

  _captureSession->setCamera(_camera);
  _captureSession->setVideoSink(_cameraSurface->videoSink());

  _camera->start();

  if (_camera->isActive())
    return true;

  if (_camera->error() != QCamera::NoError)
    QMessageBox(QMessageBox::Critical, "Camera Error",
                "Failed to start: " + _camera->errorString()).exec();

  return false;
}

int CameraImpl::getWidth() const
{
  // In Qt6, we get dimensions from the camera format
  if (_camera) {
    QCameraFormat fmt = _camera->cameraFormat();
    if (!fmt.isNull())
      return fmt.resolution().width();
  }
  return 0;
}

int CameraImpl::getHeight() const
{
  if (_camera) {
    QCameraFormat fmt = _camera->cameraFormat();
    if (!fmt.isNull())
      return fmt.resolution().height();
  }
  return 0;
}

const uchar *CameraImpl::getBits()
{
  return _cameraSurface->bits();
}

}
