/*
    qarv, a Qt interface to aravis.
    Copyright (C) 2012  Jure Varlec <jure.varlec@ad-vega.si>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "api/qarvgui.h"
#include "mainwindow.h"
#include "globals.h"

#include <QTranslator>

class QArvGuiExtension {
public:
  MainWindow* mw;
};

/*! Translations are loaded. An event filter is installed which converts
 * plain-text tooltips longer than 70 characters into rich text, allowing
 * wrapping.
 */
void QArvGui::init(QApplication* a) {
  auto trans = new QTranslator(a);
  auto locale = QLocale::system().name();
  if (trans->load(QString("qarv_") + locale, qarv_datafiles)) {
    a->installTranslator(trans);
  } else {
    delete trans;
  }

  // Install a global event filter that makes sure that long tooltips can be word-wrapped
  const int tooltip_wrap_threshold = 70;
  a->installEventFilter(new ToolTipToRichTextFilter(tooltip_wrap_threshold, a));
}

/*! In standalone mode, the GUI presents full recording facilities. When not in
 * standalone mode, only the record button is available. When toggled, frames
 * will be decoded and made available using getFrame().
 */
QArvGui::QArvGui(QWidget* parent, bool standalone) : QObject(parent) {
  ext = new QArvGuiExtension;
  ext->mw = new MainWindow(0, standalone);
  thewidget = ext->mw;
  connect(ext->mw, SIGNAL(recordingStarted(bool)), SLOT(signalForwarding(bool)));
}

QArvGui::~QArvGui() {
  delete ext;
  bool autodelete = thewidget->testAttribute(Qt::WA_DeleteOnClose);
  bool closed = thewidget->close();
  if (!(autodelete && closed)) delete thewidget;
}

QWidget* QArvGui::widget() {
  return thewidget;
}

void QArvGui::signalForwarding(bool enable) {
  if (enable)
    connect(ext->mw->camera, SIGNAL(frameReady()), SIGNAL(frameReady()));
  else
    disconnect(ext->mw->camera, SIGNAL(frameReady()), this, SIGNAL(frameReady()));
}

/*!
 * \param processed The frame as seen in the GUI video display.
 * \param unprocessed The decoded frame with no transformations applied.
 * \param raw Undecoded buffer.
 * \param rawAravisBuffer See QArvCamera::getFrame().
 * \param nocopy See QArvCamera::getFrame().
 */
void QArvGui::getFrame(QImage* processed,
                       QImage* unprocessed,
                       QByteArray* raw,
                       ArvBuffer** rawAravisBuffer,
                       bool nocopy) {
  ext->mw->getNextFrame(processed, unprocessed, raw, rawAravisBuffer, nocopy);
}
