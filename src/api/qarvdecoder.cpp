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

#include "api/qarvdecoder.h"
#include "decoders/swscaledecoder.h"
#include "decoders/graymap.h"
#include <opencv2/imgproc/imgproc.hpp>
#include <QPluginLoader>
#include <QMap>
#include <QDebug>
extern "C" {
  #include <arvenums.h>
}

void QArvDecoder::CV2QImage_RGB24(const cv::Mat& image, QImage& img) {
  if (image.depth() == CV_8U) {
    //TODO
    img = QImage();
    qDebug() << "Rendering from 8-bit image not implemented yet";
    return;
  }
  const int h = image.rows, w = image.cols;
  QSize s = img.size();
  if (s.height() != h
      || s.width() != w
      || img.format() != QImage::Format_RGB888) {
    if (image.channels() == 1)
      img = QImage(w, h, QImage::Format_Indexed8);
    else
      img = QImage(w, h, QImage::Format_RGB888);
  }
  if (image.channels() == 1) {
    img.setColorTable(graymap);
    for (int i = 0; i < h; i++) {
      auto line = image.ptr<uint16_t>(i);
      auto I = img.scanLine(i);
      for (int j = 0; j < w; j++)
        I[j] = line[j] >> 8;
    }
  } else {
    for (int i = 0; i < h; i++) {
      auto imgLine = img.scanLine(i);
      auto imageLine = image.ptr<cv::Vec<uint16_t, 3> >(i);
      for (int j = 0; j < w; j++) {
        auto& bgr = imageLine[j];
        for (int px = 0; px < 3; px++) {
          imgLine[3*j + px] = bgr[2-px] >> 8;
        }
      }
    }
  }
}

QImage QArvDecoder::CV2QImage_RGB24(const cv::Mat& image) {
  QImage img;
  CV2QImage_RGB24(image, img);
  return img;
}

void QArvDecoder::CV2QImage(const cv::Mat& image_, QImage& image) {
  if (image.depth() == CV_8U) {
    //TODO
    image = QImage();
    qDebug() << "Rendering from 8-bit image not implemented yet";
    return;
  }
  const int h = image_.rows, w = image_.cols;
  QSize s = image.size();
  if (s.height() != h
      || s.width() != w
      || image.format() != QImage::Format_ARGB32_Premultiplied)
    image = QImage(w, h, QImage::Format_ARGB32_Premultiplied);
  if (image_.channels() == 3) {
    for (int i = 0; i < h; i++) {
      auto imgLine = image.scanLine(i);
      auto imageLine = image_.ptr<cv::Vec<uint16_t, 3> >(i);
      for (int j = 0; j < w; j++) {
        auto& bgr = imageLine[j];
        for (int px = 0; px < 3; px++) {
          imgLine[4*j + px] = bgr[2-px] >> 8;
        }
        imgLine[4*j + 3] = 255;
      }
    }
  } else {
    for (int i = 0; i < h; i++) {
      auto imgLine = image.scanLine(i);
      auto imageLine = image_.ptr<uint16_t>(i);
      for (int j = 0; j < w; j++) {
        uint8_t gray = imageLine[j] >> 8;
        for (int px = 0; px < 3; px++) {
          imgLine[4*j + px] = gray;
        }
        imgLine[4*j + 3] = 255;
      }
    }
  }
}

QImage QArvDecoder::CV2QImage(const cv::Mat& image) {
  QImage img;
  CV2QImage(image, img);
  return img;
}

QList<QArvPixelFormat*> initPluginFormats() {
  QList<QArvPixelFormat*> list;
  auto plugins = QPluginLoader::staticInstances();
  foreach (auto plugin, plugins) {
    auto fmt = qobject_cast<QArvPixelFormat*>(plugin);
    if (fmt != NULL) list << fmt;
  }
  return list;
}

QMap<ArvPixelFormat, enum PixelFormat> initSwScaleFormats();

// List of formats supported by plugins.
static QList<QArvPixelFormat*> pluginFormats = initPluginFormats();

// List of formats supported by libswscale, with mappings to
// appropriate ffmpeg formats.
QMap<ArvPixelFormat, enum PixelFormat> swScaleFormats = initSwScaleFormats();

QList<ArvPixelFormat> QArvPixelFormat::supportedFormats() {
  QList<ArvPixelFormat> list;
  foreach (auto fmt, pluginFormats) {
    list << fmt->pixelFormat();
  }
  list << swScaleFormats.keys();
  return list;
}

/*!
 * Returns NULL if the format is not supported.
 */
QArvDecoder* QArvPixelFormat::makeDecoder(ArvPixelFormat format, QSize size) {
  foreach (auto fmt, pluginFormats) {
    if (fmt->pixelFormat() == format) return fmt->makeDecoder(size);
  }
  foreach (auto arvfmt, swScaleFormats.keys()) {
    if (arvfmt == format)
      return new QArv::SwScaleDecoder(size, swScaleFormats[arvfmt], arvfmt);
  }
  return NULL;
}

/*!
 * Returns NULL if the format is not supported.
 */
QArvDecoder* QArvPixelFormat::makeSwScaleDecoder(PixelFormat fmt, QSize size) {
  return new QArv::SwScaleDecoder(size, fmt, 0);
}

QMap<ArvPixelFormat, PixelFormat> initSwScaleFormats() {
  QMap<ArvPixelFormat, PixelFormat> m;

  m[ARV_PIXEL_FORMAT_YUV_422_PACKED] = PIX_FMT_UYVY422;
  m[ARV_PIXEL_FORMAT_RGB_8_PACKED] = PIX_FMT_RGB24;
  m[ARV_PIXEL_FORMAT_BGR_8_PACKED] = PIX_FMT_BGR24;
  m[ARV_PIXEL_FORMAT_RGBA_8_PACKED] = PIX_FMT_RGBA;
  m[ARV_PIXEL_FORMAT_BGRA_8_PACKED] = PIX_FMT_BGRA;
  m[ARV_PIXEL_FORMAT_YUV_411_PACKED] = PIX_FMT_UYYVYY411;
  m[ARV_PIXEL_FORMAT_YUV_422_PACKED] = PIX_FMT_UYVY422;
  m[ARV_PIXEL_FORMAT_YUV_422_YUYV_PACKED] = PIX_FMT_YUYV422;

  return m;
}
