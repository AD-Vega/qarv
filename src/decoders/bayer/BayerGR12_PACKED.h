#pragma once

#include "decoder.h"

namespace QArv
{

#ifdef ARV_PIXEL_FORMAT_BAYER_GR_12_PACKED

class BayerGR12_PACKED : public QObject, public QArvPixelFormat {
    Q_OBJECT
    Q_INTERFACES(QArvPixelFormat)
    Q_PLUGIN_METADATA(IID "si.ad-vega.qarv.BayerGR12_PACKED")

public:
    ArvPixelFormat pixelFormat() { return ARV_PIXEL_FORMAT_BAYER_GR_12_PACKED; }
    QArvDecoder* makeDecoder(QSize size) {
        return new BayerDecoder<ARV_PIXEL_FORMAT_BAYER_GR_12_PACKED>(size);
    }
};

#endif

}
