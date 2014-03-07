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

class MessageSender: public QObject {
public:
  Q_OBJECT

signals:
  void newDebugMessage(QString message);

private:
  bool connected;
  QStringList preconnectMessages;

  MessageSender();
  void connectNotify(const char* signal);
  void disconnectNotify(const char* signal);

  void sendMessage(const QString& message);

  friend class QArvDebug;
};

class QArvDebug: public QDebug {
public:

  ~QArvDebug();
  static MessageSender messageSender;

private:
  QArvDebug(bool prependProgramName) :
    QDebug(&message), prepend(prependProgramName) {}
  QString message;
  bool prepend;

  friend QArvDebug logMessage(bool);
};

inline QArvDebug logMessage(bool prependProgramName = true) {
  return QArvDebug(prependProgramName);
}

}

#endif
