#ifndef GLOBALS_H
#define GLOBALS_H

#include <QDebug>
#include <QStandardItemModel>
#include <cmath>

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

const int slidersteps = 1000;

static inline double slider2value_log(int slidervalue,
                                      QPair<double, double>& range) {
  double value = log2(range.second) - log2(range.first);
  return exp2(value * slidervalue / slidersteps + log2(range.first));
}

static inline int value2slider_log(double value,
                                   QPair<double, double>& range) {
  return slidersteps
         * (log2(value) - log2(range.first))
         / (log2(range.second) - log2(range.first));
}

template <typename T>
static inline QVariant ptr2var(const T* ptr) {
  return QVariant::fromValue<quintptr>(reinterpret_cast<quintptr>(ptr));
}

template <typename T>
static inline T* var2ptr(const QVariant& val) {
  return reinterpret_cast<T*>(val.value<quintptr>());
}

}

#endif
