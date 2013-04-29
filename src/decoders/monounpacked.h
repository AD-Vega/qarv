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

#ifndef MONOUNPACKED_H
#define MONOUNPACKED_H

#include <type_traits>
#include "api/qarvdecoder.h"
#include "decoders/graymap.h"

namespace QArv {

template <typename InputType, uint bitsPerPixel>
class MonoUnpackedDecoder : public QArvDecoder {

  static_assert(sizeof(InputType) <= sizeof(uint16_t),
                "InputType too large.");

private:
  QSize size;
  cv::Mat M;
  static const bool typeIsSigned = std::is_signed<InputType>::value;
  static const uint zeroBits = 8*sizeof(uint16_t) - bitsPerPixel;
  static const uint signedShiftBits = bitsPerPixel - 1;

public:
  MonoUnpackedDecoder(QSize size_) :
    size(size_), M(size_.height(), size_.width(), CV_16U) {}

  void decode(QByteArray frame) {
    const InputType* dta =
      reinterpret_cast<const InputType*>(frame.constData());
    const int h = size.height(), w = size.width();
    for (int i = 0; i < h; i++) {
      auto line = M.ptr<uint16_t>(i);
      for (int j = 0; j < w; j++) {
        uint16_t tmp;
        if (typeIsSigned)
          tmp = dta[i * w + j] + (1<<signedShiftBits);
        else
          tmp = dta[i * w + j];
        line[j] = tmp << (zeroBits);
      }
    }
  }

  QImage getQImage() {
    QImage img(size, QImage::Format_Indexed8);
    img.setColorTable(graymap);
    const int h = size.height(), w = size.width();
    for (int i = 0; i < h; i++) {
      auto line = M.ptr<uint16_t>(i);
      auto I = img.scanLine(i);
      for (int j = 0; j < w; j++)
        I[j] = line[j] >> 8;
    }
    return img;
  }

  cv::Mat getCvImage() {
    return M.clone();
  }
};



}

#endif
