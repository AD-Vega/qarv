/*
    qarv, a Qt interface to aravis.
    Copyright (C) 2012, 2013 Jure Varlec <jure.varlec@ad-vega.si>
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

#include "qarvmainwindow.h"

#include <QDebug>
#include <QNetworkInterface>
#include <QFileInfo>
#include <QFileDialog>
#include <QMessageBox>
#include <QSettings>
#include <qdatetime.h>
#include <QProcess>
#include <QTextDocument>
#include <QStatusBar>
#include <QtConcurrentRun>
#include <QPluginLoader>

#include <type_traits>

#include "api/qarvcameradelegate.h"
#include "getmtu_linux.h"
#include "globals.h"

extern "C" {
  #include <arvbuffer.h>
}

using namespace QArv;

const int slidersteps = 1000;

QArvMainWindow::QArvMainWindow(QWidget* parent, bool standalone_) :
  QMainWindow(parent), camera(NULL), decoder(NULL), playing(false),
  recording(false), started(false), drawHistogram(false),
  standalone(standalone_), imageTransform(),
  imageTransform_flip(0), imageTransform_rot(0), framecounter(0),
  toDisableWhenPlaying(), toDisableWhenRecording(), futureHoldsAHistogram(false) {

  setAttribute(Qt::WA_DeleteOnClose);

  qWarning() << "Please ignore \"Could not resolve property\" warnings "
                "unless icons look bad.";
  setupUi(this);
  on_statusTimeoutSpinbox_valueChanged(statusTimeoutSpinbox->value());

  // Setup theme icons if available.
  QMap<QAbstractButton*, QString> icons;
  icons[showVideoButton] = "video-display";
  icons[unzoomButton] = "zoom-original";
  icons[recordButton] = "media-record";
  icons[playButton] = "media-playback-start";
  icons[refreshCamerasButton] = "view-refresh";
  icons[chooseFilenameButton] = "document-open";
  icons[chooseSnappathButton] = "document-open";
  icons[editGainButton] = "edit-clear-locationbar-rtl";
  icons[editExposureButton] = "edit-clear-locationbar-rtl";
  icons[showHistogramButton] = "office-chart-bar";
  icons[histogramLog] = "view-object-histogram-logarithmic";
  icons[pickROIButton] = "edit-select";
  icons[closeFileButton] = "media-playback-stop";
  for (auto i = icons.begin(); i != icons.end(); i++)
    if (!QIcon::hasThemeIcon(*i))
      i.key()->setIcon(QIcon(QString(qarv_datafiles) + *i + ".svgz"));
  playIcon = playButton->icon();
  recordIcon = recordButton->icon();
  if (QIcon::hasThemeIcon("media-playback-pause"))
    pauseIcon = QIcon::fromTheme("media-playback-pause");
  else
    pauseIcon = QIcon(QString(
                        qarv_datafiles) + "media-playback-pause" + ".svgz");

  autoreadexposure = new QTimer(this);
  autoreadexposure->setInterval(sliderUpdateSpinbox->value());
  this->connect(autoreadexposure, SIGNAL(timeout()), SLOT(readExposure()));
  this->connect(autoreadexposure, SIGNAL(timeout()), SLOT(readGain()));
  this->connect(autoreadexposure, SIGNAL(timeout()),
                SLOT(updateBandwidthEstimation()));

  autoreadhistogram = new QTimer(this);
  autoreadhistogram->setInterval(histogramUpdateSpinbox->value());
  this->connect(autoreadhistogram, SIGNAL(timeout()),
                SLOT(histogramNextFrame()));
  autoreadhistogram->start();

  video->connect(pickROIButton, SIGNAL(toggled(bool)),
                 SLOT(enableSelection(bool)));
  this->connect(video, SIGNAL(selectionComplete(QRect)),
                SLOT(pickedROI(QRect)));

  rotationSelector->addItem(tr("No rotation"), 0);
  rotationSelector->addItem(tr("90 degrees"), 90);
  rotationSelector->addItem(tr("180 degrees"), 180);
  rotationSelector->addItem(tr("270 degrees"), 270);
  this->connect(rotationSelector,
                SIGNAL(currentIndexChanged(int)),
                SLOT(updateImageTransform()));
  this->connect(invertColors, SIGNAL(stateChanged(int)),
                SLOT(updateImageTransform()));
  this->connect(flipHorizontal, SIGNAL(stateChanged(int)),
                SLOT(updateImageTransform()));
  this->connect(flipVertical, SIGNAL(stateChanged(int)),
                SLOT(updateImageTransform()));
  connect(&futureRender, SIGNAL(finished()), SLOT(frameRendered()));

  if (!standalone) {
    setWindowFlags(windowFlags() & ~Qt::WindowCloseButtonHint);
    setAttribute(Qt::WA_DeleteOnClose);
    setAttribute(Qt::WA_QuitOnClose, false);
    tabWidget->removeTab(tabWidget->indexOf(recordingTab));
    snapButton->setEnabled(false);
  }

  auto plugins = QPluginLoader::staticInstances();
  foreach (auto plugin, plugins) {
    auto fmt = qobject_cast<OutputFormat*>(plugin);
    if (fmt != NULL)
      videoFormatSelector->addItem(fmt->name(), QVariant::fromValue(fmt));
  }
  videoFormatSelector->setCurrentIndex(0);

  auto timer = new QTimer(this);
  timer->setInterval(1000);
  this->connect(timer, SIGNAL(timeout()), SLOT(showFPS()));
  timer->start();

  QTimer::singleShot(300, this, SLOT(on_refreshCamerasButton_clicked()));

  setupListOfSavedWidgets();
  restoreProgramSettings();

  updateImageTransform();

  statusBar()->showMessage(tr("Welcome to qarv!"));
}

QArvMainWindow::~QArvMainWindow() {
  on_closeFileButton_clicked(true);
  saveProgramSettings();
}

void QArvMainWindow::on_refreshCamerasButton_clicked(bool clicked) {
  cameraSelector->blockSignals(true);
  cameraSelector->clear();
  cameraSelector->setEnabled(false);
  cameraSelector->addItem(tr("Looking for cameras..."));
  QApplication::processEvents();
  QApplication::flush();
  cameraSelector->clear();
  auto cameras = QArvCamera::listCameras();
  foreach (auto cam, cameras) {
    QString display;
    display = display + cam.vendor + " (" + cam.model + ")";
    cameraSelector->addItem(display, QVariant::fromValue<QArvCameraId>(cam));
  }
  cameraSelector->setCurrentIndex(-1);
  cameraSelector->setEnabled(true);
  cameraSelector->blockSignals(false);
  QString message = tr(" Found %n cameras.",
                       "Number of cameras",
                       cameraSelector->count());
  statusBar()->showMessage(statusBar()->currentMessage() + message,
                           statusTimeoutMsec);
  QSettings settings;
  QVariant data = settings.value("qarv_camera/selected");
  int previous_cam;
  if (data.isValid() && (previous_cam = cameraSelector->findText(data.toString())) != -1)
    cameraSelector->setCurrentIndex(previous_cam);
  else
    cameraSelector->setCurrentIndex(0);
}

void QArvMainWindow::on_unzoomButton_toggled(bool checked) {
  if (checked) {
    oldstate = saveState();
    oldgeometry = saveGeometry();
    oldsize = video->size();
    QSize newsize = video->getImageSize();
    video->setFixedSize(newsize);
  } else {
    video->setFixedSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
    restoreState(oldstate);
    restoreGeometry(oldgeometry);
    video->setFixedSize(oldsize);
    videodock->resize(1, 1);
    QApplication::processEvents();
    video->setFixedSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
    video->setMinimumSize(QSize(64, 64));
    QApplication::processEvents();
    QApplication::flush();
    on_videodock_topLevelChanged(videodock->isFloating());
  }
}

static inline double slider2value_log(int slidervalue,
                                      QPair<double, double>& range) {
  double value = log2(range.second) - log2(range.first);
  return exp2(value * slidervalue / slidersteps + log2(range.first));
}

static inline int value2slider_log(double value,
                                   QPair<double, double>& range) {
  return slidersteps
         * (log2(value) - log2(range.first))
         / (log2(range.second) - log2(range.first));
}

static inline double slider2value(int slidervalue,
                                  QPair<double, double>& range) {
  return range.first + (range.second - range.first)
         * slidervalue / slidersteps;
}

static inline int value2slider(double value,
                               QPair<double, double>& range) {
  return (value - range.first) / (range.second - range.first) * slidersteps;
}

void QArvMainWindow::readROILimits() {
  auto roisize = camera->getROIMaxSize();
  roirange = QRect(QPoint(1, 1), roisize.size());
  xSpinbox->setRange(1, roisize.width());
  ySpinbox->setRange(1, roisize.height());
  wSpinbox->setRange(roisize.x(), roisize.width());
  hSpinbox->setRange(roisize.y(), roisize.height());
}

void QArvMainWindow::readAllValues() {
  fpsSpinbox->setValue(camera->getFPS());

  auto formats = camera->getPixelFormats();
  auto formatnames = camera->getPixelFormatNames();
  int noofframes = formats.length();
  pixelFormatSelector->blockSignals(true);
  pixelFormatSelector->clear();
  for (int i = 0; i < noofframes; i++)
    pixelFormatSelector->addItem(formatnames.at(i), formats.at(i));
  auto format = camera->getPixelFormat();
  pixelFormatSelector->setCurrentIndex(pixelFormatSelector->findData(format));
  pixelFormatSelector->setEnabled(noofframes > 1);
  pixelFormatSelector->blockSignals(false);

  QSize binsize = camera->getBinning();
  binSpinBox->setValue(binsize.width());

  gainrange = camera->getGainLimits();
  exposurerange = camera->getExposureLimits();
  gainSlider->setRange(0, slidersteps);
  exposureSlider->setRange(0, slidersteps);
  gainSpinbox->setRange(gainrange.first, gainrange.second);
  exposureSpinbox->setRange(exposurerange.first/1000.,
                            exposurerange.second/1000.);
  readGain();
  readExposure();
  gainAutoButton->setEnabled(camera->hasAutoGain());
  exposureAutoButton->setEnabled(camera->hasAutoExposure());

  readROILimits();
  QRect roi = camera->getROI();
  roi.setWidth((roi.width() / 2) * 2);
  roi.setHeight((roi.height() / 2) * 2);
  xSpinbox->setValue(roi.x());
  ySpinbox->setValue(roi.y());
  wSpinbox->setValue(roi.width());
  hSpinbox->setValue(roi.height());
}

void QArvMainWindow::on_cameraSelector_currentIndexChanged(int index) {
  autoreadexposure->stop();

  QSettings settings;
  settings.setValue("qarv_camera/selected", cameraSelector->currentText());

  auto camid = cameraSelector->itemData(index).value<QArvCameraId>();
  if (camera != NULL) {
    startVideo(false);
    delete camera;
  }
  camera = new QArvCamera(camid, this);
  this->connect(camera, SIGNAL(frameReady()), SLOT(takeNextFrame()));

  qDebug() << "Pixel formats:" << camera->getPixelFormats();

  auto ifaceIP = camera->getHostIP();
  QNetworkInterface cameraIface;
  if (!ifaceIP.isNull()) {
    auto ifaces = QNetworkInterface::allInterfaces();
    bool process_loop = true;
    foreach (QNetworkInterface iface, ifaces) {
      if (!process_loop) break;
      auto addresses = iface.addressEntries();
      foreach (QNetworkAddressEntry addr, addresses) {
        if (addr.ip() == ifaceIP) {
          cameraIface = iface;
          process_loop = false;
          break;
        }
      }
    }

    if (cameraIface.isValid()) {
      int mtu = getMTU(cameraIface.name());
      camera->setMTU(mtu);
    }
  } else {
    QString message = tr("Network address not found, "
                         "trying best-effort MTU %1.");
    int mtu = 1500;
    statusBar()->showMessage(message.arg(mtu), statusTimeoutMsec);
    camera->setMTU(mtu);
  }

  if (camera->getMTU() == 0)
    cameraMTUDescription->setText(tr("Not an ethernet camera."));
  else {
    int mtu = camera->getMTU();
    QString ifname = cameraIface.name();
    QString description = tr("Camera is on interface %1,\nMTU set to %2.");
    description = description.arg(ifname);
    description = description.arg(QString::number(mtu));
    if (mtu < 3000)
      description += tr("\nConsider increasing the MTU!");
    cameraMTUDescription->setText(description);
  }

  camera->setAutoGain(false);
  camera->setAutoExposure(false);
  readAllValues();

  advancedTree->setModel(camera);
  advancedTree->header()->setResizeMode(QHeaderView::ResizeToContents);
  advancedTree->setItemDelegate(new QArvCameraDelegate);

  autoreadexposure->start();
  this->connect(camera,
                SIGNAL(dataChanged(QModelIndex,
                                   QModelIndex)), SLOT(readAllValues()));
}

void QArvMainWindow::readExposure() {
  bool blocked = exposureSlider->blockSignals(true);
  exposureSlider->setValue(value2slider_log(camera->getExposure(),
                                            exposurerange));
  exposureSlider->blockSignals(blocked);
  exposureSpinbox->setValue(camera->getExposure()/1000.);
}

void QArvMainWindow::readGain() {
  bool blocked = gainSlider->blockSignals(true);
  gainSlider->setValue(value2slider(camera->getGain(), gainrange));
  gainSlider->blockSignals(blocked);
  gainSpinbox->setValue(camera->getGain());
}

void QArvMainWindow::on_exposureSlider_valueChanged(int value) {
  camera->setExposure(slider2value_log(value, exposurerange));
}

void QArvMainWindow::on_gainSlider_valueChanged(int value) {
  camera->setGain(slider2value(value, gainrange));
}

void QArvMainWindow::on_exposureAutoButton_toggled(bool checked) {
  exposureSlider->setEnabled(!checked);
  exposureSpinbox->setEnabled(!checked);
  camera->setAutoExposure(checked);
}

void QArvMainWindow::on_gainAutoButton_toggled(bool checked) {
  gainSlider->setEnabled(!checked);
  gainSpinbox->setEnabled(!checked);
  camera->setAutoGain(checked);
}

void QArvMainWindow::on_pixelFormatSelector_currentIndexChanged(int index) {
  auto format = pixelFormatSelector->itemData(index).toString();
  camera->setPixelFormat(format);
}

void QArvMainWindow::on_applyROIButton_clicked(bool clicked) {
  QRect ROI(xSpinbox->value(), ySpinbox->value(),
            wSpinbox->value(), hSpinbox->value());

  {
    auto ROI2 = roirange.intersect(ROI);
    if (ROI2 != ROI)
      statusBar()->showMessage(tr("Region of interest too large, shrinking."),
                               statusTimeoutMsec);
    ROI = ROI2;
  }

  bool tostart = started;
  startVideo(false);
  ROI.setWidth((ROI.width() / 2 ) * 2);
  ROI.setHeight((ROI.height() / 2) * 2);
  camera->setROI(ROI);
  startVideo(tostart);
}

void QArvMainWindow::on_resetROIButton_clicked(bool clicked) {
  bool tostart = started;
  startVideo(false);
  camera->setROI(camera->getROIMaxSize());
  // It needs to be applied twice to reach maximum size because
  // X and Y decrease available range.
  QRect ROI = camera->getROIMaxSize();
  ROI.setWidth((ROI.width() / 2) * 2);
  ROI.setHeight((ROI.height() / 2) * 2);
  camera->setROI(ROI);
  startVideo(tostart);
}

void QArvMainWindow::on_binSpinBox_valueChanged(int value) {
  bool tostart = started;
  startVideo(false);
  int bin = binSpinBox->value();
  camera->setBinning(QSize(bin, bin));
  startVideo(tostart);
}

cv::Mat transformImage(cv::Mat img,
                    bool imageTransform_invert,
                    int imageTransform_flip,
                    int imageTransform_rot) {
  if (imageTransform_invert)
    cv::subtract((1<<16)-1, img, img);

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
  return img;
}

cv::Mat decodeAndTransformFrame(QByteArray frame,
                                QArvDecoder* decoder,
                                bool imageTransform_invert,
                                int imageTransform_flip,
                                int imageTransform_rot) {
  decoder->decode(frame);
  return transformImage(decoder->getCvImage(), imageTransform_invert,
                        imageTransform_flip, imageTransform_rot);
}

// Interestingly, QtConcurrent::run cannot take reference arguments.
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
          imgLine[4*j + px] = tmp;
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

void QArvMainWindow::takeNextFrame() {
  if (playing || recording) {
    currentRawFrame = camera->getFrame(dropInvalidFrames->isChecked(),
                                       nocopyCheck->isChecked(),
                                       &currentArvFrame);
    if (currentRawFrame.isEmpty()) {
      if (playing)
        video->setImage(invalidImage);
      return ;
    }

    currentFrame = decodeAndTransformFrame(currentRawFrame, decoder,
                                           invertColors->isChecked(),
                                           imageTransform_flip,
                                           imageTransform_rot);

    if ((playing || drawHistogram) && !futureRender.isRunning()) {
      Histograms* hists;
      if (drawHistogram) {
        futureHoldsAHistogram = true;
        drawHistogram = false;
        hists = histogram->unusedHistograms();
      } else {
        hists = nullptr;
      }
      currentRendering = currentFrame.clone();
      void (*theFunc) (const cv::Mat, QImage*, bool, QArv::Histograms*, bool);
      switch (currentRendering.type()) {
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
        video->setImage(invalidImage);
        qDebug() << "Invalid CV image format";
        return;
      }
      futureRender.setFuture(QtConcurrent::run(theFunc,
                                               currentRendering,
                                               video->unusedFrame(),
                                               markClipped->isChecked(),
                                               hists,
                                               histogramLog->isChecked()));
    }

    if (recording && standalone) {
      recorder->recordFrame(currentRawFrame, currentFrame);
      if (recorder->isOK()) {
        if (timestampFile.isOpen()) {
          quint64 ts = currentArvFrame->timestamp_ns;
          timestampFile.write(QString::number(ts).toAscii());
          timestampFile.write("\n");
        }
      } else {
        closeFileButton->setChecked(false);
      }
    }

    framecounter++;
  }
}

void QArvMainWindow::frameRendered() {
  if (playing)
    video->swapFrames();
  if (futureHoldsAHistogram) {
    futureHoldsAHistogram = false;
    histogram->swapHistograms(currentRendering.channels() == 1);
  }
}

void QArvMainWindow::startVideo(bool start) {
  if (toDisableWhenPlaying.isEmpty())
    toDisableWhenPlaying = {
      cameraSelector,
      refreshCamerasButton,
      streamFramesSpinbox,
      useFastInterpolator,
    };
  if (camera != NULL) {
    if (start && !started) {
      if (decoder != NULL) delete decoder;
      decoder = QArvPixelFormat::makeDecoder(camera->getPixelFormatId(),
                                             camera->getFrameSize(),
                                             useFastInterpolator->isChecked());
      invalidImage = QImage(camera->getFrameSize(),
                            QImage::Format_ARGB32_Premultiplied);
      invalidImage.fill(Qt::red);
      if (decoder == NULL)
        qCritical() << "Decoder for" << camera->getPixelFormat()
                    << "doesn't exist!";
      else {
        camera->setFrameQueueSize(streamFramesSpinbox->value());
        camera->startAcquisition();
        started = true;
        foreach (auto wgt, toDisableWhenPlaying) {
          wgt->setEnabled(false);
        }
        pixelFormatSelector->setEnabled(false);
      }
    } else if (!start && started) {
      QApplication::processEvents();
      camera->stopAcquisition();
      if (decoder != NULL) delete decoder;
      decoder = NULL;
      started = false;
      bool open = recorder && recorder->isOK();
      foreach (auto wgt, toDisableWhenPlaying) {
        wgt->setEnabled(!recording && !open);
      }
      pixelFormatSelector->setEnabled(pixelFormatSelector->count() > 1
                                      && !open
                                      && !started);
    }
  }
  // Set idle image on the histogram.
  histogram->setIdle();
}

void QArvMainWindow::on_playButton_clicked(bool checked) {
  playing = checked;
  startVideo(playing || recording);
  playing = checked && started;
  playButton->setChecked(playing);
  if (!playing) video->setImage();
}

void QArvMainWindow::on_recordButton_clicked(bool checked) {
  if (toDisableWhenRecording.isEmpty()) {
    toDisableWhenRecording = {
      fpsSpinbox,
      xSpinbox,
      wSpinbox,
      ySpinbox,
      hSpinbox,
      applyROIButton,
      resetROIButton,
      pickROIButton,
      binSpinBox,
      transformBox,
      filenameEdit,
      chooseFilenameButton,
      recordApendCheck,
      videoFormatSelector
    };
  }

  if (!standalone) goto skip_all_file_opening;

  if (filenameEdit->text().isEmpty()) {
    tabWidget->setCurrentWidget(recordingTab);
    statusBar()->showMessage(tr("Please set the video file name."),
                             statusTimeoutMsec);
    recordButton->setChecked(false);
    return;
  }

  if ((checked && !recorder) || !recorder->isOK()) {
    startVideo(true); // Initialize the decoder and all that.
    bool doAppend;
    if (recordApendCheck->isChecked() && recordApendCheck->isEnabled())
      doAppend = true;
    else
      doAppend = false;

    auto rct = camera->getROI();
    recorder.reset(OutputFormat::makeRecorder(decoder,
                                              filenameEdit->text(),
                                              videoFormatSelector->currentText(),
                                              rct.size(), fpsSpinbox->value(),
                                              doAppend,
                                              recordInfoCheck->isChecked()));
    bool open = recorder && recorder->isOK();

    if (!open) {
      recordButton->setChecked(false);
      checked = false;
    } else {
      statusBar()->clearMessage();
      QString msg;
      if (recordMetadataCheck->isChecked()) {
        auto metaFileName = filenameEdit->text() + ".caminfo";
        QFile metaFile(metaFileName);
        bool open = metaFile.open(QIODevice::WriteOnly);
        if (open) {
          QTextStream file(&metaFile);
          file << camera;
        } else
          msg += tr("Could not dump camera settings.");
      }
      if (recordTimestampsCheck->isChecked()) {
        auto tsFileName = filenameEdit->text() + ".timestamps";
        timestampFile.setFileName(tsFileName);
        bool open = timestampFile.open(QIODevice::WriteOnly);
        if (!open)
          msg += " " + tr("Could not open timestamp file.");
      }
      if (!msg.isNull()) statusBar()->showMessage(msg, statusTimeoutMsec);
    }
  }

skip_all_file_opening:

  recording = checked;
  startVideo(recording || playing);
  recording = checked && started;
  recordButton->setChecked(recording);
  recordButton->setIcon(recording ? pauseIcon : recordIcon);

  bool open = recorder && recorder->isOK();
  closeFileButton->setEnabled(!recording && open);
  foreach (auto wgt, toDisableWhenRecording) {
    wgt->setEnabled(!recording && !open);
  }

  emit recordingStarted(recording);
}

void QArvMainWindow::on_snapButton_clicked(bool checked) {
  if (snappathEdit->text().isEmpty() || snapbasenameEdit->text().isEmpty()) {
    tabWidget->setCurrentWidget(recordingTab);
    statusBar()->showMessage(tr("Please set the snapshot directory and name."),
                             statusTimeoutMsec);
    return;
  }
  QString name = snappathEdit->text();
  QDir dir(name);
  if (!dir.exists()) {
    tabWidget->setCurrentWidget(recordingTab);
    statusBar()->showMessage(tr("Snapshot directory does not exist."),
                             statusTimeoutMsec);
    return;
  }
  if (!playing) {
    statusBar()->showMessage(tr("Video is not playing, no image to save."),
                             statusTimeoutMsec);
    return;
  }
  statusBar()->clearMessage();

  auto time = QDateTime::currentDateTime();
  QString fileName = snappathEdit->text() + "/"
                     + snapbasenameEdit->text()
                     + time.toString("yyyy-MM-dd-hhmmss.zzz");
  if (snapshotPNG->isChecked()) {
    QImage img = QArvDecoder::CV2QImage_RGB24(currentFrame);
    if (!img.save(fileName + ".png"))
      statusBar()->showMessage(tr("Snapshot cannot be written."),
                               statusTimeoutMsec);
  } else if (snapshotRaw->isChecked()) {
    auto frame = camera->getFrame(dropInvalidFrames->isChecked(), false);
    if (frame.isEmpty()) {
      statusBar()->showMessage(tr("Current frame is invalid, try "
                                  "snapshotting again."), statusTimeoutMsec);
      return;
    }
    QFile file(fileName + ".frame");
    if (file.open(QIODevice::WriteOnly)) file.write(frame);
    else
      statusBar()->showMessage(tr("Snapshot cannot be written."),
                               statusTimeoutMsec);
  }
}

void QArvMainWindow::on_chooseFilenameButton_clicked(bool checked) {
  auto name = QFileDialog::getSaveFileName(this, tr("Open file"));
  if (!name.isNull()) filenameEdit->setText(name);
}

void QArvMainWindow::on_chooseSnappathButton_clicked(bool checked) {
  auto name = QFileDialog::getExistingDirectory(this, tr("Choose directory"));
  if (!name.isNull()) snappathEdit->setText(name);
}

void QArvMainWindow::on_fpsSpinbox_valueChanged(int value) {
  camera->setFPS(value);
  fpsSpinbox->setValue(camera->getFPS());
}

void QArvMainWindow::pickedROI(QRect roi) {
  pickROIButton->setChecked(false);
  QRect current = camera->getROI();

  // Compensate for the transform of the image. The actual transform must
  // be calculated using the size of the actual image, so we get this size
  // from the image prepared for invalid frames.
  auto imagesize = invalidImage.size();
  auto truexform = QImage::trueMatrix(imageTransform,
                                      imagesize.width(),
                                      imagesize.height());
  roi = truexform.inverted().map(QRegion(roi)).boundingRect();

  xSpinbox->setValue(current.x() + roi.x());
  ySpinbox->setValue(current.y() + roi.y());
  wSpinbox->setValue(roi.width());
  hSpinbox->setValue(roi.height());
  on_applyROIButton_clicked(true);
}

void QArvMainWindow::on_saveSettingsButton_clicked(bool checked) {
  QFileInfo fle(filenameEdit->text());
  auto name = QFileDialog::getSaveFileName(this, tr("Open file"),
                                           fle.dir().dirName());
  QFile outfile(name);
  bool open = outfile.open(QIODevice::WriteOnly);
  if (open) {
    QTextStream file(&outfile);
    file << camera;
  } else
    statusBar()->showMessage(tr("Could not open settings file."),
                             statusTimeoutMsec);
}

void QArvMainWindow::on_loadSettingsButton_clicked(bool checked) {
  QFileInfo fle(filenameEdit->text());
  auto name = QFileDialog::getOpenFileName(this, tr("Open file"),
                                           fle.dir().dirName());
  QFile infile(name);
  bool open = infile.open(QIODevice::ReadOnly);
  if (open) {
    QTextStream file(&infile);
    auto wholefile_ = file.readAll();
    QString readBack_;
    QTextStream wholefile(&wholefile_);
    QTextStream readBack(&readBack_);

    // Try setting it several times, then check if successful.
    for (int i = 0; i < 10; i++) {
      wholefile.seek(0);
      readBack.seek(0);
      wholefile >> camera;
      readBack << camera;
      readBack << endl << endl;
    }
    QStringList failures;
    wholefile.seek(0);
    while (!wholefile.atEnd()) {
      QString wanted = wholefile.readLine();
      QString actual = readBack.readLine();
      if (wanted != actual) {
        qDebug() << "wanted:" << wanted << endl << "actual:" << actual;
        failures << wanted;
      }
    }
    if (failures.count() != 0) {
      QString message = "<html><head/><body><p>"
                        + tr("Settings could not be completely loaded. "
                             "This can happen because camera features are interdependent and may "
                             "require a specific loading order. The following settings failed:")
                        + "</p>";
      foreach (auto fail, failures) message += fail;
      message += "</body></html>";
      QMessageBox::warning(this, tr("Loading settings failed"), message);
    }
  } else
    statusBar()->showMessage(tr("Could not open settings file."),
                             statusTimeoutMsec);
}

void QArvMainWindow::updateBandwidthEstimation() {
  int bw = camera->getEstimatedBW();
  if (bw == 0) {
    bandwidthDescription->setText(tr("Not an ethernet camera."));
  } else {
    QString unit(" B/s");
    if (bw >= 1024) {
      bw /= 1024;
      unit = " kB/s";
    }
    if (bw >= 1024) {
      bw /= 1024;
      unit = " MB/s";
    }
    bandwidthDescription->setText(QString::number(bw) + unit);
  }
}

void QArvMainWindow::updateImageTransform() {
  imageTransform.reset();
  imageTransform.scale(flipHorizontal->isChecked() ? -1 : 1,
                       flipVertical->isChecked() ? -1 : 1);
  int angle = rotationSelector->itemData(rotationSelector->
                                         currentIndex()).toInt();
  imageTransform.rotate(angle);

  if (flipHorizontal->isChecked() && flipVertical->isChecked())
    imageTransform_flip = -1;
  else if (flipHorizontal->isChecked() && !flipVertical->isChecked())
    imageTransform_flip = 1;
  else if (!flipHorizontal->isChecked() && flipVertical->isChecked())
    imageTransform_flip = 0;
  else if (!flipHorizontal->isChecked() && !flipVertical->isChecked())
    imageTransform_flip = -100; // Magic value
  imageTransform_rot = angle / 90;
}

void QArvMainWindow::showFPS() {
  actualFPS->setText(QString::number(framecounter));
  framecounter = 0;
}

void QArvMainWindow::on_editExposureButton_clicked(bool checked) {
  autoreadexposure->stop();
  exposureSpinbox->setReadOnly(false);
  exposureSpinbox->setFocus(Qt::OtherFocusReason);
  exposureSpinbox->selectAll();
}

void QArvMainWindow::on_editGainButton_clicked(bool checked) {
  autoreadexposure->stop();
  gainSpinbox->setReadOnly(false);
  gainSpinbox->setFocus(Qt::OtherFocusReason);
  gainSpinbox->selectAll();
}

void QArvMainWindow::on_gainSpinbox_editingFinished() {
  camera->setGain(gainSpinbox->value());
  gainSpinbox->setReadOnly(true);
  gainSpinbox->clearFocus();
  readGain();
  autoreadexposure->start();
}

void QArvMainWindow::on_exposureSpinbox_editingFinished() {
  camera->setExposure(exposureSpinbox->value()*1000);
  exposureSpinbox->setReadOnly(true);
  exposureSpinbox->clearFocus();
  readExposure();
  autoreadexposure->start();
}

void QArvMainWindow::on_videodock_visibilityChanged(bool visible) {
  showVideoButton->blockSignals(true);
  showVideoButton->setChecked(visible);
  showVideoButton->blockSignals(false);
}

void QArvMainWindow::on_videodock_topLevelChanged(bool floating) {
  if (floating) {
    videodock->setWindowFlags(Qt::Window | Qt::CustomizeWindowHint);
    videodock->setVisible(true);
  }
}

void QArvMainWindow::on_histogramdock_visibilityChanged(bool visible) {
  showHistogramButton->blockSignals(true);
  showHistogramButton->setChecked(visible);
  showHistogramButton->blockSignals(false);
}

void QArvMainWindow::on_histogramdock_topLevelChanged(bool floating) {
  if (floating) {
    histogramdock->setWindowFlags(Qt::Window | Qt::CustomizeWindowHint);
    histogramdock->setVisible(true);
  }
}

void QArvMainWindow::on_closeFileButton_clicked(bool checked) {
  recorder.reset();
  timestampFile.close();
  closeFileButton->setEnabled(recording);
  foreach (auto wgt, toDisableWhenRecording) {
    wgt->setEnabled(!recording);
  }
  foreach (auto wgt, toDisableWhenPlaying) {
    wgt->setEnabled(!started);
  }
  pixelFormatSelector->setEnabled(pixelFormatSelector->count() > 1
                                  && !started);
}

void QArvMainWindow::on_ROIsizeCombo_newSizeSelected(QSize size) {
  video->setSelectionSize(size);
}

void QArvMainWindow::histogramNextFrame() {
  drawHistogram = true;
}

ToolTipToRichTextFilter::ToolTipToRichTextFilter(int size_threshold,
                                                 QObject* parent) :
  QObject(parent), size_threshold(size_threshold) {}

bool ToolTipToRichTextFilter::eventFilter(QObject* obj, QEvent* evt) {
  if (evt->type() == QEvent::ToolTipChange) {
    QWidget* widget = static_cast<QWidget*>(obj);

    QObject* parent = qobject_cast<QObject*>(widget);
    bool doEnrich = false;
    while(NULL != (parent = parent->parent())) {
      if (parent->metaObject()->className() == QString("QArv::QArvMainWindow")) {
        doEnrich = true;
        break;
      }
    }

    if (doEnrich) {
      QString tooltip = widget->toolTip();
      if(!Qt::mightBeRichText(tooltip) && tooltip.size() > size_threshold) {
        // Prefix <qt/> to make sure Qt detects this as rich text
        // Escape the current message as HTML and replace \n by <br>
        tooltip = "<qt/>" + Qt::escape(tooltip);
        widget->setToolTip(tooltip);
        return true;
      }
    }
  }
  return QObject::eventFilter(obj, evt);
}

void QArvMainWindow::on_sliderUpdateSpinbox_valueChanged(int i) {
  autoreadexposure->setInterval(i);
}

void QArvMainWindow::on_histogramUpdateSpinbox_valueChanged(int i) {
  autoreadhistogram->setInterval(i);
}

void QArvMainWindow::on_statusTimeoutSpinbox_valueChanged(int i){
  statusTimeoutMsec = 1000*i;
}

void QArvMainWindow::setupListOfSavedWidgets() {
  // settings tab
  saved_widgets["qarv_settings/invert_colors"] = invertColors;
  saved_widgets["qarv_settings/flip_horizontal"] = flipHorizontal;
  saved_widgets["qarv_settings/flip_vertical"] = flipVertical;
  saved_widgets["qarv_settings/rotation"] = rotationSelector;
  saved_widgets["qarv_settings/drop_invalid_frames"] = dropInvalidFrames;
  saved_widgets["qarv_settings/mark_clipped"] = markClipped;
  saved_widgets["qarv_settings/exposure_update_ms"] = sliderUpdateSpinbox;
  saved_widgets["qarv_settings/histogram_update_ms"] = histogramUpdateSpinbox;
  saved_widgets["qarv_settings/statusbar_timeout"] = statusTimeoutSpinbox;
  saved_widgets["qarv_settings/frame_queue_size"] = streamFramesSpinbox;
  saved_widgets["qarv_settings/frame_transfer_nocopy"] = nocopyCheck;
  saved_widgets["qarv_settings/fast_swscale"] = useFastInterpolator;

  //recording tab
  saved_widgets["qarv_recording/snapshot_directory"] = snappathEdit;
  saved_widgets["qarv_recording/snapshot_basename"] = snapbasenameEdit;
  saved_widgets["qarv_recording/snapshot_raw"] = snapshotRaw;
  saved_widgets["qarv_recording/video_file"] = filenameEdit;
  saved_widgets["qarv_recording/video_format"] = videoFormatSelector;
  saved_widgets["qarv_recording/append_video"] = recordApendCheck;
  saved_widgets["qarv_recording/write_info"] = recordInfoCheck;
  saved_widgets["qarv_recording/write_timestamps"] = recordTimestampsCheck;
  saved_widgets["qarv_recording/dump_camera_settings"] = recordMetadataCheck;

  // video display
  saved_widgets["qarv_videodisplay/actual_size"] = unzoomButton;

  // histogram
  saved_widgets["qarv_histogram/logarithmic"] = histogramLog;
}

void QArvMainWindow::saveProgramSettings() {
  QSettings settings;

  // main window geometry and state
  settings.setValue("qarv_mainwindow/geometry", saveGeometry());
  settings.setValue("qarv_mainwindow/state", saveState());

  // buttons, combo boxes, text fields etc.
  for (auto i = saved_widgets.begin(); i != saved_widgets.end(); i++) {
    QWidget *widget = i.value();

    if (auto *w = qobject_cast<QAbstractButton*>(widget))
      settings.setValue(i.key(), w->isChecked());
    else if (auto *w = qobject_cast<QComboBox*>(widget))
      settings.setValue(i.key(), w->currentIndex());
    else if (auto *w = qobject_cast<QLineEdit*>(widget))
      settings.setValue(i.key(), w->text());
    else if (auto *w = qobject_cast<QSpinBox*>(widget))
      settings.setValue(i.key(), w->value());
    else
      qDebug() << "FIXME: don't know what to save under setting" << i.key();
  }
}

void QArvMainWindow::restoreProgramSettings() {
  QSettings settings;

  // main window geometry and state
  restoreGeometry(settings.value("qarv_mainwindow/geometry").toByteArray());
  restoreState(settings.value("qarv_mainwindow/state").toByteArray());

  // buttons, combo boxes, text fields etc.
  for (auto i = saved_widgets.begin(); i != saved_widgets.end(); i++) {
    QWidget *widget = i.value();
    QVariant data = settings.value(i.key());

    if (!data.isValid())
      continue;

    if (auto *w = qobject_cast<QAbstractButton*>(widget))
      w->setChecked(data.toBool());
    else if (auto *w = qobject_cast<QComboBox*>(widget))
      w->setCurrentIndex(data.toInt());
    else if (auto *w = qobject_cast<QLineEdit*>(widget))
      w->setText(data.toString());
    else if (auto *w = qobject_cast<QSpinBox*>(widget))
      w->setValue(data.toInt());
    else
      qDebug() << "FIXME: don't know how to restore setting" << i.key();
  }
}

void QArvMainWindow::on_videoFormatSelector_currentIndexChanged(int i) {
  auto fmt = qvariant_cast<OutputFormat*>(videoFormatSelector->itemData(i));
  if (fmt) {
    recordApendCheck->setEnabled(fmt->canAppend());
    recordInfoCheck->setEnabled(fmt->canWriteInfo());
  } else {
    qDebug() << "Video format data not OutputFormat";
  }
}
