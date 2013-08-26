#ifndef GLOBALS_H
#define GLOBALS_H

#include <QDebug>
#include <QStandardItemModel>

namespace QArv
{

#ifndef QARV_PREFIX
#define QARV_PREFIX ""
#endif

extern const char* qarv_datafiles;

class QArvDebug: public QDebug {
private:
  QArvDebug() : QDebug(&message) {}
  QString message;

public:
  static QStandardItemModel model;
  ~QArvDebug() {
    qDebug("QArv: %s", message.toAscii().constData());
    model.appendRow(new QStandardItem(message));
  }

  friend QArvDebug logMessage();
};

QArvDebug logMessage();

}

#endif
