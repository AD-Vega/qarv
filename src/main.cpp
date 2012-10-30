/*
 *    qarv, a Qt interface to aravis.
 *    Copyright (C) 2012  Jure Varlec <jure.varlec@ad-vega.si>
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include <QtGui/QApplication>
#include "api/qarvcamera.h"
#include "api/qarvgui.h"

int main(int argc, char** argv) {
  QApplication a(argc, argv);
  QArvCamera::init();
  QArvGui::init(&a);

  QCoreApplication::setOrganizationDomain("ad-vega.si");
  QCoreApplication::setOrganizationName("AD Vega");
  QCoreApplication::setApplicationName("qarv");

  qRegisterMetaType<QVector<double>>("QVector<double>");
  
  QArvGui g;
  g.widget()->show();
  return a.exec();
}
