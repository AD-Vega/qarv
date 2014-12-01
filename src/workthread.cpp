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
#include "glhistogramwidget.h"
#include "filters/filter.h"
#include "recorders/recorder.h"
#include <QThread>
#include <QCoreApplication>

extern "C" {
#include <arvbuffer.h>
}

using namespace QArv;

static int init = [] () {
  qRegisterMetaType<cv::Mat>("cv::Mat");
  qRegisterMetaType<QFile*>("QFile*");
  qRegisterMetaType<QList<ImageFilterPtr>>("QList<ImageFilterPtr>");
  return 0;
}();

Workthread::Workthread(QObject* parent) : QObject(parent) {
  camera = nullptr;
  auto cookerThread = new QThread(this);
  cooker = new Cooker;
  cooker->moveToThread(cookerThread);
  connect(cookerThread, SIGNAL(finished()), cooker, SLOT(deleteLater()));
  connect(cooker, SIGNAL(frameCooked(cv::Mat)), SIGNAL(frameCooked(cv::Mat)));
  connect(cooker, SIGNAL(recordingStopped()), SIGNAL(recordingStopped()));

  cookerThread->setObjectName("QArv Cooker");
  cookerThread->start();

  auto rendererThread = new QThread(this);
  renderer = new Renderer;
  renderer->moveToThread(rendererThread);
  connect(rendererThread, SIGNAL(finished()), renderer, SLOT(deleteLater()));
  connect(renderer, SIGNAL(frameRendered()), SIGNAL(frameRendered()));

  rendererThread->setObjectName("QArv Renderer");
  rendererThread->start();

  connect(cooker, SIGNAL(frameToRender(cv::Mat)),
          renderer, SLOT(renderFrame(cv::Mat)));
}

Workthread::~Workthread() {
  newCamera(nullptr, nullptr);
  auto cookerThread = cooker->thread();
  auto rendererThread = renderer->thread();
  cookerThread->quit();
  rendererThread->quit();
  cookerThread->wait();
  rendererThread->wait();
}

void Workthread::newCamera(QArvCamera* camera_, QArvDecoder* decoder) {
  if (camera) {
    disconnect(camera, SIGNAL(frameReady(QByteArray, ArvBuffer*)),
               cooker, SLOT(processFrame(QByteArray, ArvBuffer*)));
    disconnect(camera, SIGNAL(frameReady(QByteArray, ArvBuffer*)),
               this, SIGNAL(frameDelivered(QByteArray,ArvBuffer*)));
    QMetaObject::invokeMethod(cooker, "returnCamera", Qt::BlockingQueuedConnection,
                              Q_ARG(QArvCamera*, camera),
                              Q_ARG(QThread*, thread()));
  }
  cooker->p.decoder = decoder;
  camera = camera_;
  if (camera) {
    camera->moveToThread(cooker->thread());
    connect(camera, SIGNAL(frameReady(QByteArray, ArvBuffer*)),
            cooker, SLOT(processFrame(QByteArray, ArvBuffer*)));
    connect(camera, SIGNAL(frameReady(QByteArray, ArvBuffer*)),
            this, SIGNAL(frameDelivered(QByteArray,ArvBuffer*)));  }
}

void Workthread::newRecorder(Recorder* recorder_, QFile* timestampFile_) {
  recorder = recorder_;
  timestampFile = timestampFile_;
}

void Workthread::startRecording(int maxFrames) {
  QMetaObject::invokeMethod(cooker, "setRecorder", Qt::QueuedConnection,
                            Q_ARG(Recorder*, recorder),
                            Q_ARG(QFile*, timestampFile),
                            Q_ARG(int, maxFrames));
}

void Workthread::stopRecording() {
  QMetaObject::invokeMethod(cooker, "setRecorder", Qt::QueuedConnection,
                            Q_ARG(Recorder*, nullptr),
                            Q_ARG(QFile*, nullptr),
                            Q_ARG(int, -1));
}

void Workthread::startCamera(bool zeroCopy, bool dropInvalidFrames) {
  QMetaObject::invokeMethod(cooker, "cameraAcquisition",
                            Qt::BlockingQueuedConnection,
                            Q_ARG(QArvCamera*, camera), Q_ARG(bool, true),
                            Q_ARG(bool, zeroCopy),
                            Q_ARG(bool, dropInvalidFrames));
}

void Workthread::stopCamera() {
  QMetaObject::invokeMethod(cooker, "cameraAcquisition",
                            Qt::BlockingQueuedConnection,
                            Q_ARG(QArvCamera*, camera), Q_ARG(bool, false),
                            Q_ARG(bool, true), Q_ARG(bool, true));
}

void Workthread::setImageTransform(bool imageTransform_invert,
                                   int imageTransform_flip,
                                   int imageTransform_rot) {
  QMetaObject::invokeMethod(cooker, "setImageTransform", Qt::QueuedConnection,
                            Q_ARG(bool, imageTransform_invert),
                            Q_ARG(int, imageTransform_flip),
                            Q_ARG(int, imageTransform_rot));
}

void Workthread::setFilterChain(QList<ImageFilterPtr> filterChain) {
  QMetaObject::invokeMethod(cooker, "setFilterChain", Qt::QueuedConnection,
                            Q_ARG(QList<ImageFilterPtr>, filterChain));
}

void Workthread::waitUntilProcessingCycleCompletes() {
  QMetaObject::invokeMethod(renderer, "processEvents", Qt::BlockingQueuedConnection);
  QMetaObject::invokeMethod(cooker, "processEvents", Qt::BlockingQueuedConnection);
}

void Workthread::renderFrame(QImage* destinationImage,
                             bool markClipped,
                             Histograms* hists,
                             bool logarithmic) {
  renderer->destinationImage = destinationImage;
  renderer->markClipped = markClipped;
  renderer->hists = hists;
  renderer->logarithmic = logarithmic;
  cooker->doRender.store(true);
}

Cooker::Cooker(QObject* parent) : QObject(parent) {
  doRender.store(false);
}

void Cooker::processEvents() {
  QCoreApplication::processEvents();
}

void Cooker::returnCamera(QArvCamera* camera, QThread* thread) {
  camera->moveToThread(thread);
}

void Cooker::cameraAcquisition(QArvCamera* camera,
                               bool start,
                               bool zeroCopy,
                               bool dropInvalidFrames) {
  if (start)
    camera->startAcquisition(zeroCopy, dropInvalidFrames);
  else
    camera->stopAcquisition();
}

void Cooker::processFrame(QByteArray frame, ArvBuffer* aravisFrame) {
  if (p.decoder) {
    p.decoder->decode(frame);
    cv::Mat img = p.decoder->getCvImage();

    if (p.imageTransform_invert) {
      int bits = img.depth() == CV_8U ? 8 : 16;
      cv::subtract((1 << bits) - 1, img, img);
    }

    if (p.imageTransform_flip != -100)
      cv::flip(img, img, p.imageTransform_flip);

    switch (p.imageTransform_rot) {
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

    bool needFiltering = false;
    for (auto filter : p.filterChain) {
      if (filter->isEnabled()) {
        needFiltering = true;
        break;
      }
    }
    if (!needFiltering) {
      processedFrame = img;
    } else {
      img = img.clone();
      int imageType = img.type();
      for (auto filter : p.filterChain) {
        if (filter->isEnabled())
          filter->filterImage(img);
      }
      img.convertTo(processedFrame, imageType);
    }
  }

  if (p.recorder && p.recorder->isOK()) {
    if (maxRecordedFrames > 0) {
      if (recordedFrames < maxRecordedFrames) {
        recordedFrames++;
        if (p.recorder->recordsRaw())
          p.recorder->recordFrame(frame);
        else
          p.recorder->recordFrame(processedFrame);
        if (p.timestampFile && p.timestampFile->isOpen()) {
          quint64 ts;
#ifdef ARAVIS_OLD_BUFFER
          ts = aravisFrame->timestamp_ns;
#else
          ts = arv_buffer_get_timestamp(aravisFrame);
#endif
          p.timestampFile->write(QString::number(ts).toAscii());
          p.timestampFile->write("\n");
        }
      } else {
        emit recordingStopped();
      }
    }
  }

  emit frameCooked(processedFrame.clone());
  if (doRender.load()) {
    doRender.store(false);
    emit frameToRender(processedFrame.clone());
  }
}

void Cooker::setImageTransform(bool imageTransform_invert,
                               int imageTransform_flip,
                               int imageTransform_rot) {
  p.imageTransform_invert = imageTransform_invert;
  p.imageTransform_flip = imageTransform_flip;
  p.imageTransform_rot = imageTransform_rot;
}

void Cooker::setFilterChain(QList<ImageFilterPtr> filterChain) {
  p.filterChain = filterChain;
}

void Cooker::setRecorder(Recorder* recorder, QFile* timestampFile,
                         int maxFrames) {
  p.recorder = recorder;
  p.timestampFile = timestampFile;
  if (maxFrames != -1) {
    maxRecordedFrames = maxFrames;
    recordedFrames = 0;
  }
}

template<bool grayscale, bool depth8>
static void renderFrameF(const cv::Mat frame, QImage* image_, bool markClipped = false,
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

void Renderer::processEvents() {
  QCoreApplication::processEvents();
}

void Renderer::renderFrame(cv::Mat frame) {
  void (* theFunc)(const cv::Mat, QImage*, bool, QArv::Histograms*, bool);
  switch (frame.type()) {
  case CV_16UC1:
    theFunc = renderFrameF<true, false>;
    break;
  case CV_16UC3:
    theFunc = renderFrameF<false, false>;
    break;
  case CV_8UC1:
    theFunc = renderFrameF<true, true>;
    break;
  case CV_8UC3:
    theFunc = renderFrameF<false, true>;
    break;
  default:
    throw QString("Invalid image type!");
  }

  theFunc(frame, destinationImage, markClipped, hists, logarithmic);

  emit frameRendered();
}
