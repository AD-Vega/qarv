/*
    <one line to give the program's name and a brief idea of what it does.>
    Copyright (C) 2012  Jure Varlec <jure.varlec@gmail.com>

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

#include "getmtu_linux.h"

const int slidersteps = 1000;
const int sliderUpdateMsec = 300;

MainWindow::MainWindow():
  QMainWindow(), camera(NULL), playing(false), recording(false),
  started(false), recordingfile(NULL), decoder(NULL) {
  setupUi(this);

  QIcon idle = QIcon::fromTheme("video-display");
  idleImage = idle.pixmap(video->size()).toImage();
  video->setImage(idleImage);

  autoreadexposure = new QTimer(this);
  autoreadgain = new QTimer(this);
  autoreadexposure->setInterval(sliderUpdateMsec);
  autoreadgain->setInterval(sliderUpdateMsec);
  this->connect(autoreadexposure, SIGNAL(timeout()), SLOT(readExposure()));
  this->connect(autoreadgain, SIGNAL(timeout()), SLOT(readGain()));

  QSpinBox* boxen[] = {xSpinbox, ySpinbox, wSpinbox, hSpinbox};
  for (int i=0; i<sizeof(boxen)/sizeof(void*); i++)
    this->connect(boxen[i], SIGNAL(valueChanged(int)), SLOT(setROI()));

  auto ifaces = QNetworkInterface::allInterfaces();
  for (int i=0; i<ifaces.length(); i++) {
    ifaceSelector->addItem(ifaces.at(i).name());
  }
  ifaceSelector->setCurrentIndex(0);

  auto cameras = ArCam::listCameras();
  foreach (auto cam, cameras) {
    QString display;
    display = display + cam.vendor + " (" + cam.model + ")";
    cameraSelector->addItem(display, QVariant::fromValue<ArCamId>(cam));
  }
  cameraSelector->setCurrentIndex(0);
}

void MainWindow::on_unzoomButton_toggled(bool checked) {
  if (checked) {
    QSize newsize = video->getImage().size();
    video->setFixedSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
    video->resize(newsize);
    video->setFixedSize(newsize);
  } else {
    video->setFixedSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
    video->setMinimumSize(idleImage.size());
  }
}

static inline double slider2value_log(int slidervalue,
                                      QPair<double, double>& range) {
  double value = log2(range.second) - log2(range.first);
  return exp2(value * slidervalue / slidersteps + log2(range.first));
}

static inline int value2slider_log(double value,
                                   QPair<double, double>& range) {
  return slidersteps *
         (log2(value) - log2(range.first)) /
         (log2(range.second) - log2(range.first));
}

static inline double slider2value(int slidervalue,
                                      QPair<double, double>& range) {
  return range.first + (range.second - range.first) *
         slidervalue / slidersteps;
}

static inline int value2slider(double value,
                                   QPair<double, double>& range) {
  return (value - range.first) / (range.second - range.first) * slidersteps;
}

void MainWindow::on_cameraSelector_currentIndexChanged(int index) {
  autoreadexposure->stop();
  autoreadgain->stop();

  auto camid = cameraSelector->itemData(index).value<ArCamId>();
  if (camera != NULL) {
    startVideo(false);
    delete camera;
  }
  camera = new ArCam(camid);
  this->connect(camera, SIGNAL(frameReady()), SLOT(takeNextFrame()));

  fpsSpinbox->setValue(camera->getFPS());

  if (camera->getMTU() == 0) {
    matchMtuButton->setEnabled(false);
    matchMtuButton->setText("Unsupported");
    ifaceSelector->setEnabled(false);
    mtuSpinbox->setEnabled(false);
  }

  auto formats = camera->getPixelFormats();
  auto formatnames = camera->getPixelFormatNames();
  int noofframes = formats.length();
  pixelFormatSelector->clear();
  for (int i=0; i<noofframes; i++)
    pixelFormatSelector->addItem(formatnames.at(i), formats.at(i));
  auto format = camera->getPixelFormat();
  pixelFormatSelector->setCurrentIndex(pixelFormatSelector->findData(format));
  pixelFormatSelector->setEnabled(noofframes > 1);

  auto roisize = camera->getROIMaxSize();
  roirange = QRect(QPoint(0,0), roisize.size());
  auto roi = camera->getROI();
  xSpinbox->setRange(0, roisize.width());
  xSpinbox->setValue(roi.x());
  ySpinbox->setRange(0, roisize.height());
  ySpinbox->setValue(roi.y());
  wSpinbox->setRange(roisize.x(), roisize.width());
  wSpinbox->setValue(roi.width());
  hSpinbox->setRange(roisize.y(), roisize.height());
  hSpinbox->setValue(roi.height());

  gainrange = camera->getGainLimits();
  exposurerange = camera->getExposureLimits();
  gainSlider->setRange(0, slidersteps);
  exposureSlider->setRange(0, slidersteps);
  gainSpinbox->setRange(gainrange.first, gainrange.second);
  exposureSpinbox->setRange(exposurerange.first, exposurerange.second);
  readGain();
  readExposure();

  gainAutoButton->setEnabled(camera->hasAutoGain());
  exposureAutoButton->setEnabled(camera->hasAutoExposure());
  camera->setAutoGain(false);
  camera->setAutoExposure(false);

  autoreadexposure->start();
  autoreadgain->start();
}

void MainWindow::readExposure() {
  exposureSlider->setValue(value2slider_log(camera->getExposure(), exposurerange));
  exposureSpinbox->setValue(camera->getExposure());
}

void MainWindow::readGain() {
  gainSlider->setValue(value2slider(camera->getGain(), gainrange));
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

void MainWindow::on_ifaceSelector_currentIndexChanged(QString iface) {
  mtuSpinbox->setValue(getMTU(iface));
  matchMtuButton->setText("Match MTU");
  matchMtuButton->setEnabled(true);
}

void MainWindow::on_pixelFormatSelector_currentIndexChanged(int index) {
  auto format = pixelFormatSelector->itemData(index).toString();
  camera->setPixelFormat(format);
}

void MainWindow::setROI() {
  QRect ROI(xSpinbox->value(), ySpinbox->value(),
            wSpinbox->value(), hSpinbox->value());
  if (roirange.contains(ROI)) {
    outofrangeLabel->hide();
    camera->setROI(ROI);
  } else {
    outofrangeLabel->show();
  }
}

void MainWindow::takeNextFrame() {
  if (playing || recording) {
    QByteArray frame = camera->getFrame();
    auto img = decoder->decode(frame);
    if (playing) video->setImage(img);
    if (recording) {
      qint64 written = recordingfile->write(frame, frame.size());
      if (written != frame.size())
        qDebug() << "Incomplete write!";
    }
  }
}

void MainWindow::enableNotouchWidgets(bool enabled) {
  static QList<QWidget*> list;
  if (list.isEmpty())
    list << cameraSelector << pixelFormatSelector << fpsSpinbox <<
         xSpinbox << wSpinbox << ySpinbox << hSpinbox;
  foreach (auto wgt, list)
    wgt->setEnabled(enabled);
}

void MainWindow::startVideo(bool start) {
  if (camera != NULL) {
    if (start && !started) {
      if (decoder != NULL) delete decoder;
      decoder = makeFrameDecoder(camera->getPixelFormat(),
                                 camera->getFrameSize());
      if (decoder == NULL)
        qDebug() << "Decoder for" << camera->getPixelFormat() <<
        "doesn't exist!";
      else {
        camera->startAcquisition();
        started = true;
        enableNotouchWidgets(false);
      }
    } else if (started && !playing && !recording) {
      QApplication::processEvents();
      camera->stopAcquisition();
      if (decoder != NULL) delete decoder;
      decoder = NULL;
      started = false;
      enableNotouchWidgets(true);
    }
  }
}

void MainWindow::on_playButton_clicked(bool checked) {
  playing = checked;
  startVideo(checked);
  playing = checked && started;
  playButton->setChecked(playing);
  if (!playing) video->setImage(idleImage);
}

void MainWindow::on_recordButton_clicked(bool checked) {
  if (checked && (recordingfilename != recordingfile->fileName())) {
    recordingfilename = recordingfile->fileName();
    bool open = recordingfile->open(QIODevice::WriteOnly);
    if (!open) {
      recordButton->setChecked(false);
      recordButton->setEnabled(false);
      return;
    }
  }
  recording = checked;
  startVideo(checked);
  recording = checked && started;
  recordButton->setChecked(recording);
  recordingTab->setEnabled(!recording);
  qDebug() << checked << started << recording;
}

void MainWindow::on_filenameEdit_textChanged(QString name) {
  recordButton->setEnabled(true);
  delete recordingfile;
  recordingfile = new QFile(name, this);
  auto info = QFileInfo(*recordingfile);
  auto dir = info.dir();
  recordButton->setEnabled(dir.exists());
}

void MainWindow::on_chooseFilenameButton_clicked(bool checked) {
  auto name = QFileDialog::getOpenFileName(this, "Open file");
  if(!name.isNull()) filenameEdit->setText(name);
}

void MainWindow::on_fpsSpinbox_valueChanged(int value) {
  camera->setFPS(value);
  fpsSpinbox->setValue(camera->getFPS());
}

void MainWindow::on_matchMtuButton_clicked(bool checked) {
  camera->setMTU(mtuSpinbox->value());
  int mtu = camera->getMTU();
  if (mtu != mtuSpinbox->value()) {
    matchMtuButton->setText("Unsupported value");
    matchMtuButton->setEnabled(false);
  }
}
