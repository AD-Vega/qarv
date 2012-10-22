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


#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "ui_mainwindow.h"

#include "glvideowidget.h"
#include "framedecoders.h"
#include "arcam.h"

#include <QTimer>
#include <QFile>
#include <QTransform>

class MainWindow : public QMainWindow, private Ui::MainWindowUI {
  Q_OBJECT

public:
  MainWindow();
  ~MainWindow();

private slots:
  void on_refreshCamerasButton_clicked(bool clicked = false);
  void on_unzoomButton_toggled(bool checked);
  void on_cameraSelector_currentIndexChanged(int index);
  void on_exposureAutoButton_toggled(bool checked);
  void on_gainAutoButton_toggled(bool checked);
  void on_pixelFormatSelector_currentIndexChanged(int index);
  void on_playButton_clicked(bool checked);
  void on_recordButton_clicked(bool checked);
  void on_snapButton_clicked(bool checked);
  void on_filenameEdit_textChanged(QString name);
  void on_snappathEdit_textChanged();
  void on_chooseSnappathButton_clicked(bool checked);
  void on_chooseFilenameButton_clicked(bool checked);
  void on_fpsSpinbox_valueChanged(int value);
  void on_gainSlider_valueChanged(int value);
  void on_exposureSlider_valueChanged(int value);
  void on_resetROIButton_clicked(bool clicked);
  void on_applyROIButton_clicked(bool clicked);
  void on_binSpinBox_valueChanged(int value);
  void on_dumpSettingsButton_clicked(bool checked);
  void on_editExposureButton_clicked(bool checked);
  void on_editGainButton_clicked(bool checked);
  void on_exposureSpinbox_editingFinished();
  void on_gainSpinbox_editingFinished();
  void on_videodock_visibilityChanged(bool visible);
  void on_videodock_topLevelChanged(bool floating);
  void on_closeFileButton_clicked(bool checked);
  void on_videoFormatSelector_currentIndexChanged(int index);
  void pickedROI(QRect roi);
  void readExposure();
  void readGain();
  void startVideo(bool start);
  void takeNextFrame();
  void updateBandwidthEstimation();
  void closeEvent(QCloseEvent* event);
  void updateImageTransform();
  void showFPS();

private:
  void readROILimits();

  QImage invalidImage;
  ArCam* camera;
  FrameDecoder* decoder;
  QRect roirange, roidefault;
  QPair<double, double> gainrange, exposurerange;
  QTimer* autoreadexposure;
  bool playing, recording, started;
  QIODevice* recordingfile;
  QTransform imageTransform;
  uint framecounter;
  QByteArray currentFrame;
  QByteArray oldstate, oldgeometry;
  QSize oldsize;
  QList<QWidget*> toDisableWhenPlaying;
  QList<QWidget*> toDisableWhenRecording;
};

#endif
