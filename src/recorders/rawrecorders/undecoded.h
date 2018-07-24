#pragma once

#include "../recorder.h"

namespace QArv
{
    
class RawUndecodedFormat : public QObject, public OutputFormat {
    Q_OBJECT
    Q_INTERFACES(QArv::OutputFormat)
    Q_PLUGIN_METADATA(IID "si.ad-vega.qarv.RawUndecodedFormat")

public:
    QString name() { return "Raw undecoded"; }
    bool canAppend() { return true; }
    bool canWriteInfo() { return true; }
    Recorder* makeRecorder(QArvDecoder* decoder,
                           QString fileName,
                           QSize frameSize,
                           int framesPerSecond,
                           bool writeInfo);
};

}
