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
 */

#ifndef WORKTHREAD_H
#define WORKTHREAD_H

#include "filters/filter.h"
#include <QImage>
#include <QQueue>
#include <opencv2/core/core.hpp>

class QArvDecoder;

namespace QArv {

class Recorder;
class ImageFilter;
class Histograms;

class Cooker: public QObject {
  Q_OBJECT

  friend class Workthread;
  explicit Cooker(QObject* parent = 0);

private slots:
  void start();

signals:
  void done();

private:
  QByteArray frame;
  QArvDecoder* decoder;
  bool imageTransform_invert;
  int imageTransform_flip;
  int imageTransform_rot;
  QList<ImageFilterPtr> filterChain;
  Recorder* recorder;

  cv::Mat processedFrame;
  bool busy = false;
};

class Renderer: public QObject {
  Q_OBJECT

  friend class Workthread;
  explicit Renderer(QObject* parent = 0);

private slots:
  void start();

signals:
  void done();

private:
  cv::Mat frame;
  QImage* destinationImage;
  bool markClipped;
  Histograms* hists;
  bool logarithmic;

  bool busy = false;
};

class Workthread: public QObject {
  Q_OBJECT

public:
  explicit Workthread(QObject* parent = 0);
  ~Workthread();

  // These two return false if the corresponding threads are busy and
  // work cannot be started.
  bool cookFrame(int queueMax,
                 QByteArray rawFrame,
                 QArvDecoder* decoder,
                 bool imageTransform_invert,
                 int imageTransform_flip,
                 int imageTransform_rot,
                 QList<ImageFilterPtr> filterChain,
                 Recorder* recorder = NULL);
  bool renderFrame(const cv::Mat frame,
                   QImage* destinationImage,
                   bool markClipped = false,
                   Histograms* hists = NULL,
                   bool logarithmic = false);
  bool isBusy();

signals:
  void frameCooked(cv::Mat frame);
  void frameRendered();

private slots:
  void cookerFinished();
  void rendererFinished();

private:
  Cooker* cooker;
  Renderer* renderer;
  QQueue<QByteArray> queue;
};

};

#endif
