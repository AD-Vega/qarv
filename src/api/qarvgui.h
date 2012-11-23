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


#ifndef QARVGUI_H
#define QARVGUI_H

#include "qarvcamera.h"
#include <QWidget>
#include <QApplication>

class QArvGuiExtension;

//! QArvGui contains the widget for working with a camera.
/*! The init() functions should be called to load translations and other setup.
 * After that, the GUI can be instantiated. It will create a widget, accessible
 * via the widget() method. This widget is hidden by default.
 * 
 * Only the standalone mode is currently implemented.
 */
class QArvGui : public QObject {
  Q_OBJECT

public:
  QArvGui(QWidget* parent = 0, bool standalone = true);
  ~QArvGui();

  //! Fills the non-NULL parameters with the current frame.
  void getFrame(QImage* processed = 0,
                QImage* unprocessed = 0,
                QByteArray* raw = 0,
                ArvBuffer** rawAravisBuffer = 0,
                bool nocopy = false);

  //! Same as getFrame(), but takes the nocopy parameter from the GUI.
  void getFrame(QImage* processed = 0,
                QImage* unprocessed = 0,
                QByteArray* raw = 0,
                ArvBuffer** rawAravisBuffer = 0);

  //! Returns the widget containing the GUI.
  QWidget* widget();

  //! Does static initialization.
  static void init(QApplication* a);

  //! Permanently enable the frameReady() signal and disable some GUI options.
  void forceRecording();

  //! Returns the camera object.
  QArvCamera* camera();

signals:
  void frameReady();

private slots:
  void signalForwarding(bool enable);

private:
  QWidget* thewidget;
  QArvGuiExtension* ext;

  friend class QArvGuiExtension;
};

#endif