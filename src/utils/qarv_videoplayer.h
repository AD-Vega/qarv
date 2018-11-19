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

#include <gio/gio.h>  // Workaround for gdbusintrospection's use of "signal".
#include "ui_qarv_videoplayer.h"
#include "ui_qarv_videoplayer_rawvideo.h"
#include "api/qarvdecoder.h"
#include "api/qarvrecordedvideo.h"
#include "recorders/recorder.h"
#include <QTimer>

class QArvVideoPlayer : public QWidget, private Ui::VideoPlayer {
    Q_OBJECT

public:
    explicit QArvVideoPlayer(QString filename = QString(),
                             QWidget* parent = 0,
                             Qt::WindowFlags f = 0);

private slots:
    void on_playButton_toggled(bool checked);
    void on_openQArvVideoAction_triggered(bool);
    void on_openRawVideoAction_triggered(bool);
    void on_slider_valueChanged(int value);
    void on_transcodeBox_toggled(bool checked);
    void on_transcodeButton_toggled(bool checked);
    void on_leftMarkButton_clicked(bool checked);
    void on_rightMarkButton_clicked(bool checked);
    void showNextFrame();
    void readNextFrame(bool seeking = false);

private:
    bool handleFileOpening();
    void openQArvVideo(QString name = QString());
    void openRawVideo(QString name = QString());

    QTimer* showTimer;
    QScopedPointer<QArvDecoder> decoder;
    QScopedPointer<QArvRecordedVideo> recording;
    QScopedPointer<QArv::Recorder> recorder;
    int leftFrame, rightFrame;
};

class QArvRawVideoDialog : public QDialog, private Ui::RawVideoDialog {
    Q_OBJECT

public:
    explicit QArvRawVideoDialog(QWidget* parent, QString name);
    QArvRecordedVideo* getVideo();
};

#endif
