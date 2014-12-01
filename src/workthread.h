/*
    QArv, a Qt interface to aravis.
    Copyright (C) 2012-2014 Jure Varlec <jure.varlec@ad-vega.si>
                            Andrej Lajovic <andrej.lajovic@ad-vega.si>

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

/*
 * The worker threads serve only to keep the GUI responsive. There are
 * no resources that are "really" shared, e.g. the decoder is only ever used
 * in the Cooker thread so there is no chance of a race. These resources are
 * only freed in the main thread after the work is complete.
 *
 * Exceptions: the camera lives in the Cooker's thread in order to deliver
 * signals completely bypassing the GUI thread. Apart from its signals and
 * acqusition control, its functions should be safe to use from the GUI thread
 * because signals are the only thing it needs event loop for. Calls that need
 * to be threadsafe are implemented via Workthread and Cooker.
 */

#ifndef WORKTHREAD_H
#define WORKTHREAD_H

#include "filters/filter.h"
#include "api/qarvcamera.h"
#include <QImage>
#include <QFile>
#include <opencv2/core/core.hpp>
#include <functional>

class QArvDecoder;
class QThread;

namespace QArv {

class Recorder;
class ImageFilter;
class Histograms;

class Cooker: public QObject {
  Q_OBJECT

  friend class Workthread;
  explicit Cooker(QObject* parent = 0);

  struct Parameters {
    bool imageTransform_invert = false;
    int imageTransform_flip = 0;
    int imageTransform_rot = 0;
    QList<ImageFilterPtr> filterChain;
    QArvDecoder* decoder = nullptr;
    QFile* timestampFile = nullptr;
    Recorder* recorder = nullptr;
  };

private slots:
  void processEvents();

  void returnCamera(QArvCamera* camera, QThread* thread);

  void cameraAcquisition(QArvCamera* camera, bool start,
                         bool zeroCopy, bool dropInvalidFrames);

  void processFrame(QByteArray frame, ArvBuffer* aravisFrame);

  void setImageTransform(bool imageTransform_invert,
                         int imageTransform_flip,
                         int imageTransform_rot);

  void setFilterChain(QList<ImageFilterPtr> filterChain);

  void setRecorder(Recorder* recorder, QFile* timestampFile);

signals:
  void frameCooked(cv::Mat frame);
  void frameToRender(cv::Mat frame);

private:
  Parameters p;
  cv::Mat processedFrame;
  std::atomic_bool doRender;
};

class Renderer: public QObject {
  Q_OBJECT

  friend class Workthread;
  explicit Renderer(QObject* parent = 0);

private slots:
  void renderFrame(cv::Mat frame);
  void processEvents();

signals:
  void frameRendered();

private:
  QImage* destinationImage;
  bool markClipped;
  Histograms* hists;
  bool logarithmic;
};

class Workthread: public QObject {
  Q_OBJECT

public:
  explicit Workthread(QObject* parent = 0);
  ~Workthread();

  // Replaces the old camera with the new one. After that, the old
  // camera can be deleted. The new camera can be NULL, and so can the
  // decoder.
  void newCamera(QArvCamera* camera, QArvDecoder* decoder);

  // Can be NULL.
  void newRecorder(Recorder* recorder, QFile* timestampFile);

  void startRecording();
  void stopRecording();

  void startCamera(bool zeroCopy, bool dropInvalidFrames);
  void stopCamera();

  void setImageTransform(bool imageTransform_invert,
                         int imageTransform_flip,
                         int imageTransform_rot);

  void setFilterChain(QList<ImageFilterPtr> filterChain);

  void renderFrame(QImage* destinationImage,
                   bool markClipped = false,
                   Histograms* hists = NULL,
                   bool logarithmic = false);

  void waitUntilProcessingCycleCompletes();

signals:
  void frameDelivered(QByteArray frame, ArvBuffer* arvFrame);
  void frameCooked(cv::Mat frame);
  void frameRendered();

private:
  QArvCamera* camera = nullptr;
  Recorder* recorder = nullptr;
  QFile* timestampFile = nullptr;
  Cooker* cooker;
  Renderer* renderer;
};

};

#endif
