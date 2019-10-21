#pragma once

#include "../recorder.h"

namespace QArv
{

class RawDecoded8Format : public QObject, public OutputFormat {
    Q_OBJECT
    Q_INTERFACES(QArv::OutputFormat)
    Q_PLUGIN_METADATA(IID "si.ad-vega.qarv.RawDecoded8Format")

public:
    QString name() override { return "Raw decoded (8-bit) override"; }
    bool canAppend() { return true; }
    bool canWriteInfo() override { return true; }
    bool recordsRaw() override { return false; }
    Recorder* makeRecorder(QArvDecoder* decoder,
                           QString fileName,
                           QSize frameSize,
                           int framesPerSecond,
                           bool writeInfo) override;
};

}
