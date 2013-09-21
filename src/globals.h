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
  QArvDebug(bool prependProgramName) :
    QDebug(&message), prepend(prependProgramName) {}
  QString message;
  bool prepend;

public:
  static QStandardItemModel model;
  ~QArvDebug() {
    qDebug(prepend ? "QArv: %s" : "%s", message.toAscii().constData());
    model.appendRow(new QStandardItem(message));
  }

  friend QArvDebug logMessage(bool);
};

inline QArvDebug logMessage(bool prependProgramName = true) {
  return QArvDebug(prependProgramName);
}

}

#endif
