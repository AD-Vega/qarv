/*
    QArv, a Qt interface to aravis.
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

#ifndef VIDEOPLAYER_H
#define VIDEOPLAYER_H

#include "ui_qarv_videoplayer.h"
#include "api/qarvdecoder.h"
#include "api/qarvrecordedvideo.h"
#include "recorders/recorder.h"
#include <QTimer>

class QArvVideoPlayer: public QWidget, private Ui::VideoPlayer {
  Q_OBJECT

public:
  explicit QArvVideoPlayer(QString filename = QString(),
                           QWidget* parent = 0,
                           Qt::WindowFlags f = 0);
  bool open(QString filename);

private slots:
  void on_playButton_toggled(bool checked);
  void on_openButton_clicked(bool checked);
  void on_slider_valueChanged(int value);
  void on_transcodeBox_toggled(bool checked);
  void on_transcodeButton_toggled(bool checked);
  void on_leftMarkButton_clicked(bool checked);
  void on_rightMarkButton_clicked(bool checked);
  void showNextFrame();
  void readNextFrame(bool seeking = false);

private:
  QTimer* showTimer;
  QScopedPointer<QArvDecoder> decoder;
  QScopedPointer<QArvRecordedVideo> recording;
  QScopedPointer<QArv::Recorder> recorder;
  int leftFrame, rightFrame;
};

#endif
