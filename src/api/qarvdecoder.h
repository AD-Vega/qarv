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

#ifndef QARVDECODER_H
#define QARVDECODER_H

#include <QByteArray>
#include <QString>
#include <QSize>
#include <QImage>
#include <QtPlugin>
#include <opencv2/core/core.hpp>
extern "C" {
#include <libavutil/pixfmt.h>
}

#ifndef ARV_PIXEL_FORMAT_MONO_8
typedef quint32 ArvPixelFormat;
#endif

//! An abstract interface of a decoder for a particular frame format and size.
class QArvDecoder {
public:
  virtual ~QArvDecoder() {};

  //! Decodes the given frame.
  virtual void decode(QByteArray frame) = 0;

  /*!
   * Returns the decoded frame as an OpenCv matrix, CV_16U, either 1 or 3 channels.
   * The matrix is constant to avoid copying.
   */
  virtual const cv::Mat getCvImage() = 0;

  //! Returns the pixel format supported by this decoder.
  virtual ArvPixelFormat pixelFormat() = 0;

  /*!
   * Convenience function to convert an OpenCV image to QImage. Limited to
   * formats used by QArv. Returns Format_ARGB32_Premultiplied, which is
   * fast to render.
   */
  static void CV2QImage(const cv::Mat& image, QImage& out);
  static QImage CV2QImage(const cv::Mat& image);

  /*!
   * Alternative version of CV2QImage() which returns either Format_RGB888
   * or Format_Indexed8, which may be easier to use for direct pixel
   * manipulation, but are slow to render. Not recommended, for manipulation
   * cv::Mat should be used instead.
   */
  static void CV2QImage_RGB24(const cv::Mat& image, QImage& out);
  static QImage CV2QImage_RGB24(const cv::Mat& image);
};

//! Interface for the plugin to generate a decoder for a particular format.
class QArvPixelFormat {
public:
  virtual ~QArvPixelFormat() {};

  //! Returns the Aravis' name for the pixel format supported by this plugin.
  virtual ArvPixelFormat pixelFormat() = 0;

  //! Instantiates a decoder using this plugin.
  virtual QArvDecoder* makeDecoder(QSize size) = 0;

  //! Returns the list of supported pixel formats.
  static QList<ArvPixelFormat> supportedFormats();

  //! Directly creates a decoder for the requested format and frame size.
  static QArvDecoder* makeDecoder(ArvPixelFormat, QSize size);

  //! Convenience function to create a libswscale decoder.
  static QArvDecoder* makeSwScaleDecoder(enum PixelFormat, QSize size);
};

Q_DECLARE_INTERFACE(QArvPixelFormat,
                    "si.ad-vega.qarv.QArvPixelFormat/0.1");

#endif
