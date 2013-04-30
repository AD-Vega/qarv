/*
    qarv, a Qt interface to aravis.
    Copyright (C) 2013 Jure Varlec <jure.varlec@ad-vega.si>

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

using namespace QArv;

class RawUndecoded: public Recorder {
public:
  RawUndecoded(QArvDecoder* decoder_,
               QString fileName,
               QSize size,
               int FPS,
               bool appendToFile) :
    file(fileName), decoder(decoder_) {
    file.open(appendToFile ? QIODevice::Append : QIODevice::WriteOnly);
  }

  bool isOK() {
    return file.isOpen() && (file.error() == QFile::NoError);
  }

  void recordFrame(QByteArray raw, bool isDecoded) {
    file.write(raw);
  }

private:
  QFile file;
  QArvDecoder* decoder;
};

class RawDecoded8: public Recorder {
public:
  RawDecoded8(QArvDecoder* decoder_,
              QString fileName,
              QSize size,
              int FPS,
              bool appendToFile) :
    file(fileName), decoder(decoder_) {
    file.open(appendToFile ? QIODevice::Append : QIODevice::WriteOnly);
  }

  bool isOK() {
    return file.isOpen() && (file.error() == QFile::NoError);
  }

  void recordFrame(QByteArray raw, bool isDecoded) {
    if (!isDecoded) decoder->decode(raw);
    QImage img = decoder->getQImage();
    int bytesPerLine;
    if (img.format() == QImage::Format_Indexed8)
      bytesPerLine = img.width();
    else
      bytesPerLine = 3*img.width();
    for (int i = 0; i < img.height(); i++) {
      auto line = reinterpret_cast<const char*>(img.constScanLine(i));
      file.write(line, bytesPerLine);
    }
  }

private:
  QFile file;
  QArvDecoder* decoder;
};

class RawDecoded16: public Recorder {
public:
  RawDecoded16(QArvDecoder* decoder_,
               QString fileName,
               QSize size,
               int FPS,
               bool appendToFile) :
    file(fileName), decoder(decoder_) {
    file.open(appendToFile ? QIODevice::Append : QIODevice::WriteOnly);
  }

  bool isOK() {
    return file.isOpen() && (file.error() == QFile::NoError);
  }

  void recordFrame(QByteArray raw, bool isDecoded) {
    if (!isDecoded) decoder->decode(raw);
    cv::Mat M = decoder->getCvImage();
    // The image is assumed contiguous, as cv::Mat is by default.
    assert(M.isContinuous());
    if (M.channels() == 1) {
      file.write(M.ptr<const char>(), M.total() * M.elemSize());
    } else {
      for(int i = 0; i < M.rows; i++) {
        auto Mr = M.ptr<cv::Vec<uint16_t, 3> >(i);
        for (int j = 0; j < M.cols; j++) {
          const auto& bgr = Mr[j];
          for (int px = 0; px < 3; px++) {
            auto bytes = reinterpret_cast<const char*>(&(bgr[2 - px]));
            file.write(bytes, 2);
          }
        }
      }
    }
  }

private:
  QFile file;
  QArvDecoder* decoder;
};

Recorder* RawUndecodedFormat::makeRecorder(QArvDecoder* decoder,
                                           QString fileName,
                                           QSize frameSize,
                                           int framesPerSecond,
                                           bool appendToFile) {
  return new RawUndecoded(decoder, fileName, frameSize, framesPerSecond, appendToFile);
}

Recorder* RawDecoded8Format::makeRecorder(QArvDecoder* decoder,
                                          QString fileName,
                                          QSize frameSize,
                                          int framesPerSecond,
                                          bool appendToFile) {
  return new RawDecoded8(decoder, fileName, frameSize, framesPerSecond, appendToFile);
}

Recorder* RawDecoded16Format::makeRecorder(QArvDecoder* decoder,
                                           QString fileName,
                                           QSize frameSize,
                                           int framesPerSecond,
                                           bool appendToFile) {
  return new RawDecoded16(decoder, fileName, frameSize, framesPerSecond, appendToFile);
}

Q_EXPORT_PLUGIN2(RawUndecoded, QArv::RawUndecodedFormat)
Q_IMPORT_PLUGIN(RawUndecoded)

Q_EXPORT_PLUGIN2(RawDecoded8, QArv::RawDecoded8Format)
Q_IMPORT_PLUGIN(RawDecoded8)

Q_EXPORT_PLUGIN2(RawDecoded16, QArv::RawDecoded16Format)
Q_IMPORT_PLUGIN(RawDecoded16)
