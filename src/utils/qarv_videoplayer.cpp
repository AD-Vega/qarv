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

#include "utils/qarv_videoplayer.h"
#include "recorders/gstrecorder_implementation.h"
#include "globals.h"
#include <QFileDialog>
#include <QPluginLoader>
#include <QMenu>
extern "C" {
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>
}

using namespace QArv;

QArvVideoPlayer::QArvVideoPlayer(QString filename,
                                 QWidget* parent) : QWidget(parent) {
    setupUi(this);
    QHash<QAbstractButton*, QString> icons;
    icons[openMenuButton] = "document-open";
    icons[playButton] = "media-playback-start";
    icons[transcodeButton] = "media-record";
    icons[leftMarkButton] = "go-first";
    icons[rightMarkButton] = "go-last";
    for (auto i = icons.begin(); i != icons.end(); i++)
        if (!QIcon::hasThemeIcon(*i))
            i.key()->setIcon(QIcon(QString(qarv_datafiles) + *i + ".svgz"));

    auto submenu = new QMenu;
    submenu->addAction(openQArvVideoAction);
    submenu->addAction(openRawVideoAction);
    openMenuButton->setMenu(submenu);

    showTimer = new QTimer(this);
    connect(showTimer, SIGNAL(timeout()), SLOT(showNextFrame()));
    transcodeBox->setEnabled(false);
    transcodeBox->setChecked(false);
    if (!filename.isNull()) {
        if (filename.endsWith(".qarv")) {
            openQArvVideo(filename);
        } else {
            openRawVideo(filename);
        }
    }

    auto plugins = QPluginLoader::staticInstances();
    foreach (auto plugin, plugins) {
        auto fmt = qobject_cast<OutputFormat*>(plugin);
        if (fmt != NULL)
            codecBox->addItem(fmt->name(), QVariant::fromValue(fmt));
    }
    codecBox->addItem("Custom...", QVariant::fromValue((void*)NULL));
    codecBox->setCurrentIndex(0);
}

void QArvVideoPlayer::openQArvVideo(QString name) {
    if (name.isNull()) {
        QString filter = tr("QArv video description (*.qarv)");
        name = QFileDialog::getOpenFileName(this,
                                            tr("Open file"),
                                            QString(), filter);
    }
    if (!name.isNull()) {
        recording.reset(new QArvRecordedVideo(name));
    } else {
        recording.reset(nullptr);
    }
    handleFileOpening();
}

void QArvVideoPlayer::openRawVideo(QString name) {
    QArvRawVideoDialog dialog(this, name);
    dialog.exec();
    if (dialog.result() == QDialog::Accepted) {
        recording.reset(dialog.getVideo());
        handleFileOpening();
    }
}

// Call this when `recording` is reset.
bool QArvVideoPlayer::handleFileOpening() {
    if (playButton->isChecked())
        playButton->setChecked(false);
    playButton->setEnabled(false);

    if (!recording || !recording->status()) {
        transcodeBox->setEnabled(false);
        return false;
    }

    decoder.reset(recording->makeDecoder());
    slider->blockSignals(true);
    slider->setEnabled(recording->isSeekable());
    slider->setMaximum(recording->numberOfFrames() - 1);
    slider->setValue(0);
    on_slider_valueChanged(0);
    slider->blockSignals(false);
    fpsSpinbox->setValue(recording->framerate());

    leftMarkButton->setEnabled(recording->isSeekable());
    rightMarkButton->setEnabled(recording->isSeekable());
    leftMarkButton->setChecked(false);
    rightMarkButton->setChecked(false);
    on_leftMarkButton_clicked(false);
    on_rightMarkButton_clicked(false);

    playButton->setEnabled(true);
    transcodeBox->setEnabled(true);
    return true;
}

void QArvVideoPlayer::on_playButton_toggled(bool checked) {
    if (checked) {
        showTimer->setInterval(1000 / fpsSpinbox->value());
        showTimer->start();
        readNextFrame();
    } else {
        showTimer->stop();
    }
}

void QArvVideoPlayer::on_openQArvVideoAction_triggered(bool) {
    openQArvVideo();
}

void QArvVideoPlayer::on_openRawVideoAction_triggered(bool) {
    openRawVideo();
}

void QArvVideoPlayer::readNextFrame(bool seeking) {
    auto frame = recording->read();
    if (!seeking && slider->isEnabled()) {
        slider->blockSignals(true);
        slider->setValue(slider->value()+1);
        slider->blockSignals(false);
    }
    if (frame.isNull()) {
        playButton->setChecked(false);
        QApplication::processEvents();
        videoWidget->setImage();
        return;
    } else {
        decoder->decode(frame);
        QArvDecoder::CV2QImage(decoder->getCvImage(),
                               *(videoWidget->unusedFrame()));
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
    readNextFrame(true);
    recording->seek(value);
    showNextFrame();
}

void QArvVideoPlayer::on_transcodeBox_toggled(bool checked) {
    foreach (QObject* child, transcodeBox->children()) {
        QWidget* wgt;
        if (NULL != (wgt = qobject_cast<QWidget*>(child)))
            wgt->setVisible(checked);
    }
}

void QArvVideoPlayer::on_leftMarkButton_clicked(bool checked) {
    leftFrame = checked ? slider->value() : 0;
}

void QArvVideoPlayer::on_rightMarkButton_clicked(bool checked) {
    rightFrame = checked ? slider->value() : slider->maximum();
}

void QArvVideoPlayer::on_transcodeButton_toggled(bool checked) {
    if (checked) {
        QString fname = QFileDialog::getSaveFileName(this,
                                                     tr("Destination file"));
        if (fname.isNull()) {
            transcodeButton->setChecked(false);
            return;
        }
        playButton->setChecked(false);
        QApplication::processEvents();
        if (recording->isSeekable()) {
            if (rightFrame < leftFrame) {
                auto tmp = rightFrame;
                rightFrame = leftFrame;
                leftFrame = tmp;
            }
            on_slider_valueChanged(leftFrame);
            transcodeBar->setMinimum(leftFrame);
            transcodeBar->setMaximum(rightFrame);
            transcodeBar->setValue(leftFrame);
        }
        QApplication::processEvents();

        auto fmt =
            qvariant_cast<OutputFormat*>(codecBox->itemData(codecBox->
                                                                currentIndex()));
        if (fmt) {
            recorder.reset(fmt->makeRecorder(decoder.data(), fname,
                                             recording->frameSize(),
                                             fpsSpinbox->value(), false));
        } else if (!gstLine->text().isEmpty()) {
            recorder.reset(makeGstRecorder(QStringList(),
                                           gstLine->text(),
                                           decoder.data(), fname,
                                           recording->frameSize(),
                                           fpsSpinbox->value(), false));
        } else {
            transcodeButton->setChecked(false);
            return;
        }

        slider->setEnabled(false);
        playButton->setChecked(false);
        playButton->setEnabled(false);
        QApplication::processEvents();

        // Work in a loop and abort if the user clicks this button again, which
        // results in deallocating the recorder.
        quint64 counter = 0;
        forever {
            if (!transcodeButton->isChecked())
                break;
            counter++;
            auto frame = recording->read();
            if (frame.isNull()) {
                transcodeButton->setChecked(false);
                break;
            }
            decoder->decode(frame);
            if (recorder->recordsRaw())
                recorder->recordFrame(frame);
            else
                recorder->recordFrame(decoder->getCvImage());
            if (!recorder->isOK()) {
                transcodeButton->setChecked(false);
                break;
            }
            if (counter % 10) {
                transcodeBar->setValue(transcodeBar->value() + 10);
                QApplication::processEvents();
            }
        }
    } else {
        playButton->setEnabled(true);
        QApplication::processEvents();
        transcodeBar->setValue(transcodeBar->minimum());
        recorder.reset();
        if (recording->isSeekable()) {
            slider->setEnabled(true);
            slider->setValue(0);
            on_slider_valueChanged(0);
        }
    }
}

QArvRawVideoDialog::QArvRawVideoDialog(QWidget* parent, QString name) :
    QDialog(parent) {
    setupUi(this);
    if (!QIcon::hasThemeIcon("document-open")) {
        inputFileButton->setIcon(QIcon(QString(qarv_datafiles)
                                       + "document-open.svgz"));
    }
    inputFileEdit->setText(name);

    connect(inputFileButton, &QPushButton::clicked, [this](bool) {
        QString filter = tr("Raw video file (*)");
        QString name = QFileDialog::getOpenFileName(this,
                                                    tr("Open file"),
                                                    QString(), filter);
        if (!name.isNull()) {
            inputFileEdit->setText(name);
        }
    });

    QStringList formats;
    //TODO don't use AV_PIX_FMT_NB once libav is updated
    for (int i = 0; i < AV_PIX_FMT_NB; i++) {
        if (sws_isSupportedInput((enum AVPixelFormat)i))
            formats.append(av_get_pix_fmt_name((enum AVPixelFormat)i));
    }
    formats.sort();
    fmtCombo->clear();
    fmtCombo->addItems(formats);
}

QArvRecordedVideo* QArvRawVideoDialog::getVideo() {
    if (result() == QDialog::Accepted) {
        auto pixfmt = av_get_pix_fmt(fmtCombo->currentText().toLatin1());
        return new QArvRecordedVideo(inputFileEdit->text(), pixfmt,
                                     headerSpin->value(),
                                     QSize(widthSpin->value(),
                                           heightSpin->value()));
    } else {
        return nullptr;
    }
}


int main(int argc, char** argv) {
    QApplication a(argc, argv);
    QString filename;
    if (argc > 1)
        filename = QCoreApplication::arguments().at(1);
    QArvVideoPlayer p(filename);
    p.show();
    return a.exec();
}
