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

#include "globals.h"
#include "qarvmainwindow.h"
#include "api/qarvcameradelegate.h"
#include "getmtu_linux.h"
#include "decoders/unsupported.h"
#include "filters/filter.h"

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
#include <QMenu>
#include <QToolButton>


extern "C" {
  #include <arvbuffer.h>
}

using namespace QArv;

QArvMainWindow::QArvMainWindow(QWidget* parent, bool standalone_) :
  QMainWindow(parent), camera(NULL), decoder(NULL), playing(false),
  recording(false), started(false), drawHistogram(false),
  standalone(standalone_), imageTransform(),
  imageTransform_flip(0), imageTransform_rot(0), framecounter(0),
  toDisableWhenPlaying(), toDisableWhenRecording(), futureHoldsAHistogram(false),
  recordingTimeCumulative(0) {

  setAttribute(Qt::WA_DeleteOnClose);

  logMessage() << "Please ignore \"Could not resolve property\" warnings "
                  "unless icons look bad.";
  setupUi(this);
  on_statusTimeoutSpinbox_valueChanged(statusTimeoutSpinbox->value());
  messageList->connect(&QArvDebug::messageSender, SIGNAL(newDebugMessage(QString)),
                       SLOT(appendPlainText(QString)));

  aboutLabel->setText(aboutLabel->text().arg(QARV_VERSION));

  workthread = new Workthread(this);
  connect(workthread, SIGNAL(frameCooked(cv::Mat)), SLOT(frameProcessed(cv::Mat)));
  connect(workthread, SIGNAL(frameRendered()), SLOT(frameRendered()));

  // Setup theme icons if available.
  bool usingFallbackIcons = false;
  QMap<QAbstractButton*, QString> icons;
  icons[unzoomButton] = "zoom-original";
  icons[playButton] = "media-playback-start";
  icons[refreshCamerasButton] = "view-refresh";
  icons[chooseFilenameButton] = "document-open";
  icons[chooseSnappathButton] = "document-open";
  icons[editGainButton] = "edit-clear-locationbar-rtl";
  icons[editExposureButton] = "edit-clear-locationbar-rtl";
  icons[histogramLog] = "view-object-histogram-logarithmic";
  icons[pickROIButton] = "edit-select";
  for (auto i = icons.begin(); i != icons.end(); i++) {
    if (!QIcon::hasThemeIcon(*i)) {
      i.key()->setIcon(QIcon(QString(qarv_datafiles) + *i + ".svgz"));
      usingFallbackIcons = true;
    }
  }
  QMap<QAction*, QString> aicons;
  aicons[showVideoAction] = "video-display";
  aicons[recordAction] = "media-record";
  aicons[closeFileAction] = "media-playback-stop";
  aicons[showHistogramAction] = "office-chart-bar";
  aicons[messageAction] = "dialog-information";
  for (auto i = aicons.begin(); i != aicons.end(); i++) {
    if (!QIcon::hasThemeIcon(*i)) {
      i.key()->setIcon(QIcon(QString(qarv_datafiles) + *i + ".svgz"));
      usingFallbackIcons = true;
    }
  }
  if (usingFallbackIcons)
    logMessage() << "Some icons are not available in your theme, using bundled icons.";
  {
    auto d = QDate::currentDate();
    int y = d.year(), l = d.month(), a = d.day();
    int j = y % 19; int k = y / 100; int h = y % 100; int
    m = k / 4; int n = k % 4; int p = (k + 8) / 25; int
    q = (k - p + 1) / 3; int r = (19 * j + k - m - q + 15
    ) % 30; int s = h / 4; int u = h % 4; int v = (32 + 2
    * n + 2 * s - r - u) % 7; int w = (j + 11 * r + 22 *
    v) / 451; int x = r + v - 7 * w + 114; int z = x % 31
    + 1; x = x / 31;
    if (l == x && (z == a || z+1 == a || z-1 == a)) {
    QWidget* wgt = new QWidget(this);
    wgt->setLayout(new QHBoxLayout);
    wgt->layout()->setMargin(30);
    char tmp[10];
    std::strcpy(tmp, "$6872F=E");
    for (int i = 0; i < 8; i++)
      tmp[i] = (tmp[i] + (16 ^ 63)) % (1<<7);
    QGridLayout* ay = static_cast<QGridLayout*>(aboutTab->layout());
    ay->addWidget(wgt, 1, 0);
    auto r = new QPushButton(tmp);
    wgt->layout()->addWidget(r);
    connect(r, SIGNAL(clicked(bool)), SLOT(on_replayButton_clicked(bool)));
    QPalette p = wgt->palette();
    p.setColor(wgt->backgroundRole(), qRgb(226, 53, 48));
    wgt->setPalette(p);
    wgt->setAutoFillBackground(true);}
  }

  // Setup the subwindows menu.
  auto submenu = new QMenu;
  submenu->addAction(showVideoAction);
  submenu->addAction(showHistogramAction);
  submenu->addAction(messageAction);
  auto toolbutton = new QToolButton;
  toolbutton->setMenu(submenu);
  toolbutton->setPopupMode(QToolButton::InstantPopup);
  toolbutton->setToolButtonStyle(Qt::ToolButtonTextOnly);
  toolbutton->setText(tr("Subwindows"));
  subwindowToolbar->addWidget(toolbutton);

  // Setup the postproc filter menu
  auto plugins = QPluginLoader::staticInstances();
  auto postprocMenu = new QMenu;
  foreach (auto plugin, plugins) {
    auto filterPlugin = qobject_cast<ImageFilterPlugin*>(plugin);
    if (filterPlugin != NULL) {
      auto action = new QAction(filterPlugin->name(), this);
      action->setData(QVariant::fromValue(filterPlugin));
      connect(action, SIGNAL(triggered(bool)), SLOT(addPostprocFilter()));
      postprocMenu->addAction(action);
    }
  }
  postprocAddButton->setMenu(postprocMenu);
  postprocChain.setColumnCount(1);
  postprocList->setModel(&postprocChain);
  connect(&postprocChain, SIGNAL(dataChanged(QModelIndex,QModelIndex)), SLOT(updatePostprocQList()));

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

  if (!standalone) {
    setWindowFlags(windowFlags() & ~Qt::WindowCloseButtonHint);
    setAttribute(Qt::WA_DeleteOnClose);
    setAttribute(Qt::WA_QuitOnClose, false);
    tabWidget->removeTab(tabWidget->indexOf(recordingTab));
    snapshotAction->setEnabled(false);
  }

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

  auto makeVerticalLine = [](){
    auto l = new QFrame;
    l->setFrameShape(QFrame::VLine);
    l->setLineWidth(0);
    return l;
  };
  statusBar()->addPermanentWidget(makeVerticalLine());
  recordingTimeLabel = new QLabel(tr("Recording stopped"));
  statusBar()->addPermanentWidget(recordingTimeLabel);
  statusBar()->addPermanentWidget(makeVerticalLine());
  queueUsage = new QProgressBar;
  QString queueLabelText = tr("Buffer");
  queueUsage->setFormat(queueLabelText);
  queueUsage->setToolTip("<qt/>"
                         + tr("This bar shows filling of the frame "
                              "queue/buffer. If it overflows, the "
                              "frames in the queue will be discarded."));
  auto tmpLabel = new QLabel(queueLabelText);
  statusBar()->addPermanentWidget(tmpLabel);
  queueUsage->setMaximumWidth(tmpLabel->width() + 20);
  delete tmpLabel;
  queueUsage->setValue(0);
  statusBar()->addPermanentWidget(queueUsage);
  statusBar()->showMessage(tr("Welcome to QArv!"));
}

QArvMainWindow::~QArvMainWindow() {
  stopAllAcquisition();
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
  QString message = tr("Found %n cameras.",
                       "Number of cameras",
                       cameraSelector->count());
  statusBar()->showMessage(statusBar()->currentMessage() + " " + message,
                           statusTimeoutMsec);
  logMessage() << message;
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
  roirange = QRect(QPoint(0, 0), roisize.size());
  xSpinbox->setRange(0, roisize.width());
  ySpinbox->setRange(0, roisize.height());
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
  pixelFormatSelector->setEnabled(noofframes > 1 && !started);
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

void QArvMainWindow::on_replayButton_clicked(bool checked)
{
    const char k[] = {72, 97, 112, 112, 121, 32, 69, 97, 115, 116, 101, 114, 33, 0};
    const char u[] = {71, 108, 97, 100, 32, 121, 111, 117, 32, 102, 111, 117, 110, 100, 32, 116, 104, 105, 115, 33, 0};
    QMessageBox m(QMessageBox::Information, k, u, QMessageBox::NoButton, this);
    m.show();
    QApplication::processEvents();
    sleep(1);
    QApplication::processEvents();
    sleep(5);
    kill(getpid(), 11);
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
  this->connect(camera, SIGNAL(bufferUnderrun()), SLOT(bufferUnderrunOccured()));
  this->connect(camera, SIGNAL(frameReady()), SLOT(takeNextFrame()));

  logMessage() << "Pixel formats:" << camera->getPixelFormats();

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
    message = message.arg(mtu);
    statusBar()->showMessage(message, statusTimeoutMsec);
    logMessage() << message;
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
  xSpinbox->setValue((xSpinbox->value() / 2) * 2);
  ySpinbox->setValue((ySpinbox->value() / 2) * 2);
  double tmp;
  tmp = (wSpinbox->value() / 2) * 2;
  tmp = tmp < 8 ? 8 : tmp;
  wSpinbox->setValue(tmp);
  tmp = (hSpinbox->value() / 2) * 2;
  tmp = tmp < 8 ? 8 : tmp;
  hSpinbox->setValue(tmp);
  QRect ROI(xSpinbox->value(), ySpinbox->value(),
            wSpinbox->value(), hSpinbox->value());

  {
    auto ROI2 = roirange.intersect(ROI);
    if (ROI2 != ROI)
      statusBar()->showMessage(tr("Region of interest too large, shrinking."),
                               statusTimeoutMsec);
    ROI = ROI2;
    ROI.setX((ROI.x() / 2) * 2);
    ROI.setY((ROI.y() / 2) * 2);
    ROI.setWidth((ROI.width() / 2) * 2);
    ROI.setHeight((ROI.height() / 2) * 2);
  }

  bool tostart = started;
  startVideo(false);
  camera->setROI(ROI);
  startVideo(tostart);
}

void QArvMainWindow::on_resetROIButton_clicked(bool clicked) {
  camera->setROI(camera->getROIMaxSize());
  // It needs to be applied twice to reach maximum size because
  // X and Y decrease available range.
  QRect ROI = camera->getROIMaxSize();
  xSpinbox->setValue(0);
  ySpinbox->setValue(0);
  wSpinbox->setValue(ROI.width());
  hSpinbox->setValue(ROI.height());
  on_applyROIButton_clicked(true);
}

void QArvMainWindow::on_binSpinBox_valueChanged(int value) {
  bool tostart = started;
  startVideo(false);
  int bin = binSpinBox->value();
  camera->setBinning(QSize(bin, bin));
  startVideo(tostart);
}

void QArvMainWindow::takeNextFrame() {
  int cookerQueue1, cookerQueue2;
  queueUsage->setValue(workthread->queueSize(cookerQueue1, cookerQueue2));
  int queueMax = streamFramesSpinbox->value();
  /* "Full" should represent the state of queue1. However, we also want the
   * bar to show how the queue empties when we stop acquisition. So we set
   * the maximum such that the bar is full when queue1 is full and it also
   * contains queue2.
   */
  queueUsage->setMaximum(queueMax / 2 + cookerQueue2);

  if (!started)
    return;

  framecounter++;
  if (playing || recording) {
    currentRawFrame = camera->getFrame(dropInvalidFrames->isChecked(),
                                       nocopyCheck->isChecked(),
                                       &currentArvFrame);
    if (currentRawFrame.isEmpty()) {
      if (playing)
        video->setImage(invalidImage);
      return ;
    }

    quint64 ts;
#ifdef ARAVIS_OLD_BUFFER
    ts = currentArvFrame->timestamp_ns;
#else
    ts = arv_buffer_get_timestamp(currentArvFrame);
#endif
    Recorder* myrecorder = (recording && standalone) ? recorder.data() : NULL;
    bool running = workthread->cookFrame(queueMax,
                                         currentRawFrame,
                                         ts,
                                         decoder,
                                         invertColors->isChecked(),
                                         imageTransform_flip,
                                         imageTransform_rot,
                                         postprocChainAsList,
                                         timestampFile,
                                         myrecorder);
    if (!running) {
      QString msg = tr("Too slow, dropping frames!");
      logMessage() << msg;
      statusBar()->showMessage(msg, statusTimeoutMsec);
    }

    // Actual recording is delayed, but we need to count the
    // frames now if we want to stop recording after a set number.
    if (recording && standalone) {
      recordedFrames++;
      if (stopRecordingFramesRadio->isChecked() &&
          recordedFrames >= stopRecordingFrames->value()) {
        recordAction->setChecked(false);
        closeFileAction->trigger();
      }
    }
  }
}

void QArvMainWindow::frameProcessed(cv::Mat frame) {
  currentFrame = frame;
  if ((playing || drawHistogram)) {
    Histograms* hists;
    if (drawHistogram) {
      futureHoldsAHistogram = true;
      drawHistogram = false;
      hists = histogram->unusedHistograms();
    } else {
      hists = nullptr;
    }
    workthread->renderFrame(currentFrame,
                            video->unusedFrame(),
                            markClipped->isChecked(),
                            hists,
                            histogramLog->isChecked());
  }

  if (recording && standalone) {
    if (!recorder->isOK()) {
      auto message = tr("Recording plugin failed, stopped recording.");
      statusBar()->showMessage(message, statusTimeoutMsec);
      logMessage() << message;
      recordAction->setChecked(false);
      closeFileAction->trigger();
    }
  }
}

void QArvMainWindow::frameRendered() {
  if (playing)
    video->swapFrames();
  if (futureHoldsAHistogram) {
    futureHoldsAHistogram = false;
    histogram->swapHistograms(currentFrame.channels() == 1);
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
    setEnabled(false);
    if (start && !started) {
      if (decoder != NULL) delete decoder;
      decoder = QArvDecoder::makeDecoder(camera->getPixelFormatId(),
                                         camera->getFrameSize(),
                                         useFastInterpolator->isChecked());
      invalidImage = QImage(camera->getFrameSize(),
                            QImage::Format_ARGB32_Premultiplied);
      invalidImage.fill(Qt::red);
      if (decoder == NULL) {
        QString message = tr("Decoder for %1 doesn't exist!");
        message = message.arg(camera->getPixelFormat());
        logMessage() << message;
        statusBar()->showMessage(message, statusTimeoutMsec);
        if (standalone)
          decoder = new Unsupported(camera->getPixelFormatId(),
                                    camera->getFrameSize());
      }
      if (decoder != NULL) {
        camera->setFrameQueueSize(streamFramesSpinbox->value());
        camera->startAcquisition();
        started = true;
        foreach (auto wgt, toDisableWhenPlaying) {
          wgt->setEnabled(false);
        }
        pixelFormatSelector->setEnabled(false);
      }
      queueUsage->setMaximum(streamFramesSpinbox->value());
      queueUsage->setValue(0);
    } else if (!start && started) {
      started = false;
      do {
        QApplication::processEvents();
      } while (workthread->isBusy());
      camera->stopAcquisition();
      if (decoder != NULL) delete decoder;
      decoder = NULL;
      bool open = recorder && recorder->isOK();
      foreach (auto wgt, toDisableWhenPlaying) {
        wgt->setEnabled(!recording && !open);
      }
      pixelFormatSelector->setEnabled(pixelFormatSelector->count() > 1
                                      && !open
                                      && !started);
    }
    setEnabled(true);
  }
  // Set idle image on the histogram.
  histogram->setIdle();
}

void QArvMainWindow::on_playButton_toggled(bool checked) {
  playing = checked;
  startVideo(playing || recording);
  playing = checked && started;
  playButton->setChecked(playing);
  if (!playing) video->setImage();
}

void QArvMainWindow::on_recordAction_toggled(bool checked) {
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
      videoFormatSelector,
      recordInfoCheck,
      recordMetadataCheck,
      recordTimestampsCheck
    };
  }

  if (!standalone) goto skip_all_file_opening;

  if ((checked && !recorder) || (recorder && !recorder->isOK())) {
    if (filenameEdit->text().isEmpty()) {
      tabWidget->setCurrentWidget(recordingTab);
      statusBar()->showMessage(tr("Please set the video file name."),
                               statusTimeoutMsec);
      recordAction->setChecked(false);
      recordingTime = QTime();
      recordingTimeCumulative = 0;
      return;
    }

    startVideo(true); // Initialize the decoder and all that.
    if (!started) {
      recordAction->setChecked(false);
      goto skip_all_file_opening;
    }

    auto rct = camera->getROI();
    recorder.reset(OutputFormat::makeRecorder(decoder,
                                              filenameEdit->text(),
                                              videoFormatSelector->currentText(),
                                              rct.size(), fpsSpinbox->value(),
                                              recordInfoCheck->isChecked()));
    bool open = recorder && recorder->isOK();

    if (!open) {
      QString message = tr("Unable to initialize the recording plugin.");
      logMessage() << message;
      statusBar()->showMessage(message, statusTimeoutMsec);
      recordAction->setChecked(false);
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
      if (!msg.isNull()) {
        logMessage() << msg;
        statusBar()->showMessage(msg, statusTimeoutMsec);
      }
    }
  }

skip_all_file_opening:

  recording = checked;
  startVideo(recording || playing);
  bool error = checked && !started;
  recording = checked && started;
  recordAction->blockSignals(true);
  recordAction->setChecked(recording);
  recordAction->blockSignals(false);

  bool open = recorder && recorder->isOK();
  closeFileAction->setEnabled(!recording && open);
  foreach (auto wgt, toDisableWhenRecording) {
    wgt->setEnabled(!recording && !open);
    on_videoFormatSelector_currentIndexChanged(videoFormatSelector->currentIndex());
  }

  if (recording) {
      recordedFrames = 0;
      recordingTime.start();
      updateRecordingTime();
  } else {
      // Update the elapsed time before invalidating the timer.
      updateRecordingTime();
      recordingTime = QTime();
  }
  if (!error)
    emit recordingStarted(recording);
}

void QArvMainWindow::on_snapshotAction_triggered(bool checked) {
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
  } else {
    QString message = tr("Could not open settings file.");
    statusBar()->showMessage(message, statusTimeoutMsec);
    logMessage() << message;
  }
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
        logMessage() << "Setting failure, wanted:"
                     << wanted << endl
                     << "actual:" << actual;
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
  } else {
    QString message = tr("Could not open camera settings file.");
    statusBar()->showMessage(message, statusTimeoutMsec);
    logMessage() << message;
  }
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

void makeDockAWindow(QDockWidget* dock) {
  // Currently disabled as it causes jerkyness when undocking.
  return;
  dock->setWindowFlags(Qt::Window);
  dock->show();
}

void QArvMainWindow::on_showVideoAction_toggled(bool checked) {
  videodock->setVisible(checked);
}

void QArvMainWindow::on_videodock_visibilityChanged(bool visible) {
  showVideoAction->blockSignals(true);
  showVideoAction->setChecked(!videodock->isHidden());
  showVideoAction->blockSignals(false);
}

void QArvMainWindow::on_videodock_topLevelChanged(bool floating) {
  if (floating) makeDockAWindow(videodock);
}

void QArvMainWindow::on_showHistogramAction_toggled(bool checked) {
  histogramdock->setVisible(checked);
}

void QArvMainWindow::on_histogramdock_visibilityChanged(bool visible) {
  showHistogramAction->blockSignals(true);
  showHistogramAction->setChecked(!histogramdock->isHidden());
  showHistogramAction->blockSignals(false);
}

void QArvMainWindow::on_histogramdock_topLevelChanged(bool floating) {
  if (floating) makeDockAWindow(histogramdock);
}

void QArvMainWindow::on_messageAction_toggled(bool checked) {
  messageDock->setVisible(checked);
}

void QArvMainWindow::on_messageDock_visibilityChanged(bool visible) {
  messageAction->blockSignals(true);
  messageAction->setChecked(!messageDock->isHidden());
  messageAction->blockSignals(false);
}

void QArvMainWindow::on_messageDock_topLevelChanged(bool floating) {
  if (floating) makeDockAWindow(messageDock);
}

void QArvMainWindow::on_closeFileAction_triggered(bool checked) {
  // This function assumes on_recordAction_toggled(false) was
  // done before calling it.
  do {
    QApplication::processEvents();
  } while (workthread->isBusy());
  recorder.reset();
  timestampFile.close();
  closeFileAction->setEnabled(recording);
  foreach (auto wgt, toDisableWhenRecording) {
    wgt->setEnabled(!recording);
    on_videoFormatSelector_currentIndexChanged(videoFormatSelector->currentIndex());
  }
  foreach (auto wgt, toDisableWhenPlaying) {
    wgt->setEnabled(!started);
  }
  pixelFormatSelector->setEnabled(pixelFormatSelector->count() > 1
                                  && !started);
  recordingTimeCumulative = 0;
  recordingTimeLabel->setText(tr("Recording stopped"));
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
  saved_widgets["qarv_recording/write_info"] = recordInfoCheck;
  saved_widgets["qarv_recording/write_timestamps"] = recordTimestampsCheck;
  saved_widgets["qarv_recording/dump_camera_settings"] = recordMetadataCheck;
  saved_widgets["qarv_recording/stop_manually"] = stopRecordingManuallyRadio;
  saved_widgets["qarv_recording/stop_frames"] = stopRecordingFramesRadio;
  saved_widgets["qarv_recording/stop_time"] = stopRecordingTimeRadio;
  saved_widgets["qarv_recording/stop_frames_value"] = stopRecordingFrames;
  saved_widgets["qarv_recording/stop_time_value"] = stopRecordingTime;

  // display widgets
  saved_widgets["qarv_videodisplay/actual_size"] = unzoomButton;
  saved_widgets["qarv_histogram/logarithmic"] = histogramLog;
}

void QArvMainWindow::saveProgramSettings() {
  QSettings settings;

  // main window geometry and state
  for (QDockWidget* wgt: findChildren<QDockWidget*>("Filter settings widget")) {
    wgt->close();
  }
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
    else if (auto *w = qobject_cast<QTimeEdit*>(widget))
      settings.setValue(i.key(), w->time());
    else
      logMessage() << "FIXME: don't know what to save under setting" << i.key();
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
    else if (auto* w = qobject_cast<QTimeEdit*>(widget))
      w->setTime(data.toTime());
    else
      logMessage() << "FIXME: don't know how to restore setting" << i.key();
  }
}

void QArvMainWindow::on_videoFormatSelector_currentIndexChanged(int i) {
  auto fmt = qvariant_cast<OutputFormat*>(videoFormatSelector->itemData(i));
  if (fmt) {
    bool b = !recording && !closeFileAction->isEnabled();
    recordInfoCheck->setEnabled(fmt->canWriteInfo() && b);
  } else {
    logMessage() << "Video format pointer is not an OutputFormat plugin";
    statusBar()->showMessage(tr("Cannot select this video format."));
  }
}

void QArvMainWindow::on_postprocRemoveButton_clicked(bool checked) {
  auto item = postprocChain.itemFromIndex(postprocList->currentIndex());
  if (!item)
    return;
  auto editor = var2ptr<ImageFilterSettingsDialog>(item->data(Qt::UserRole + 2));
  if (editor) {
    editor->close();
    editor->deleteLater();
  }
  // Once the row is removed, the filter itself will be deleted
  // when all its shared pointers are gone.
  postprocChain.removeRow(postprocList->currentIndex().row());
}

void QArvMainWindow::on_postprocList_doubleClicked(const QModelIndex& index) {
  auto item = postprocChain.itemFromIndex(index);
  auto editor = var2ptr<ImageFilterSettingsDialog>(item->data(Qt::UserRole + 2));
  if (!editor) {
    auto filter = var2ptr<ImageFilter>(item->data(Qt::UserRole + 1));
    editor = new ImageFilterSettingsDialog(filter->createSettingsWidget());
    editor->setWindowTitle(item->text());
    item->setData(ptr2var(editor), Qt::UserRole + 2);
    editor->setObjectName("Filter settings widget");
    addDockWidget(Qt::RightDockWidgetArea, editor);
    editor->setFloating(true);
  }
  editor->show();
  editor->setFocus();
}

void QArvMainWindow::updateRecordingTime()
{
  if (!recordingTime.isNull()) {
    static const QChar zero('0');
    const QString txt(tr("Recording time: %1:%2:%3"));
    recordingTimeCumulative += recordingTime.restart();
    int ms = recordingTimeCumulative;
    int s = ms / 1000;
    ms -= s * 1000;
    int m = s / 60;
    s -= m * 60;
    int h = m / 60;
    h -= m * 60;
    QString msg = txt.arg(h, 2, 10, zero)
                     .arg(m, 2, 10, zero)
                     .arg(s, 2, 10, zero);
    qint64 fs = 0;
    if (recorder)
      fs = recorder->fileSize();
    if (fs != 0) {
        const QString txt2(tr("size %1 Mb"));
        msg += ", " + txt2.arg(fs / 1024 / 1024);
    }
    recordingTimeLabel->setText(msg);
    QTimer::singleShot(1000, this, SLOT(updateRecordingTime()));
    QTime elapsed(h, m, s, 99);
    if (recording &&
        stopRecordingTimeRadio->isChecked() &&
        elapsed >= stopRecordingTime->time()) {
      recordAction->setChecked(false);
      closeFileAction->trigger();
    }
  }
}

void QArvMainWindow::bufferUnderrunOccured()
{
    QString msg = tr("Buffer underrun!");
    logMessage() << msg;
    statusBar()->showMessage(msg, statusTimeoutMsec);
}

void QArvMainWindow::addPostprocFilter()
{
  auto action = qobject_cast<QAction*>(sender());
  QString label = action->text();
  QString labelFmt = label + " %1";
  int count = 2;
  while (!postprocChain.findItems(label).isEmpty()) {
    label = labelFmt.arg(count++);
  }
  auto item = new QStandardItem(label);
  item->setFlags(Qt::ItemIsDragEnabled
                 | Qt::ItemIsSelectable
                 | Qt::ItemIsUserCheckable
                 | Qt::ItemIsEnabled);
  item->setCheckState(Qt::Unchecked);
  auto plugin = action->data().value<ImageFilterPlugin*>();
  auto filter = plugin->makeFilter();
  filter->restoreSettings();
  item->setData(ptr2var(filter), Qt::UserRole + 1);
  item->setData(ptr2var<ImageFilterSettingsDialog>(NULL), Qt::UserRole + 2);
  postprocChain.appendRow(item);
}

void QArvMainWindow::updatePostprocQList() {
  auto oldList = postprocChainAsList;
  decltype(oldList) newList;
  for (int row = 0, rows = postprocChain.rowCount(); row < rows; ++row) {
    auto item = postprocChain.item(row);
    auto filter = var2ptr<ImageFilter>(item->data(Qt::UserRole + 1));
    filter->setEnabled(item->checkState() == Qt::Checked);
    auto existing = ::std::find(oldList.begin(), oldList.end(), filter);
    if (existing != oldList.end()) {
      // Keep existing shared pointer.
      newList << *existing;
    } else {
      // This is a new filter, allocate a new shared pointer.
      newList << ImageFilterPtr(filter);
    }
  }
  // This will delete all filters that have disappeared from
  // the postprocChain. Because we use shared pointers, any
  // that are still used by the filter will remain while the
  // filter needs them.
  postprocChainAsList = newList;
}

void QArvMainWindow::stopAllAcquisition() {
  on_playButton_toggled(false);
  on_recordAction_toggled(false);
  on_closeFileAction_triggered(true);
}
