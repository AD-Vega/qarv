/*
    <one line to give the program's name and a brief idea of what it does.>
    Copyright (C) 2012  Jure Varlec <jure.varlec@gmail.com>

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


#ifndef GLVIDEOWIDGET_H
#define GLVIDEOWIDGET_H

#include <QGLWidget>
#include <QImage>

class GLVideoWidget: public QGLWidget {
  Q_OBJECT

public:
  GLVideoWidget(QWidget* parent = NULL);
  ~GLVideoWidget();
  void setImage(const QImage& image);
  QImage getImage();
  void paintGL();

private:
  QImage image;
};

#endif // GLVIDEOWIDGET_H
