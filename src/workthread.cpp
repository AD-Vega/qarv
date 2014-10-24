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

#include "workthread.h"

#include "api/qarvdecoder.h"
#include "glhistogramwidget.h"
#include "filters/filter.h"
#include "recorders/recorder.h"
#include <QThread>

using namespace QArv;

Workthread::Workthread(QObject* parent) : QObject(parent) {
  auto cookerThread = new QThread(this);
  cooker1 = new Cooker;
  cooker1->moveToThread(cookerThread);
  connect(cookerThread, SIGNAL(finished()), cooker1, SLOT(deleteLater()));
  connect(cooker1, SIGNAL(done()), SLOT(cookerFinished()));

  cooker2 = new Cooker;
  cooker2->moveToThread(cookerThread);
  connect(cookerThread, SIGNAL(finished()), cooker2, SLOT(deleteLater()));
  connect(cooker2, SIGNAL(done()), SLOT(cookerFinished()));

  cookerThread->setObjectName("QArv Cooker");
  cookerThread->start();

  auto rendererThread = new QThread(this);
  renderer = new Renderer;
  renderer->moveToThread(rendererThread);
  connect(rendererThread, SIGNAL(finished()), renderer, SLOT(deleteLater()));
  connect(renderer, SIGNAL(done()), SLOT(rendererFinished()));

  rendererThread->setObjectName("QArv Renderer");
  rendererThread->start();
}

Workthread::~Workthread() {
  auto cookerThread = cooker1->thread();
  auto rendererThread = renderer->thread();
  cookerThread->quit();
  rendererThread->quit();
  cookerThread->wait();
  rendererThread->wait();
}

bool Workthread::isBusy() {
  return cooker1->busy || cooker2->busy
         || cooker1->scheduled || cooker2->scheduled
         || renderer->busy;
}

bool Workthread::cookFrame(QByteArray frame,
                           QArvDecoder* decoder,
                           bool imageTransform_invert,
                           int imageTransform_flip,
                           int imageTransform_rot,
                           QList< ImageFilter* > filterChain,
                           Recorder* recorder) {
  Cooker* cooker;
  bool shouldStart;
  if (!cooker1->busy && !cooker1->scheduled) {
    cooker = cooker1;
    shouldStart = !cooker2->busy;
  } else if (!cooker2->busy && !cooker2->scheduled) {
    cooker = cooker2;
    shouldStart = !cooker1->busy;
  } else {
    return false;
  }

  cooker->frame = frame;
  cooker->decoder = decoder;
  cooker->imageTransform_invert = imageTransform_invert;
  cooker->imageTransform_flip = imageTransform_flip;
  cooker->filterChain = filterChain;
  cooker->recorder = recorder;
  if (shouldStart) {
    cooker->busy = true;
    QMetaObject::invokeMethod(cooker, "start", Qt::QueuedConnection);
  } else {
    cooker->scheduled = true;
  }
  return true;
}

bool Workthread::renderFrame(const cv::Mat frame,
                             QImage* destinationImage,
                             bool markClipped,
                             Histograms* hists,
                             bool logarithmic) {
  if (renderer->busy)
    return false;
  renderer->frame = frame.clone();
  renderer->destinationImage = destinationImage;
  renderer->markClipped = markClipped;
  renderer->hists = hists;
  renderer->logarithmic = logarithmic;
  QMetaObject::invokeMethod(renderer, "start", Qt::QueuedConnection);
  return renderer->busy = true;
}

void Workthread::cookerFinished() {
  auto cooker = qobject_cast<Cooker*>(sender());
  cooker->busy = cooker->scheduled = false;
  if (cooker1->scheduled) {
    cooker1->busy = true;
    QMetaObject::invokeMethod(cooker1, "start", Qt::QueuedConnection);
  }
  if (cooker2->scheduled) {
    cooker2->busy = true;
    QMetaObject::invokeMethod(cooker2, "start", Qt::QueuedConnection);
  }
  emit frameCooked(cooker->processedFrame);
}

void Workthread::rendererFinished() {
  renderer->busy = false;
  emit frameRendered();
}

Cooker::Cooker(QObject* parent) : QObject(parent) {}

void Cooker::start() {
  decoder->decode(frame);
  cv::Mat img = decoder->getCvImage().clone();

  if (imageTransform_invert) {
    int bits = img.depth() == CV_8U ? 8 : 16;
    cv::subtract((1 << bits) - 1, img, img);
  }

  if (imageTransform_flip != -100)
    cv::flip(img, img, imageTransform_flip);

  switch (imageTransform_rot) {
  case 1:
    cv::transpose(img, img);
    cv::flip(img, img, 0);
    break;
  case 2:
    cv::flip(img, img, -1);
    break;
  case 3:
    cv::transpose(img, img);
    cv::flip(img, img, 1);
    break;
  }

  if (filterChain.isEmpty()) {
    processedFrame = img;
  } else {
    int imageType = img.type();
    for (auto filter : filterChain) {
      filter->filterImage(img);
    }
    img.convertTo(processedFrame, imageType);
  }

  if (recorder) {
    recorder->recordFrame(frame, processedFrame);
  }

  emit done();
}

template<bool grayscale, bool depth8>
void renderFrame(const cv::Mat frame, QImage* image_, bool markClipped = false,
                 Histograms* hists = NULL, bool logarithmic = false) {
  typedef typename ::std::conditional<depth8, uint8_t, uint16_t>::type ImageType;
  float * histRed, * histGreen, * histBlue;
  if (hists) {
    histRed = hists->red;
    histGreen = hists->green;
    histBlue = hists->blue;
    for (int i = 0; i < 256; i++) {
      histRed[i] = histGreen[i] = histBlue[i] = 0;
    }
  } else {
    histRed = histGreen = histBlue = NULL;
  }
  QImage& image = *image_;
  const int h = frame.rows, w = frame.cols;
  QSize s = image.size();
  if (s.height() != h
      || s.width() != w
      || image.format() != QImage::Format_ARGB32_Premultiplied)
    image = QImage(w, h, QImage::Format_ARGB32_Premultiplied);
  if (!grayscale) {
    float* histograms[3] = { histRed, histGreen, histBlue };
    for (int i = 0; i < h; i++) {
      auto imgLine = image.scanLine(i);
      auto imageLine = frame.ptr<cv::Vec<ImageType, 3> >(i);
      for (int j = 0; j < w; j++) {
        auto& bgr = imageLine[j];
        bool clipped = false;
        for (int px = 0; px < 3; px++) {
          uint8_t tmp = depth8 ? bgr[2-px] : bgr[2-px] >> 8;
          if (hists)
            histograms[px][tmp]++;
          clipped = clipped || tmp == 255;
          imgLine[4*j + 2 - px] = tmp;
        }
        imgLine[4*j + 3] = 255;
        if (clipped && markClipped) {
          imgLine[4*j + 0] = 255;
          imgLine[4*j + 1] = 0;
          imgLine[4*j + 2] = 200;
        }
      }
    }
    if (hists && logarithmic) {;
      for (int c = 0; c < 3; c++)
        for (int i = 0; i < 256; i++) {
          float* h = histograms[c] + i;
          *h = log2(*h + 1);
        }
    }
  } else {
    for (int i = 0; i < h; i++) {
      auto imgLine = image.scanLine(i);
      auto imageLine = frame.ptr<ImageType>(i);
      for (int j = 0; j < w; j++) {
        uint8_t gray = depth8 ? imageLine[j] : imageLine[j] >> 8;
        if (hists)
          histRed[gray]++;
        if (gray == 255 && markClipped) {
          imgLine[4*j + 0] = 255;
          imgLine[4*j + 1] = 0;
          imgLine[4*j + 2] = 200;
        } else {
          for (int px = 0; px < 3; px++) {
            imgLine[4*j + px] = gray;
          }
        }
        imgLine[4*j + 3] = 255;
      }
    }
    if (hists && logarithmic)
      for (int i = 0; i < 256; i++)
        histRed[i] = log2(histRed[i] + 1);
  }
}

Renderer::Renderer(QObject* parent) : QObject(parent) {}

void Renderer::start() {
  void (* theFunc)(const cv::Mat, QImage*, bool, QArv::Histograms*, bool);
  switch (frame.type()) {
  case CV_16UC1:
    theFunc = renderFrame<true, false>;
    break;
  case CV_16UC3:
    theFunc = renderFrame<false, false>;
    break;
  case CV_8UC1:
    theFunc = renderFrame<true, true>;
    break;
  case CV_8UC3:
    theFunc = renderFrame<false, true>;
    break;
  default:
    throw QString("Invalid image type!");
  }

  theFunc(frame, destinationImage, markClipped, hists, logarithmic);

  emit done();
}
