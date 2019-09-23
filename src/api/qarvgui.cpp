/*
    QArv, a Qt interface to aravis.
    Copyright (C) 2012, 2013 Jure Varlec <jure.varlec@ad-vega.si>
                             Andrej Lajovic <andrej.lajovic@ad-vega.si>

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
#include "qarvmainwindow.h"
#include "globals.h"

#include <QTranslator>
#include <QHBoxLayout>
#include <QMetaProperty>
#include <QShortcut>

using namespace QArv;

class QArvGuiExtension {
public:
    QArvMainWindow* mw;
};

/*! Translations and event filters are loaded.
 */
void QArvGui::init(QApplication* a) {
    auto trans = new QTranslator(a);
    auto locale = QLocale::system().name();
    if (trans->load(QString("qarv_") + locale, qarv_datafiles)) {
        a->installTranslator(trans);
        logMessage() << "Loaded translations for language" << locale;
    } else {
        logMessage() << "No translations available for language" << locale;
        delete trans;
    }

    // Install a global event filter that makes sure that long tooltips can be word-wrapped
    const int tooltip_wrap_threshold = 70;
    a->installEventFilter(new ToolTipToRichTextFilter(tooltip_wrap_threshold,
                                                      a));
}

/*! In standalone mode, the GUI presents full recording facilities. When not in
 * standalone mode, only the record button is available. When toggled, frames
 * will be decoded and made available using getFrame().
 */
QArvGui::QArvGui(QWidget* parent, bool standalone) :
    QWidget(parent) {
    ext = new QArvGuiExtension;
    ext->mw = new QArvMainWindow(NULL, standalone);
    setLayout(new QHBoxLayout);
    layout()->addWidget(ext->mw);
    auto metaobject = ext->mw->metaObject();
    for (int i = 0; i < metaobject->propertyCount(); i++) {
        auto metaproperty = metaobject->property(i);
        auto name = metaproperty.name();
        auto idx = metaObject()->indexOfProperty(name);
        if (idx != -1 && metaObject()->property(idx).isWritable()) {
            setProperty(name, ext->mw->property(name));
        }
    }
    connect(ext->mw,
            SIGNAL(recordingStarted(bool)),
            SLOT(signalForwarding(bool)));
    if (standalone)
        new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_Q), this, SLOT(close()));
}

QArvGui::~QArvGui() {
    delete ext;
}

void QArvGui::signalForwarding(bool enable) {
    if (enable) {
        connect(ext->mw->workthread,
                SIGNAL(frameDelivered(QByteArray,ArvBuffer*)),
                this, SIGNAL(frameReady(QByteArray,ArvBuffer*)));
        connect(ext->mw->workthread, SIGNAL(frameCooked(cv::Mat)),
                this, SIGNAL(frameReady(cv::Mat)));
    } else {
        disconnect(ext->mw->workthread,
                   SIGNAL(frameDelivered(QByteArray,ArvBuffer*)),
                   this, SIGNAL(frameReady(QByteArray,ArvBuffer*)));
        disconnect(ext->mw->workthread, SIGNAL(frameCooked(cv::Mat)),
                   this, SIGNAL(frameReady(cv::Mat)));
    }
    emit recordingToggled(enable);
}

/*! This function only works when not in standalone mode. It is useful when the
 * caller relies on fixed frame format and wants to disallow changing it during
 * operation.
 */
void QArvGui::forceRecording() {
    if (!ext->mw->standalone) {
        if (!ext->mw->recordAction->isChecked()) ext->mw->recordAction->
                setChecked(true);
        ext->mw->recordAction->setEnabled(false);
    }
}

/*! While the camera shouldn't normally be used outside the GUI, it is
 * sometimes useful to be able to get information about it, such as the
 * current pixel format.
 */
QArvCamera* QArvGui::camera() {
    return ext->mw->camera;
}

QMainWindow* QArvGui::mainWindow() {
    return ext->mw;
}

void QArvGui::closeEvent(QCloseEvent* event) {
    ext->mw->close();
    QWidget::closeEvent(event);
}
