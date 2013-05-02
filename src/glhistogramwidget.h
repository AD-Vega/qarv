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


#ifndef GLHISTOGRAMWIDGET_H
#define GLHISTOGRAMWIDGET_H

#include <QGLWidget>
#include <QMouseEvent>
#include <QIcon>
#include <opencv2/core/core.hpp>

namespace QArv
{

class GLHistogramWidget : public QGLWidget {
  Q_OBJECT

public:
  GLHistogramWidget(QWidget* parent = 0);
  ~GLHistogramWidget();

  void paintGL();
  void fromImage(const cv::Mat& image = cv::Mat());

public slots:
  void setLogarithmic(bool logarithmic);

private:
  QIcon idleImageIcon;
  bool indexed, logarithmic, idle;
  float* histRed;
  float* histGreen;
  float* histBlue;
};

}

#endif
