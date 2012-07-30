/*
    qarv, a Qt interface to aravis.
    Copyright (C) 2012  Jure Varlec <jure.varlec@ad-vega.si>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#ifndef ARCAM_H
#define ARCAM_H

#include <QList>
#include <QString>
#include <QRect>
#include <QPair>
#include <QMetaType>
#include <QImage>

void arcamInit();


struct _ArvCamera;
typedef _ArvCamera ArvCamera;
struct _ArvDevice;
typedef _ArvDevice ArvDevice;
struct _ArvBuffer;
typedef _ArvBuffer ArvBuffer;
struct _ArvStream;
typedef _ArvStream ArvStream;

class ArCamId {
public:
  ArCamId();
  ArCamId(const char* id, const char* vendor, const char* model);
  ArCamId(const ArCamId& camid);
  ~ArCamId();
  const char *id, *vendor, *model;
};

Q_DECLARE_METATYPE(ArCamId)

class ArCam : public QObject {
  Q_OBJECT

public:
  ArCam(ArCamId id, QObject* parent = NULL);
  ~ArCam();
  
  static QList<ArCamId> listCameras();
  
  QRect getROI();
  void setROI(QRect roi);
  QRect getROIMaxSize();
  
  QSize getBinning();
  void setBinning(QSize bin);
  
  QList<QString> getPixelFormats();
  QList<QString> getPixelFormatNames();
  QString getPixelFormat();
  void setPixelFormat(QString format);
  
  double getFPS();
  void setFPS(double fps);

  int getMTU();
  void setMTU(int mtu);
  
  double getExposure();
  void setExposure(double exposure);
  QPair<double, double> getExposureLimits();
  bool hasAutoExposure();
  void setAutoExposure(bool enable);

  double getGain();
  void setGain(double gain);
  QPair<double, double> getGainLimits();
  bool hasAutoGain();
  void setAutoGain(bool enable);

  void startAcquisition();
  void stopAcquisition();

  QSize getFrameSize();
  QByteArray getFrame();

signals:
  void frameReady();

private:
  void swapBuffers();
  static QList<ArCamId> cameraList;
  ArvCamera* camera;
  ArvDevice* device;
  ArvStream* stream;
  ArvBuffer* currentFrame;
  bool acquiring;

  friend void streamCallback(ArvStream* stream, ArCam* cam);
};

#endif // ARCAM_H
