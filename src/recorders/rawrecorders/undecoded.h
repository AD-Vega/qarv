#pragma once

#include "../recorder.h"

namespace QArv
{
    
class RawUndecodedFormat : public QObject, public OutputFormat {
    Q_OBJECT
    Q_INTERFACES(QArv::OutputFormat)
    Q_PLUGIN_METADATA(IID "si.ad-vega.qarv.RawUndecodedFormat")

public:
    QString name() override { return "Raw undecoded"; }
    bool canAppend() { return true; }
    bool canWriteInfo() override { return true; }
    bool recordsRaw() override { return true; }
    Recorder* makeRecorder(QArvDecoder* decoder,
                           QString fileName,
                           QSize frameSize,
                           int framesPerSecond) override;
};

}
