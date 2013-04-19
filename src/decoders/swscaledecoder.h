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

#ifndef FFMPEG_DECODER_H
#define FFMPEG_DECODER_H

#include <QSize>
#include <QImage>
#include <opencv2/core/core.hpp>

extern "C" {
#include <libswscale/swscale.h>
}

namespace QArv {

/*
 * This decoder works by first decoding into RGB48 using libswscale, and then
 * copying data into the appropriate container.
 */
class SwScaleDecoder {
public:
  SwScaleDecoder(QSize size, enum PixelFormat inputPixfmt);
  virtual ~SwScaleDecoder();
  void decode(QByteArray frame);
  QImage getQImage();
  cv::Mat getCvImage();

private:
  QSize size;
  struct SwsContext* ctx;
  const uint8_t* channel_pointers[3];
  int channel_strides[3];
  uint8_t* image_pointers[3];
  int image_strides[3];
  uint16_t* buffer;
};

}

#endif
