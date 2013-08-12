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

#include "utils/qarv_videoplayer.h"
#include <QFileDialog>

QArvVideoPlayer::QArvVideoPlayer(QString filename,
                                 QWidget* parent,
                                 Qt::WindowFlags f) : QWidget(parent, f) {
  setupUi(this);
  showTimer = new QTimer(this);
  connect(showTimer, SIGNAL(timeout()), SLOT(showNextFrame()));
  if (!filename.isNull())
    open(filename);
}

bool QArvVideoPlayer::open(QString filename) {
  if (playButton->isChecked())
    playButton->setChecked(false);
  playButton->setEnabled(false);
  recording.reset(new QArvRecordedVideo(filename));
  if (!recording->status())
    return false;

  decoder.reset(recording->makeDecoder());
  slider->blockSignals(true);
  slider->setEnabled(recording->isSeekable());
  slider->setMaximum(recording->numberOfFrames());
  slider->setValue(0);
  slider->blockSignals(false);
  on_slider_valueChanged(0);
  fpsSpinbox->setValue(recording->framerate());

  playButton->setEnabled(true);
  return true;
}

void QArvVideoPlayer::on_playButton_toggled(bool checked) {
  if (checked) {
    readNextFrame();
    showTimer->setInterval(1000 / fpsSpinbox->value());
    showTimer->start();
  } else {
    showTimer->stop();
  }
}

void QArvVideoPlayer::on_openButton_clicked(bool checked) {
  QString filter = tr("qarv video description (*.qarv);;All file types (*.*)");
  auto name = QFileDialog::getOpenFileName(this, tr("Open file"), QString(), filter);
  if (!name.isNull())
    open(name);
}

void QArvVideoPlayer::readNextFrame() {
  auto frame = recording->read();
  slider->blockSignals(true);
  slider->setValue(slider->value()+1);
  slider->blockSignals(false);
  if (frame.isNull()) {
    playButton->setChecked(false);
    videoWidget->setImage();
  } else {
    decoder->decode(frame);
    QArvDecoder::CV2QImage(decoder->getCvImage(), *(videoWidget->unusedFrame()));
  }
}

void QArvVideoPlayer::showNextFrame() {
  videoWidget->swapFrames();
  if (playButton->isChecked()) {
    showTimer->setInterval(1000 / fpsSpinbox->value());
    QTimer::singleShot(0, this, SLOT(readNextFrame()));
  }
}

void QArvVideoPlayer::on_slider_valueChanged(int value) {
  recording->seek(value);
  readNextFrame();
  showNextFrame();
}

int main(int argc, char** argv) {
  QString filename;
  if (argc > 1)
    filename = argv[1];
  QApplication a(argc, argv);
  QArvVideoPlayer p(filename);
  p.show();
  return a.exec();
}
