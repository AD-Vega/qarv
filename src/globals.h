#ifndef GLOBALS_H
#define GLOBALS_H

#include <QDebug>
#include <QStandardItemModel>

namespace QArv
{

#ifndef QARV_DATA
#define QARV_DATA "res/icons/"
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
    foreach (auto line, message.split('\n')) {
      qDebug(prepend ? "QArv: %s" : "%s", line.toAscii().constData());
      model.appendRow(new QStandardItem(line));
    }
  }

  friend QArvDebug logMessage(bool);
};

inline QArvDebug logMessage(bool prependProgramName = true) {
  return QArvDebug(prependProgramName);
}

}

#endif
