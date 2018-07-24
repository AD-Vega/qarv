/*
    QArv, a Qt interface to aravis.
    Copyright (C) 2014 Jure Varlec <jure.varlec@ad-vega.si>

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

#include "recorders/imagerecorder.h"
#include <opencv2/highgui/highgui.hpp>
#include <QFileInfo>

using namespace QArv;

class ImageRecorder : public Recorder {
public:
    ImageRecorder(QArvDecoder* decoder_, QString baseName_) :
        decoder(decoder_) {
        if (!decoder || baseName_.isEmpty()) {
            return;
        }
        baseName = baseName_
                   +(baseName_[baseName_.size() - 1] == '-' ? "" : "-")
                   + "%1.tiff";
        OK = true;
    }

    bool isOK() { return OK; }

    bool recordsRaw() {
        return false;
    }

    QPair<qint64, qint64> fileSize() {
        return qMakePair(currentSize, currentNumber);
    }

    void recordFrame(cv::Mat decoded) {
        QString file = baseName.arg(currentNumber++, 19, 10, QChar('0'));
        OK = cv::imwrite(file.toStdString(), decoded);
        if (OK)
            currentSize += QFileInfo(file).size();
    }

private:
    bool OK = false;
    QArvDecoder* decoder;
    QString baseName;
    qint64 currentSize = 0;
    qint64 currentNumber = 0;
};

Recorder* ImageFormat::makeRecorder(QArvDecoder* decoder,
                                    QString fileName,
                                    QSize frameSize,
                                    int framesPerSecond,
                                    bool writeInfo) {
    return new ImageRecorder(decoder, fileName);
}

Q_IMPORT_PLUGIN(ImageFormat)
