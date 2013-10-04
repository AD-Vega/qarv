#include "globals.h"

using namespace QArv;

const char* QArv::qarv_datafiles = QARV_DATA;

QStandardItemModel QArvDebug::model;

QArvDebug::~QArvDebug() {
  foreach (auto line, message.split('\n')) {
    if (line.startsWith('"')) {
      auto lineref = line.midRef(1, line.length() - 3);
      qDebug(prepend ? "QArv: %s" : "%s", lineref.toLocal8Bit().constData());
      model.appendRow(new QStandardItem(lineref.toString()));
    } else {
      qDebug(prepend ? "QArv: %s" : "%s", line.toLocal8Bit().constData());
      model.appendRow(new QStandardItem(line));
    }
  }
}
