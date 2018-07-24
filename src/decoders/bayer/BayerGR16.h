#pragma once

#include "decoder.h"

namespace QArv
{

#ifdef ARV_PIXEL_FORMAT_BAYER_GR_16

class BayerGR16 : public QObject, public QArvPixelFormat {
    Q_OBJECT
    Q_INTERFACES(QArvPixelFormat)
    Q_PLUGIN_METADATA(IID "si.ad-vega.qarv.BayerGR16")

public:
    ArvPixelFormat pixelFormat() { return ARV_PIXEL_FORMAT_BAYER_GR_16; }
    QArvDecoder* makeDecoder(QSize size) {
        return new BayerDecoder<ARV_PIXEL_FORMAT_BAYER_GR_16>(size);
    }
};

#endif

}
