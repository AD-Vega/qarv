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

#include "recorders/rawrecorders.h"
#include <QFile>
#include <QSettings>
#include <QRegExp>
#include <QFileInfo>
#include "globals.h"
extern "C" {
#include <libavutil/pixdesc.h>
}

using namespace QArv;

// Make sure settings format matches qarvrecordedvideo.cpp!

static const QString descExt(".qarv");

static void initDescfile(QSettings& s, QSize size, int FPS) {
    s.beginGroup("qarv_raw_video_description");
    s.remove("");
    s.setValue("description_version", "0.1");
    QFileInfo finfo(s.fileName());
    QString fname(finfo.completeBaseName());
    s.setValue("file_name", fname);
    s.setValue("frame_size", size);
    s.setValue("nominal_fps", FPS);
}

class RawUndecoded : public Recorder {
public:
    RawUndecoded(QArvDecoder* decoder_,
                 QString fileName,
                 QSize size,
                 int FPS,
                 bool writeInfo) :
        file(fileName), decoder(decoder_), bytesizeWritten(false) {
        file.open(QIODevice::WriteOnly);
        if (isOK() && writeInfo) {
            QSettings s(fileName + descExt, QSettings::Format::IniFormat);
            initDescfile(s, size, FPS);
            s.setValue("encoding_type", "aravis");
            auto pxfmt = QString::number(decoder->pixelFormat(), 16);
            s.setValue("arv_pixel_format", QString("0x") + pxfmt);
        }
    }

    bool isOK() {
        return file.isOpen() && (file.error() == QFile::NoError);
    }

    bool recordsRaw() {
        return true;
    }

    void recordFrame(QByteArray raw) {
        if (isOK()) {
            file.write(raw);
            if (!bytesizeWritten) {
                bytesizeWritten = true;
                QSettings s(file.fileName() + descExt,
                            QSettings::Format::IniFormat);
                s.beginGroup("qarv_raw_video_description");
                s.setValue("frame_bytes", raw.size());
                frameBytes = raw.size();
            }
        }
    }

    QPair<qint64, qint64> fileSize() {
        qint64 s, n;
        if (!bytesizeWritten || !frameBytes) {
            s = n = 0;
        } else {
            s = file.size();
            n = s / frameBytes;
        }
        return qMakePair(s, n);
    }

private:
    QFile file;
    QArvDecoder* decoder;
    bool bytesizeWritten;
    qint64 frameBytes;
};

class RawDecoded8 : public Recorder {
public:
    RawDecoded8(QArvDecoder* decoder_,
                QString fileName,
                QSize size,
                int FPS,
                bool writeInfo) :
        file(fileName), decoder(decoder_), OK(true) {
        file.open(QIODevice::WriteOnly);
        if (isOK()) {
            enum AVPixelFormat fmt;
            switch (decoder->cvType()) {
            case CV_8UC1:
            case CV_16UC1:
                fmt = AV_PIX_FMT_GRAY8;
                frameBytes = size.width()*size.height();
                break;

            case CV_8UC3:
            case CV_16UC3:
                fmt = AV_PIX_FMT_BGR24;
                frameBytes = size.width()*size.height()*3;
                break;

            default:
                OK = false;
                logMessage() << "Recorder: Invalid CV image format";
                return;
            }
            if (writeInfo) {
                QSettings s(fileName + descExt, QSettings::Format::IniFormat);
                initDescfile(s, size, FPS);
                s.setValue("encoding_type", "libavutil");
                s.setValue("libavutil_pixel_format", fmt);
                s.setValue("libavutil_pixel_format_name",
                           av_get_pix_fmt_name(fmt));
                s.setValue("frame_bytes", frameBytes);
            }
        }
    }

    bool isOK() {
        return OK && file.isOpen() && (file.error() == QFile::NoError);
    }

    bool recordsRaw() {
        return false;
    }

    void recordFrame(cv::Mat decoded) {
        if (!isOK())
            return;
        int pixPerRow = decoded.cols*decoded.channels();
        if (decoded.depth() == CV_8U) {
            for (int row = 0; row < decoded.rows; ++row) {
                auto ptr = decoded.ptr<uint8_t>(row);
                file.write(reinterpret_cast<char*>(ptr), pixPerRow);
            }
        } else {
            QVector<uint8_t> line(pixPerRow);
            for (int row = 0; row < decoded.rows; ++row) {
                auto ptr = decoded.ptr<uint16_t>(row);
                for (int col = 0; col < pixPerRow; ++col)
                    line[col] = ptr[col] >> 8;
                file.write(reinterpret_cast<const char*>(line.constData()),
                           pixPerRow);
            }
        }
    }

    QPair<qint64, qint64> fileSize() {
        qint64 s = file.size();
        qint64 n = s / frameBytes;
        return qMakePair(s, n);
    }

private:
    QFile file;
    QArvDecoder* decoder;
    bool OK;
    qint64 frameBytes;
};

class RawDecoded16 : public Recorder {
public:
    RawDecoded16(QArvDecoder* decoder_,
                 QString fileName,
                 QSize size,
                 int FPS,
                 bool writeInfo) :
        file(fileName), decoder(decoder_), OK(true) {
        file.open(QIODevice::WriteOnly);
        if (isOK()) {
            enum AVPixelFormat fmt;
            switch (decoder->cvType()) {
            case CV_8UC1:
            case CV_16UC1:
                fmt = AV_PIX_FMT_GRAY16;
                frameBytes = size.width()*size.height()*2;
                break;

            case CV_8UC3:
            case CV_16UC3:
                fmt = AV_PIX_FMT_BGR48;
                frameBytes = size.width()*size.height()*6;
                break;

            default:
                OK = false;
                logMessage() << "Recorder: Invalid CV image format";
                return;
            }
            if (writeInfo) {
                QSettings s(fileName + descExt, QSettings::Format::IniFormat);
                initDescfile(s, size, FPS);
                s.setValue("encoding_type", "libavutil");
                s.setValue("libavutil_pixel_format", fmt);
                s.setValue("libavutil_pixel_format_name",
                           av_get_pix_fmt_name(fmt));
                s.setValue("frame_bytes", frameBytes);
            }
        }
    }

    bool isOK() {
        return OK && file.isOpen() && (file.error() == QFile::NoError);
    }

    bool recordsRaw() {
        return false;
    }

    void recordFrame(cv::Mat decoded) {
        if (!isOK())
            return;
        int pixPerRow = decoded.cols * decoded.channels();
        if (decoded.depth() == CV_16U) {
            for (int row = 0; row < decoded.rows; ++row) {
                auto ptr = decoded.ptr<uint16_t>(row);
                file.write(reinterpret_cast<char*>(ptr), pixPerRow*2);
            }
        } else {
            QVector<uint16_t> line(pixPerRow);
            for (int row = 0; row < decoded.rows; ++row) {
                auto ptr = decoded.ptr<uint8_t>(row);
                for (int col = 0; col < pixPerRow; ++col)
                    line[col] = ptr[col] << 8;
                file.write(reinterpret_cast<const char*>(line.constData()),
                           pixPerRow*2);
            }
        }
    }

    QPair<qint64, qint64> fileSize() {
        qint64 s = file.size();
        qint64 n = s / frameBytes;
        return qMakePair(s, n);
    }

private:
    QFile file;
    QArvDecoder* decoder;
    bool OK;
    qint64 frameBytes;
};

Recorder* RawUndecodedFormat::makeRecorder(QArvDecoder* decoder,
                                           QString fileName,
                                           QSize frameSize,
                                           int framesPerSecond,
                                           bool writeInfo) {
    return new RawUndecoded(decoder,
                            fileName,
                            frameSize,
                            framesPerSecond,
                            writeInfo);
}

Recorder* RawDecoded8Format::makeRecorder(QArvDecoder* decoder,
                                          QString fileName,
                                          QSize frameSize,
                                          int framesPerSecond,
                                          bool writeInfo) {
    return new RawDecoded8(decoder,
                           fileName,
                           frameSize,
                           framesPerSecond,
                           writeInfo);
}

Recorder* RawDecoded16Format::makeRecorder(QArvDecoder* decoder,
                                           QString fileName,
                                           QSize frameSize,
                                           int framesPerSecond,
                                           bool writeInfo) {
    return new RawDecoded16(decoder,
                            fileName,
                            frameSize,
                            framesPerSecond,
                            writeInfo);
}

Q_IMPORT_PLUGIN(RawUndecodedFormat)
Q_IMPORT_PLUGIN(RawDecoded8Format)
Q_IMPORT_PLUGIN(RawDecoded16Format)
