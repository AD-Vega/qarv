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

#include "decoders/swscaledecoder.h"
#include <cstdlib>
#include <opencv2/core/types_c.h>
#include <QDebug>

extern "C" {
#include <libavutil/pixdesc.h>
}

using namespace QArv;

#if Q_BYTE_ORDER == Q_BIG_ENDIAN
#define BUFFER_PIXFMT PIX_FMT_RGB48BE
#else
#define BUFFER_PIXFMT PIX_FMT_RGB48LE
#endif

SwScaleDecoder::SwScaleDecoder(QSize size_, PixelFormat inputPixfmt_) :
  inputPixfmt(inputPixfmt_), size(size_),
  image_pointers { 0, 0, 0 }, image_strides { 0, 0, 0 } {
  if (size.width() != (size.width() / 2) * 2
      || size.height() != (size.height() / 2) * 2) {
    qDebug() << "Frame size must be factor of two for SwScaleDecoder.";
    OK = false;
    return;
  }
  if (sws_isSupportedInput(inputPixfmt) > 0) {
    OK = true;
    image_strides[0] = 3 * sizeof(uint16_t) * size.width();
    buffer = new uint16_t[size.width()*size.height()*3];
    image_pointers[0] = reinterpret_cast<uint8_t*>(buffer);
    ctx = sws_getContext(size.width(), size.height(), inputPixfmt,
                         size.width(), size.height(), BUFFER_PIXFMT,
                         SWS_BICUBIC, 0, 0, 0);
  } else {
    qDebug() << "Pixel format" << av_get_pix_fmt_name(inputPixfmt)
             << "is not supported for input.";
    OK = false;
  }
}

SwScaleDecoder::~SwScaleDecoder() {
  if (OK) {
    sws_freeContext(ctx);
    delete[] buffer;
  }
}

void SwScaleDecoder::decode(QByteArray frame) {
  if (!OK) return;
  auto dataptr = reinterpret_cast<const uint8_t*>(frame.constData());
  int calculatedSize =
    avpicture_fill(&srcInfo, const_cast<uint8_t*>(dataptr),
                   inputPixfmt, size.width(), size.height());
  assert(calculatedSize == frame.size());
  int outheight = sws_scale(ctx, srcInfo.data, srcInfo.linesize,
                            0, size.height(),
                            image_pointers, image_strides);
  if(outheight != size.height()) {
    qDebug() << "swscale error! outheight =" << outheight;
  }
}

QImage SwScaleDecoder::getQImage() {
  if (!OK) return QImage();
  QImage out(size, QImage::Format_RGB888);
  int width = size.width(), height = size.height();
  for (int i = 0; i < height; i++) {
    auto l = out.scanLine(i);
    for (int j = 0; j < width; j++) {
      for (int px = 0; px < 3; px++) {
        l[3*j + px] = buffer[3*(i*width + j) + px] >> 8;
      }
    }
  }
  return out;
}

cv::Mat SwScaleDecoder::getCvImage() {
  if (!OK) return cv::Mat();
  cv::Mat M(size.height(), size.width(), CV_16UC3);
  for(int i = 0; i < M.rows; i++) {
    auto Mr = M.ptr<cv::Vec<uint16_t, 3>>(i);
    for (int j = 0; j < M.cols; j++) {
      auto& bgr = Mr[j];
      for (int px = 0; px < 3; px++)
        bgr[px] = buffer[3*(i*M.cols + j) + (2 - px)];
    }
  }
  return M;
}
