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
#include "api/qarvcamera.h"
#include "api/qarvdecoder.h"

#include <QTimer>
#include <QFile>
#include <QTransform>

class QArvGui;

namespace QArv
{

class QArvMainWindow : public QMainWindow, private Ui::MainWindowUI {
  Q_OBJECT

public:
  QArvMainWindow(QWidget* parent = 0, bool standalone = true);
  ~QArvMainWindow();

signals:
  void recordingStarted(bool started);

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
  void on_chooseSnappathButton_clicked(bool checked);
  void on_chooseFilenameButton_clicked(bool checked);
  void on_fpsSpinbox_valueChanged(int value);
  void on_gainSlider_valueChanged(int value);
  void on_exposureSlider_valueChanged(int value);
  void on_resetROIButton_clicked(bool clicked);
  void on_applyROIButton_clicked(bool clicked);
  void on_binSpinBox_valueChanged(int value);
  void on_saveSettingsButton_clicked(bool checked);
  void on_loadSettingsButton_clicked(bool checked);
  void on_editExposureButton_clicked(bool checked);
  void on_editGainButton_clicked(bool checked);
  void on_exposureSpinbox_editingFinished();
  void on_gainSpinbox_editingFinished();
  void on_videodock_visibilityChanged(bool visible);
  void on_videodock_topLevelChanged(bool floating);
  void on_histogramdock_visibilityChanged(bool visible);
  void on_histogramdock_topLevelChanged(bool floating);
  void on_closeFileButton_clicked(bool checked);
  void on_videoFormatSelector_currentIndexChanged(int index);
  void on_ROIsizeCombo_newSizeSelected(QSize size);
  void on_sliderUpdateSpinbox_valueChanged(int i);
  void on_histogramUpdateSpinbox_valueChanged(int i);
  void on_statusTimeoutSpinbox_valueChanged(int i);
  void pickedROI(QRect roi);
  void readExposure();
  void readGain();
  void startVideo(bool start);
  void takeNextFrame();
  void updateBandwidthEstimation();
  void closeEvent(QCloseEvent* event);
  void updateImageTransform();
  void showFPS();
  void histogramNextFrame();
  void readAllValues();
  void setupListOfSavedWidgets();
  void saveProgramSettings();
  void restoreProgramSettings();

private:
  void readROILimits();
  void transformImage(QImage& img);
  void getNextFrame(QImage* processed,
                    QImage* unprocessed,
                    QByteArray* raw,
                    ArvBuffer** rawAravisBuffer,
                    bool nocopy = false);

  QImage invalidImage;
  QArvCamera* camera;
  QArvDecoder* decoder;
  QRect roirange, roidefault;
  QPair<double, double> gainrange, exposurerange;
  QTimer* autoreadexposure;
  QTimer* autoreadhistogram;
  bool playing, recording, started, drawHistogram, standalone;
  QIODevice* recordingfile;
  QTransform imageTransform;
  uint framecounter;
  QByteArray oldstate, oldgeometry;
  QSize oldsize;
  QList<QWidget*> toDisableWhenPlaying;
  QList<QWidget*> toDisableWhenRecording;
  QIcon recordIcon, pauseIcon, playIcon;
  int statusTimeoutMsec;
  QMap<QString, QWidget*> saved_widgets;

  friend class ::QArvGui;
};

/* Qt event filter that intercepts ToolTipChange events and replaces the
 * tooltip with a rich text representation if needed. This assures that Qt
 * can word-wrap long tooltip messages. Tooltips longer than the provided
 * size threshold (in characters) are wrapped. Only effective if the widget's
 * ancestors include a QArvMainWindow.
 */
class ToolTipToRichTextFilter : public QObject {
  Q_OBJECT

public:
  ToolTipToRichTextFilter(int size_threshold, QObject* parent);

protected:
  bool eventFilter(QObject* obj, QEvent* evt);

private:
  int size_threshold;
};

}

#endif
