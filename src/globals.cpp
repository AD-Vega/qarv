#include "globals.h"

using namespace QArv;

const char* QArv::qarv_datafiles = QARV_DATA;

MessageSender QArvDebug::messageSender __attribute__((init_priority(1000)));

MessageSender::MessageSender(): QObject(), connected(false) {}

void MessageSender::connectNotify(const char * signal)
{
  QObject::connectNotify(signal);
  connected = true;
  foreach (const QString& msg, preconnectMessages)
    emit newDebugMessage(msg);
  preconnectMessages.clear();
}

void MessageSender::disconnectNotify(const char * signal)
{
  QObject::disconnectNotify(signal);
  if (receivers(SIGNAL(newDebugMessage(QString))) < 1)
    connected = false;
}

void MessageSender::sendMessage(const QString& message)
{
  if (connected)
    emit newDebugMessage(message);
  else
    preconnectMessages << message;
}

QArvDebug::~QArvDebug() {
  foreach (auto line, message.split('\n')) {
    if (line.startsWith('"')) {
      auto lineref = line.midRef(1, line.length() - 3);
      qDebug(prepend ? "QArv: %s" : "%s", lineref.toLocal8Bit().constData());
      messageSender.sendMessage(lineref.toString());
    } else {
      qDebug(prepend ? "QArv: %s" : "%s", line.toLocal8Bit().constData());
      messageSender.sendMessage(line);
    }
  }
}
