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

#include "decoders/mono8.h"
#include "decoders/graymap.h"

using namespace QArv;

Mono8Decoder::Mono8Decoder(QSize size_) :
  size(size_), M(size_.height(), size_.width(), CV_8U) {}


void Mono8Decoder::decode(QByteArray frame) {
  memcpy(M.ptr(0), frame.constData(), frame.size());
}

QImage Mono8Decoder::getQImage() {
  QImage img(size, QImage::Format_Indexed8);
  img.setColorTable(graymap);
  char* dta = M.ptr<char>(0);
  const int h = size.height(), w = size.width();
  for (int i = 0; i < h; i++)
    memcpy(img.scanLine(i), dta+i*w, w);
  return img;
}

cv::Mat Mono8Decoder::getCvImage() {
  return M;
}

Q_EXPORT_STATIC_PLUGIN2(Mono8, QArv::Mono8Format)
