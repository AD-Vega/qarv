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

#ifndef UNSUPPORTED_H
#define UNSUPPORTED_H

#include "api/qarvdecoder.h"
extern "C" {
  #include <arvenums.h>
}

/* This is a decoder class which can specify the required
 * ArvPixelFormat and frame size nothing else.
 * It can be used to record undecoded video.
 * It is not exported as a plugin, but is instantiated by
 * QArvMainWindow when necessary.
 */

namespace QArv {

class Unsupported : public QArvDecoder {
public:
  Unsupported(ArvPixelFormat type_, QSize size):
    type(type_), redImage(size.height(), size.width(), CV_8UC3) {
    redImage = cv::Scalar(0, 0, 255);
  }
  void decode(QByteArray frame) {}
  const cv::Mat getCvImage() { return redImage; }
  int cvType() { return CV_USRTYPE1; }
  ArvPixelFormat pixelFormat() { return type; }
  QByteArray decoderSpecification() { return QByteArray{}; }

private:
  ArvPixelFormat type;
  cv::Mat redImage;
};

}

#endif
