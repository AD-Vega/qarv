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

#ifndef BAYER_H
#define BAYER_H

#include "api/qarvdecoder.h"
#include <opencv2/imgproc/imgproc.hpp>
#include <QDataStream>
extern "C" {
  #include <arvenums.h>
}


// Some formats appeared only after aravis-0.2.0, so
// we check for their presence. The 12_PACKED formats
// were added individually.

namespace QArv {

template <ArvPixelFormat fmt>
class BayerDecoder: public QArvDecoder {
public:
  BayerDecoder(QSize size_): size(size_),
  decoded(size.height(), size.width(), cvType()) {
    switch (fmt) {
      case ARV_PIXEL_FORMAT_BAYER_GR_10:
      case ARV_PIXEL_FORMAT_BAYER_RG_10:
      case ARV_PIXEL_FORMAT_BAYER_GB_10:
      case ARV_PIXEL_FORMAT_BAYER_BG_10:
        stage1 = QArvDecoder::makeDecoder(ARV_PIXEL_FORMAT_MONO_10, size);
        break;
      case ARV_PIXEL_FORMAT_BAYER_GR_12:
      case ARV_PIXEL_FORMAT_BAYER_RG_12:
      case ARV_PIXEL_FORMAT_BAYER_GB_12:
      case ARV_PIXEL_FORMAT_BAYER_BG_12:
        stage1 = QArvDecoder::makeDecoder(ARV_PIXEL_FORMAT_MONO_12, size);
        break;
#ifdef ARV_PIXEL_FORMAT_BAYER_GR_12_PACKED
      case ARV_PIXEL_FORMAT_BAYER_GR_12_PACKED:
#endif
#ifdef ARV_PIXEL_FORMAT_BAYER_RG_12_PACKED
      case ARV_PIXEL_FORMAT_BAYER_RG_12_PACKED:
#endif
#ifdef ARV_PIXEL_FORMAT_BAYER_GB_12_PACKED
      case ARV_PIXEL_FORMAT_BAYER_GB_12_PACKED:
#endif
      case ARV_PIXEL_FORMAT_BAYER_BG_12_PACKED:
        stage1 = QArvDecoder::makeDecoder(ARV_PIXEL_FORMAT_MONO_12_PACKED, size);
        break;
    }

    switch (fmt) {
      case ARV_PIXEL_FORMAT_BAYER_GR_8:
        cvt = CV_BayerGB2BGR;
        break;
      case ARV_PIXEL_FORMAT_BAYER_RG_8:
        cvt = CV_BayerBG2BGR;
        break;
      case ARV_PIXEL_FORMAT_BAYER_GB_8:
        cvt = CV_BayerGR2BGR;
        break;
      case ARV_PIXEL_FORMAT_BAYER_BG_8:
        cvt = CV_BayerRG2BGR;
        break;
#ifdef ARV_PIXEL_FORMAT_BAYER_GR_16
      case ARV_PIXEL_FORMAT_BAYER_GR_16:
        cvt = CV_BayerGB2BGR;
        break;
      case ARV_PIXEL_FORMAT_BAYER_RG_16:
        cvt = CV_BayerBG2BGR;
        break;
      case ARV_PIXEL_FORMAT_BAYER_GB_16:
        cvt = CV_BayerGR2BGR;
        break;
      case ARV_PIXEL_FORMAT_BAYER_BG_16:
        cvt = CV_BayerRG2BGR;
        break;
#endif
      case ARV_PIXEL_FORMAT_BAYER_GR_10:
        cvt = CV_BayerGB2BGR;
        break;
      case ARV_PIXEL_FORMAT_BAYER_RG_10:
        cvt = CV_BayerBG2BGR;
        break;
      case ARV_PIXEL_FORMAT_BAYER_GB_10:
        cvt = CV_BayerGR2BGR;
        break;
      case ARV_PIXEL_FORMAT_BAYER_BG_10:
        cvt = CV_BayerRG2BGR;
        break;
      case ARV_PIXEL_FORMAT_BAYER_GR_12:
        cvt = CV_BayerGB2BGR;
        break;
      case ARV_PIXEL_FORMAT_BAYER_RG_12:
        cvt = CV_BayerBG2BGR;
        break;
      case ARV_PIXEL_FORMAT_BAYER_GB_12:
        cvt = CV_BayerGR2BGR;
        break;
      case ARV_PIXEL_FORMAT_BAYER_BG_12:
        cvt = CV_BayerRG2BGR;
        break;
#ifdef ARV_PIXEL_FORMAT_BAYER_GR_12_PACKED
      case ARV_PIXEL_FORMAT_BAYER_GR_12_PACKED:
        cvt = CV_BayerGB2BGR;
        break;
#endif
#ifdef ARV_PIXEL_FORMAT_BAYER_RG_12_PACKED
      case ARV_PIXEL_FORMAT_BAYER_RG_12_PACKED:
        cvt = CV_BayerBG2BGR;
        break;
#endif
#ifdef ARV_PIXEL_FORMAT_BAYER_GB_12_PACKED
      case ARV_PIXEL_FORMAT_BAYER_GB_12_PACKED:
        cvt = CV_BayerGR2BGR;
        break;
#endif
      case ARV_PIXEL_FORMAT_BAYER_BG_12_PACKED:
        cvt = CV_BayerRG2BGR;
        break;
    }
};

  const cv::Mat getCvImage() { return decoded; }

  int cvType() {
    switch (fmt) {
      case ARV_PIXEL_FORMAT_BAYER_GR_8:
      case ARV_PIXEL_FORMAT_BAYER_RG_8:
      case ARV_PIXEL_FORMAT_BAYER_GB_8:
      case ARV_PIXEL_FORMAT_BAYER_BG_8:
        return CV_8UC3;
      default:
        return CV_16UC3;
    }
  }

  ArvPixelFormat pixelFormat() { return fmt; }

  QByteArray decoderSpecification() {
    QByteArray b;
    QDataStream s(&b, QIODevice::WriteOnly);
    s << QString("Aravis") << size << pixelFormat() << false;
    return b;
  }

  void decode(QByteArray frame) {
    // Workaround: cv::Mat has no const data constructor, but data need
    // not be copied, as QByteArray::data() does.
    void* data = const_cast<void*>(reinterpret_cast<const void*>(frame.constData()));
    switch (fmt) {
      case ARV_PIXEL_FORMAT_BAYER_GR_8:
      case ARV_PIXEL_FORMAT_BAYER_RG_8:
      case ARV_PIXEL_FORMAT_BAYER_GB_8:
      case ARV_PIXEL_FORMAT_BAYER_BG_8:
        tmp = cv::Mat(size.height(), size.width(), CV_8UC1, data);
        break;
#ifdef ARV_PIXEL_FORMAT_BAYER_GR_16
      case ARV_PIXEL_FORMAT_BAYER_GR_16:
      case ARV_PIXEL_FORMAT_BAYER_RG_16:
      case ARV_PIXEL_FORMAT_BAYER_GB_16:
      case ARV_PIXEL_FORMAT_BAYER_BG_16:
        tmp = cv::Mat(size.height(), size.width(), CV_16UC1, data);
        break;
#endif
      default:
        stage1->decode(frame);
        tmp = stage1->getCvImage();
        break;
    }
    cv::cvtColor(tmp, decoded, cvt);
  }

private:
    QSize size;
    cv::Mat tmp;
    cv::Mat decoded;
    QArvDecoder* stage1;
    int cvt;
};

// 8-bit

class BayerGR8 : public QObject, public QArvPixelFormat {
  Q_OBJECT
  Q_INTERFACES(QArvPixelFormat)

public:
  ArvPixelFormat pixelFormat() { return ARV_PIXEL_FORMAT_BAYER_GR_8; }
  QArvDecoder* makeDecoder(QSize size) {
    return new BayerDecoder<ARV_PIXEL_FORMAT_BAYER_GR_8>(size);
  }
};

class BayerRG8 : public QObject, public QArvPixelFormat {
  Q_OBJECT
  Q_INTERFACES(QArvPixelFormat)

public:
  ArvPixelFormat pixelFormat() { return ARV_PIXEL_FORMAT_BAYER_RG_8; }
  QArvDecoder* makeDecoder(QSize size) {
    return new BayerDecoder<ARV_PIXEL_FORMAT_BAYER_RG_8>(size);
  }
};

class BayerGB8 : public QObject, public QArvPixelFormat {
  Q_OBJECT
  Q_INTERFACES(QArvPixelFormat)

public:
  ArvPixelFormat pixelFormat() { return ARV_PIXEL_FORMAT_BAYER_GB_8; }
  QArvDecoder* makeDecoder(QSize size) {
    return new BayerDecoder<ARV_PIXEL_FORMAT_BAYER_GB_8>(size);
  }
};

class BayerBG8 : public QObject, public QArvPixelFormat {
  Q_OBJECT
  Q_INTERFACES(QArvPixelFormat)

public:
  ArvPixelFormat pixelFormat() { return ARV_PIXEL_FORMAT_BAYER_BG_8; }
  QArvDecoder* makeDecoder(QSize size) {
    return new BayerDecoder<ARV_PIXEL_FORMAT_BAYER_BG_8>(size);
  }
};

// 10-bit

class BayerGR10 : public QObject, public QArvPixelFormat {
  Q_OBJECT
  Q_INTERFACES(QArvPixelFormat)

public:
  ArvPixelFormat pixelFormat() { return ARV_PIXEL_FORMAT_BAYER_GR_10; }
  QArvDecoder* makeDecoder(QSize size) {
    return new BayerDecoder<ARV_PIXEL_FORMAT_BAYER_GR_10>(size);
  }
};

class BayerRG10 : public QObject, public QArvPixelFormat {
  Q_OBJECT
  Q_INTERFACES(QArvPixelFormat)

public:
  ArvPixelFormat pixelFormat() { return ARV_PIXEL_FORMAT_BAYER_RG_10; }
  QArvDecoder* makeDecoder(QSize size) {
    return new BayerDecoder<ARV_PIXEL_FORMAT_BAYER_RG_10>(size);
  }
};

class BayerGB10 : public QObject, public QArvPixelFormat {
  Q_OBJECT
  Q_INTERFACES(QArvPixelFormat)

public:
  ArvPixelFormat pixelFormat() { return ARV_PIXEL_FORMAT_BAYER_GB_10; }
  QArvDecoder* makeDecoder(QSize size) {
    return new BayerDecoder<ARV_PIXEL_FORMAT_BAYER_GB_10>(size);
  }
};

class BayerBG10 : public QObject, public QArvPixelFormat {
  Q_OBJECT
  Q_INTERFACES(QArvPixelFormat)

public:
  ArvPixelFormat pixelFormat() { return ARV_PIXEL_FORMAT_BAYER_BG_10; }
  QArvDecoder* makeDecoder(QSize size) {
    return new BayerDecoder<ARV_PIXEL_FORMAT_BAYER_BG_10>(size);
  }
};

// 12-bit

class BayerGR12 : public QObject, public QArvPixelFormat {
  Q_OBJECT
  Q_INTERFACES(QArvPixelFormat)

public:
  ArvPixelFormat pixelFormat() { return ARV_PIXEL_FORMAT_BAYER_GR_12; }
  QArvDecoder* makeDecoder(QSize size) {
    return new BayerDecoder<ARV_PIXEL_FORMAT_BAYER_GR_12>(size);
  }
};

class BayerRG12 : public QObject, public QArvPixelFormat {
  Q_OBJECT
  Q_INTERFACES(QArvPixelFormat)

public:
  ArvPixelFormat pixelFormat() { return ARV_PIXEL_FORMAT_BAYER_RG_12; }
  QArvDecoder* makeDecoder(QSize size) {
    return new BayerDecoder<ARV_PIXEL_FORMAT_BAYER_RG_12>(size);
  }
};

class BayerGB12 : public QObject, public QArvPixelFormat {
  Q_OBJECT
  Q_INTERFACES(QArvPixelFormat)

public:
  ArvPixelFormat pixelFormat() { return ARV_PIXEL_FORMAT_BAYER_GB_12; }
  QArvDecoder* makeDecoder(QSize size) {
    return new BayerDecoder<ARV_PIXEL_FORMAT_BAYER_GB_12>(size);
  }
};

class BayerBG12 : public QObject, public QArvPixelFormat {
  Q_OBJECT
  Q_INTERFACES(QArvPixelFormat)

public:
  ArvPixelFormat pixelFormat() { return ARV_PIXEL_FORMAT_BAYER_BG_12; }
  QArvDecoder* makeDecoder(QSize size) {
    return new BayerDecoder<ARV_PIXEL_FORMAT_BAYER_BG_12>(size);
  }
};

// 12-bit packed

#ifdef ARV_PIXEL_FORMAT_BAYER_GR_12_PACKED

class BayerGR12_PACKED : public QObject, public QArvPixelFormat {
  Q_OBJECT
  Q_INTERFACES(QArvPixelFormat)

public:
  ArvPixelFormat pixelFormat() { return ARV_PIXEL_FORMAT_BAYER_GR_12_PACKED; }
  QArvDecoder* makeDecoder(QSize size) {
    return new BayerDecoder<ARV_PIXEL_FORMAT_BAYER_GR_12_PACKED>(size);
  }
};

#endif

#ifdef ARV_PIXEL_FORMAT_BAYER_RG_12_PACKED

class BayerRG12_PACKED : public QObject, public QArvPixelFormat {
  Q_OBJECT
  Q_INTERFACES(QArvPixelFormat)

public:
  ArvPixelFormat pixelFormat() { return ARV_PIXEL_FORMAT_BAYER_RG_12_PACKED; }
  QArvDecoder* makeDecoder(QSize size) {
    return new BayerDecoder<ARV_PIXEL_FORMAT_BAYER_RG_12_PACKED>(size);
  }
};

#endif

#ifdef ARV_PIXEL_FORMAT_BAYER_GB_12_PACKED

class BayerGB12_PACKED : public QObject, public QArvPixelFormat {
  Q_OBJECT
  Q_INTERFACES(QArvPixelFormat)

public:
  ArvPixelFormat pixelFormat() { return ARV_PIXEL_FORMAT_BAYER_GB_12_PACKED; }
  QArvDecoder* makeDecoder(QSize size) {
    return new BayerDecoder<ARV_PIXEL_FORMAT_BAYER_GB_12_PACKED>(size);
  }
};

#endif

class BayerBG12_PACKED : public QObject, public QArvPixelFormat {
  Q_OBJECT
  Q_INTERFACES(QArvPixelFormat)

public:
  ArvPixelFormat pixelFormat() { return ARV_PIXEL_FORMAT_BAYER_BG_12_PACKED; }
  QArvDecoder* makeDecoder(QSize size) {
    return new BayerDecoder<ARV_PIXEL_FORMAT_BAYER_BG_12_PACKED>(size);
  }
};

// 16-bit

#ifdef ARV_PIXEL_FORMAT_BAYER_GR_16

class BayerGR16 : public QObject, public QArvPixelFormat {
  Q_OBJECT
  Q_INTERFACES(QArvPixelFormat)

public:
  ArvPixelFormat pixelFormat() { return ARV_PIXEL_FORMAT_BAYER_GR_16; }
  QArvDecoder* makeDecoder(QSize size) {
    return new BayerDecoder<ARV_PIXEL_FORMAT_BAYER_GR_16>(size);
  }
};

class BayerRG16 : public QObject, public QArvPixelFormat {
  Q_OBJECT
  Q_INTERFACES(QArvPixelFormat)

public:
  ArvPixelFormat pixelFormat() { return ARV_PIXEL_FORMAT_BAYER_RG_16; }
  QArvDecoder* makeDecoder(QSize size) {
    return new BayerDecoder<ARV_PIXEL_FORMAT_BAYER_RG_16>(size);
  }
};

class BayerGB16 : public QObject, public QArvPixelFormat {
  Q_OBJECT
  Q_INTERFACES(QArvPixelFormat)

public:
  ArvPixelFormat pixelFormat() { return ARV_PIXEL_FORMAT_BAYER_GB_16; }
  QArvDecoder* makeDecoder(QSize size) {
    return new BayerDecoder<ARV_PIXEL_FORMAT_BAYER_GB_16>(size);
  }
};

class BayerBG16 : public QObject, public QArvPixelFormat {
  Q_OBJECT
  Q_INTERFACES(QArvPixelFormat)

public:
  ArvPixelFormat pixelFormat() { return ARV_PIXEL_FORMAT_BAYER_BG_16; }
  QArvDecoder* makeDecoder(QSize size) {
    return new BayerDecoder<ARV_PIXEL_FORMAT_BAYER_BG_16>(size);
  }
};

#endif

}

#endif
