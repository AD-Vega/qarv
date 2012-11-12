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

#include "mainwindow.h"

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

#include "getmtu_linux.h"
#include "globals.h"

const int slidersteps = 1000;

auto ffmpegOutputOptions = QStringList();
auto ffmpegOutputCommands = QHash<QString, QString>();
QString ffmpegInputCommand = "-pixel_format %1 -f rawvideo -s %2x%3 -i - ";

static void initFfmpegOutputCommands() {
  static const char* keys[] = {
    QT_TRANSLATE_NOOP("Video formats", "Raw data"),
    QT_TRANSLATE_NOOP("Video formats", "AVI (raw)"),
    QT_TRANSLATE_NOOP("Video formats", "AVI (processed)")
  };
  ffmpegOutputOptions = {
    QApplication::translate("Video formats", keys[0]),
    QApplication::translate("Video formats", keys[1]),
    QApplication::translate("Video formats", keys[2])
  };
  ffmpegOutputCommands[ffmpegOutputOptions[0]] = QString();
  ffmpegOutputCommands[ffmpegOutputOptions[1]] = "-f avi -codec rawvideo";
  ffmpegOutputCommands[ffmpegOutputOptions[2]] = "-f avi -codec huffyuv";
}

MainWindow::MainWindow(QWidget* parent, bool standalone_) :
  QMainWindow(parent), camera(NULL), playing(false), recording(false),
  started(false), drawHistogram(false), decoder(NULL),
  imageTransform(), framecounter(0), standalone(standalone_),
  toDisableWhenPlaying(), toDisableWhenRecording() {

  recordingfile = new QFile(this);

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

  if (ffmpegOutputCommands.isEmpty()) initFfmpegOutputCommands();
  videoFormatSelector->addItems(ffmpegOutputOptions);

  QSettings settings;
  restoreGeometry(settings.value("qarvmainwindow/geometry").toByteArray());
  restoreState(settings.value("qarvmainwindow/state").toByteArray());

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

  if (!standalone) tabWidget->removeTab(tabWidget->indexOf(recordingTab));

  auto timer = new QTimer(this);
  timer->setInterval(1000);
  this->connect(timer, SIGNAL(timeout()), SLOT(showFPS()));
  timer->start();

  QTimer::singleShot(300, this, SLOT(on_refreshCamerasButton_clicked()));

  statusBar()->showMessage(tr("Welcome to qarv!"));
}

MainWindow::~MainWindow() {
  on_closeFileButton_clicked(true);
}

void MainWindow::on_refreshCamerasButton_clicked(bool clicked) {
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
  cameraSelector->setEnabled(true);
  cameraSelector->blockSignals(false);
  cameraSelector->setCurrentIndex(0);
  QString message = tr(" Found %n cameras.",
                       "Number of cameras",
                       cameraSelector->count());
  statusBar()->showMessage(statusBar()->currentMessage() + message,
                           statusTimeoutMsec);
  on_cameraSelector_currentIndexChanged(0);
}

void MainWindow::on_unzoomButton_toggled(bool checked) {
  if (checked) {
    oldstate = saveState();
    oldgeometry = saveGeometry();
    oldsize = video->size();
    QSize newsize = video->getImage().size();
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

void MainWindow::readROILimits() {
  auto roisize = camera->getROIMaxSize();
  roirange = QRect(QPoint(1, 1), roisize.size());
  xSpinbox->setRange(1, roisize.width());
  ySpinbox->setRange(1, roisize.height());
  wSpinbox->setRange(roisize.x(), roisize.width());
  hSpinbox->setRange(roisize.y(), roisize.height());
}

void MainWindow::readAllValues() {
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
  xSpinbox->setValue(roi.x());
  ySpinbox->setValue(roi.y());
  wSpinbox->setValue(roi.width());
  hSpinbox->setValue(roi.height());
}

void MainWindow::on_cameraSelector_currentIndexChanged(int index) {
  autoreadexposure->stop();

  auto camid = cameraSelector->itemData(index).value<QArvCameraId>();
  if (camera != NULL) {
    startVideo(false);
    delete camera;
  }
  camera = new QArvCamera(camid, this);
  this->connect(camera, SIGNAL(frameReady()), SLOT(takeNextFrame()));

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

  on_resetROIButton_clicked(true);
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

void MainWindow::readExposure() {
  bool blocked = exposureSlider->blockSignals(true);
  exposureSlider->setValue(value2slider_log(camera->getExposure(),
                                            exposurerange));
  exposureSlider->blockSignals(blocked);
  exposureSpinbox->setValue(camera->getExposure()/1000.);
}

void MainWindow::readGain() {
  bool blocked = gainSlider->blockSignals(true);
  gainSlider->setValue(value2slider(camera->getGain(), gainrange));
  gainSlider->blockSignals(blocked);
  gainSpinbox->setValue(camera->getGain());
}

void MainWindow::on_exposureSlider_valueChanged(int value) {
  camera->setExposure(slider2value_log(value, exposurerange));
}

void MainWindow::on_gainSlider_valueChanged(int value) {
  camera->setGain(slider2value(value, gainrange));
}

void MainWindow::on_exposureAutoButton_toggled(bool checked) {
  exposureSlider->setEnabled(!checked);
  exposureSpinbox->setEnabled(!checked);
  camera->setAutoExposure(checked);
}

void MainWindow::on_gainAutoButton_toggled(bool checked) {
  gainSlider->setEnabled(!checked);
  gainSpinbox->setEnabled(!checked);
  camera->setAutoGain(checked);
}

void MainWindow::on_pixelFormatSelector_currentIndexChanged(int index) {
  auto format = pixelFormatSelector->itemData(index).toString();
  camera->setPixelFormat(format);
}

void MainWindow::on_applyROIButton_clicked(bool clicked) {
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
  camera->setROI(ROI);
  QRect roi = camera->getROI();
  xSpinbox->setValue(roi.x());
  ySpinbox->setValue(roi.y());
  wSpinbox->setValue(roi.width());
  hSpinbox->setValue(roi.height());
  startVideo(tostart);
}

void MainWindow::on_resetROIButton_clicked(bool clicked) {
  bool tostart = started;
  startVideo(false);
  camera->setROI(camera->getROIMaxSize());
  // It needs to be applied twice to reach maximum size.
  camera->setROI(camera->getROIMaxSize());
  readROILimits();
  QRect roi = camera->getROI();
  xSpinbox->setValue(roi.x());
  ySpinbox->setValue(roi.y());
  wSpinbox->setValue(roi.width());
  hSpinbox->setValue(roi.height());
  startVideo(tostart);
}

void MainWindow::on_binSpinBox_valueChanged(int value) {
  bool tostart = started;
  startVideo(false);
  int bin = binSpinBox->value();
  camera->setBinning(QSize(bin, bin));
  QSize binsize = camera->getBinning();
  binSpinBox->blockSignals(true);
  binSpinBox->setValue(binsize.width());
  binSpinBox->blockSignals(false);
  readROILimits();
  auto roi = camera->getROI();
  xSpinbox->setValue(roi.x());
  ySpinbox->setValue(roi.y());
  wSpinbox->setValue(roi.width());
  hSpinbox->setValue(roi.height());
  startVideo(tostart);
}

static QVector<QRgb> initHighlightColors() {
  QVector<QRgb> colors(2);
  colors[0] = qRgba(255, 0, 200, 0);
  colors[1] = qRgba(255, 0, 200, 255);
  return colors;
}

static QVector<QRgb> highlightColors = initHighlightColors();

void MainWindow::transformImage(QImage& img) {
  if (imageTransform.type() != QTransform::TxNone)
    img = img.transformed(imageTransform);
  if (invertColors->isChecked()) img.invertPixels();

  if (markClipped->isChecked()) {
    if (img.format() == QImage::Format_Indexed8) {
      auto colors = img.colorTable();
      colors[255] = qRgb(255, 0, 200);
      img.setColorTable(colors);
    } else {
      auto mask = img.createMaskFromColor(qRgb(255, 255, 255));
      mask.setColorTable(highlightColors);
      QPainter painter(&img);
      painter.drawImage(img.rect(), mask);
    }
  }
}

void MainWindow::getNextFrame(QImage* processed,
                              QImage* unprocessed,
                              QByteArray* raw,
                              ArvBuffer** rawAravisBuffer,
                              bool nocopy) {
  if (processed == NULL
      && unprocessed == NULL
      && raw == NULL
      && rawAravisBuffer == NULL)
    return;

  QByteArray frame = camera->getFrame(dropInvalidFrames->isChecked(), nocopy,
                                      rawAravisBuffer);
  if (raw != NULL) *raw = frame;

  QImage img;
  if (unprocessed != NULL
      || processed != NULL) {
    if (frame.isEmpty()) img = invalidImage;
    else img = decoder->decode(frame);
  }

  if (unprocessed != NULL) *unprocessed = img;

  if (processed != NULL) {
    transformImage(img);
    *processed = img;
  }
}

void MainWindow::takeNextFrame() {
  if (playing || recording) {
    QByteArray frame;
    QImage image;

    if (playing || drawHistogram || videoFormatSelector->currentIndex() >= 2)
      getNextFrame(&image, NULL, &frame, NULL, nocopyCheck->isChecked());
    else
      getNextFrame(NULL, NULL, &frame, NULL, nocopyCheck->isChecked());

    if (playing) video->setImage(image);

    if (drawHistogram && !frame.isEmpty()) {
      histogram->fromImage(image);
      drawHistogram = false;
    }

    if (recording && !frame.isEmpty()) {
      if (videoFormatSelector->currentIndex() < 2) {
        QByteArray outframe(frame);
        outframe.data(); // make deep copy, writing can block
        recordingfile->write(outframe);
      } else {
        image = image.convertToFormat(QImage::Format_RGB888);
        for (int i = 0; i < image.height(); i++)
          recordingfile->write((char*)(image.scanLine(i)),
                               image.bytesPerLine());
      }
    }

    if (!frame.isEmpty()) {
      framecounter++;
    }
  }
}

void MainWindow::startVideo(bool start) {
  if (toDisableWhenPlaying.isEmpty())
    toDisableWhenPlaying << cameraSelector << refreshCamerasButton;
  if (camera != NULL) {
    if (start && !started) {
      if (decoder != NULL) delete decoder;
      decoder = makeFrameDecoder(camera->getPixelFormat(),
                                 camera->getFrameSize());
      invalidImage = QImage(camera->getFrameSize(), QImage::Format_RGB32);
      invalidImage.fill(Qt::red);
      if (decoder == NULL)
        qCritical() << "Decoder for" << camera->getPixelFormat()
                    << "doesn't exist!";
      else {
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
      foreach (auto wgt, toDisableWhenPlaying) {
        wgt->setEnabled(!recordingfile->isOpen());
      }
      pixelFormatSelector->setEnabled(pixelFormatSelector->count() > 1
                                      && !recordingfile->isOpen());
    }
  }
  // Set idle image on the histogram.
  histogram->fromImage();
}

void MainWindow::on_playButton_clicked(bool checked) {
  playing = checked;
  startVideo(playing || recording);
  playing = checked && started;
  playButton->setChecked(playing);
  if (!playing) video->setImage();
}

void MainWindow::on_recordButton_clicked(bool checked) {
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
      settingsTab,
      filenameEdit,
      chooseFilenameButton,
      recordApendCheck,
      recordLogCheck,
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

  if (checked && !recordingfile->isOpen()) {
    startVideo(true); // Initialize the decoder and all that.
    QIODevice::OpenMode openflags;
    if (recordApendCheck->isChecked() && recordApendCheck->isEnabled())
      openflags = QIODevice::Append;
    else
      openflags = QIODevice::Truncate | QIODevice::WriteOnly;
    bool open = false;
    {
      auto thefile = qobject_cast<QFile*>(recordingfile);
      thefile->setFileName(filenameEdit->text());
      auto info = QFileInfo(*thefile);
      auto dir = info.dir();
      if (!dir.exists()) {
        statusBar()->showMessage(tr("Video directory does not exist."),
                                 statusTimeoutMsec);
        goto skip_file_opening;
      }
    }
    if (videoFormatSelector->currentIndex() == 0) {
      open = recordingfile->open(openflags);
      if (!open)
        statusBar()->showMessage(tr("Cannot open video file for recording."),
                                 statusTimeoutMsec);
    } else {
      auto ffmpeg = new QProcess(this);
      QString cmd = ffmpegInputCommand;
      QString fmt;
      if (videoFormatSelector->currentIndex() == 1) {
        fmt = decoder->ffmpegPixfmtRaw();
        if (fmt.isNull()) {
          statusBar()->showMessage(tr("Unable to record. "
                                      "AVI cannot store this pixel format in raw "
                                      "form. Use processed form instead."),
                                   statusTimeoutMsec);
          open = false;
        } else {
          open = true;
        }
      } else {
        fmt = "rgb24";
        open = true;
      }
      if (open) {
        cmd = cmd.arg(fmt);
        if (videoFormatSelector->currentIndex() < 2) {
          cmd = cmd.arg(camera->getFrameSize().width());
          cmd = cmd.arg(camera->getFrameSize().height());
        } else {
          QImage emptyimg(camera->getFrameSize().width(),
                          camera->getFrameSize().height(), QImage::Format_Mono);
          transformImage(emptyimg);
          cmd = cmd.arg(emptyimg.width());
          cmd = cmd.arg(emptyimg.height());
        }
        cmd += ffmpegOutputCommands[videoFormatSelector->currentText()];
        cmd += QString(" -r %1 -").arg(fpsSpinbox->value());

        auto fname = qobject_cast<QFile*>(recordingfile)->fileName();
        ffmpeg->setStandardOutputFile(fname, openflags);
        if (recordLogCheck->isChecked() && recordLogCheck->isEnabled())
          ffmpeg->setStandardErrorFile(fname + ".log", QIODevice::Truncate);
        qDebug() << "ffmpeg" << cmd;
        ffmpeg->start("ffmpeg", cmd.split(' ', QString::SkipEmptyParts));
        open = ffmpeg->waitForStarted(10000);
      }
      if (open) {
        delete recordingfile;
        recordingfile = ffmpeg;
      }
    }

skip_file_opening:

    if (!open) {
      recordButton->setChecked(false);
      checked = false;
    } else
      statusBar()->clearMessage();
  }

skip_all_file_opening:

  recording = checked;
  startVideo(recording || playing);
  recording = checked && started;
  recordButton->setChecked(recording);
  recordButton->setIcon(recording ? pauseIcon : recordIcon);

  closeFileButton->setEnabled(!recording && recordingfile->isOpen());
  foreach (auto wgt, toDisableWhenRecording) {
    wgt->setEnabled(!recordingfile->isOpen());
  }
  on_videoFormatSelector_currentIndexChanged(videoFormatSelector->currentIndex());

  emit recordingStarted(recording);
}

void MainWindow::on_snapButton_clicked(bool checked) {
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
                     + time.toString("yyyy-MM-dd-hh:mm:ss:zzz");
  if (snapshotPNG->isChecked()) {
    auto img = video->getImage();
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

void MainWindow::on_chooseFilenameButton_clicked(bool checked) {
  auto name = QFileDialog::getSaveFileName(this, tr("Open file"));
  if (!name.isNull()) filenameEdit->setText(name);
}

void MainWindow::on_chooseSnappathButton_clicked(bool checked) {
  auto name = QFileDialog::getExistingDirectory(this, tr("Choose directory"));
  if (!name.isNull()) snappathEdit->setText(name);
}

void MainWindow::on_fpsSpinbox_valueChanged(int value) {
  camera->setFPS(value);
  fpsSpinbox->setValue(camera->getFPS());
}

void MainWindow::pickedROI(QRect roi) {
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

void MainWindow::on_saveSettingsButton_clicked(bool checked) {
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

void MainWindow::on_loadSettingsButton_clicked(bool checked) {
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

void MainWindow::updateBandwidthEstimation() {
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

void MainWindow::closeEvent(QCloseEvent* event) {
  QSettings settings;
  settings.setValue("qarvmainwindow/geometry", saveGeometry());
  settings.setValue("qarvmainwindow/state", saveState());
  QMainWindow::closeEvent(event);
}

void MainWindow::updateImageTransform() {
  imageTransform.reset();
  imageTransform.scale(flipHorizontal->isChecked() ? -1 : 1,
                       flipVertical->isChecked() ? -1 : 1);
  int angle = rotationSelector->itemData(rotationSelector->
                                         currentIndex()).toInt();
  imageTransform.rotate(angle);
}

void MainWindow::showFPS() {
  actualFPS->setText(QString::number(framecounter));
  framecounter = 0;
}

void MainWindow::on_editExposureButton_clicked(bool checked) {
  autoreadexposure->stop();
  exposureSpinbox->setReadOnly(false);
  exposureSpinbox->setFocus(Qt::OtherFocusReason);
  exposureSpinbox->selectAll();
}

void MainWindow::on_editGainButton_clicked(bool checked) {
  autoreadexposure->stop();
  gainSpinbox->setReadOnly(false);
  gainSpinbox->setFocus(Qt::OtherFocusReason);
  gainSpinbox->selectAll();
}

void MainWindow::on_gainSpinbox_editingFinished() {
  camera->setGain(gainSpinbox->value());
  gainSpinbox->setReadOnly(true);
  gainSpinbox->clearFocus();
  readGain();
  autoreadexposure->start();
}

void MainWindow::on_exposureSpinbox_editingFinished() {
  camera->setExposure(exposureSpinbox->value()*1000);
  exposureSpinbox->setReadOnly(true);
  exposureSpinbox->clearFocus();
  readExposure();
  autoreadexposure->start();
}

void MainWindow::on_videodock_visibilityChanged(bool visible) {
  showVideoButton->blockSignals(true);
  showVideoButton->setChecked(visible);
  showVideoButton->blockSignals(false);
}

void MainWindow::on_videodock_topLevelChanged(bool floating) {
  if (floating) {
    videodock->setWindowFlags(Qt::Window | Qt::CustomizeWindowHint);
    videodock->setVisible(true);
  }
}

void MainWindow::on_histogramdock_visibilityChanged(bool visible) {
  showHistogramButton->blockSignals(true);
  showHistogramButton->setChecked(visible);
  showHistogramButton->blockSignals(false);
}

void MainWindow::on_histogramdock_topLevelChanged(bool floating) {
  if (floating) {
    histogramdock->setWindowFlags(Qt::Window | Qt::CustomizeWindowHint);
    histogramdock->setVisible(true);
  }
}

void MainWindow::on_closeFileButton_clicked(bool checked) {
  auto ffmpeg = qobject_cast<QProcess*>(recordingfile);
  statusBar()->showMessage(tr("Finalizing video recording, please wait..."));
  QApplication::processEvents();
  QApplication::flush();
  if (ffmpeg != NULL) {
    ffmpeg->closeWriteChannel();
    ffmpeg->waitForFinished();
  } else {
    recordingfile->close();
  }
  delete recordingfile;
  recordingfile = new QFile(this);
  statusBar()->clearMessage();

  closeFileButton->setEnabled(recording);
  foreach (auto wgt, toDisableWhenRecording) {
    wgt->setEnabled(!recording);
  }
  foreach (auto wgt, toDisableWhenPlaying) {
    wgt->setEnabled(!started);
  }
  pixelFormatSelector->setEnabled(pixelFormatSelector->count() > 1
                                  && !started);
  on_videoFormatSelector_currentIndexChanged(videoFormatSelector->currentIndex());
}

void MainWindow::on_videoFormatSelector_currentIndexChanged(int index) {
  if (index != 0) {
    recordApendCheck->setEnabled(false);
    recordLogCheck->setEnabled(true);
  } else {
    recordApendCheck->setEnabled(true);
    recordLogCheck->setEnabled(false);
  }
}

void MainWindow::on_ROIsizeCombo_newSizeSelected(QSize size) {
  video->setSelectionSize(size);
}

void MainWindow::histogramNextFrame() {
  drawHistogram = true;
}

ToolTipToRichTextFilter::ToolTipToRichTextFilter(int size_threshold,
                                                 QObject* parent) :
  size_threshold(size_threshold), QObject(parent) {}

bool ToolTipToRichTextFilter::eventFilter(QObject* obj, QEvent* evt) {
  if (evt->type() == QEvent::ToolTipChange) {
    QWidget* widget = static_cast<QWidget*>(obj);
    QString tooltip = widget->toolTip();
    if (!Qt::mightBeRichText(tooltip) && tooltip.size() > size_threshold) {
      // Prefix <qt/> to make sure Qt detects this as rich text
      // Escape the current message as HTML and replace \n by <br>
      tooltip = "<qt/>" + Qt::escape(tooltip);
      widget->setToolTip(tooltip);
      return true;
    }
  }
  return QObject::eventFilter(obj, evt);
}

void MainWindow::on_sliderUpdateSpinbox_valueChanged(int i) {
  autoreadexposure->setInterval(i);
}

void MainWindow::on_histogramUpdateSpinbox_valueChanged(int i) {
  autoreadhistogram->setInterval(i);
}

void MainWindow::on_statusTimeoutSpinbox_valueChanged(int i){
  statusTimeoutMsec = 1000*i;
}
