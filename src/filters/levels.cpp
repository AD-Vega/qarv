/*
    QArv, a Qt interface to aravis.
    Copyright (C) 2012-2014 Jure Varlec <jure.varlec@ad-vega.si>
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

#include "filters/levels.h"
#include "globals.h"
#include <QtPlugin>

using namespace QArv;

QString LevelsPlugin::name() {
  return tr("Levels");
}

ImageFilter* LevelsPlugin::makeFilter() {
  return new LevelsFilter(this);
}

void LevelsFilter::filterImage(cv::Mat& image) {
  float g = 1. / gamma;
  float b = pow(black, g);
  float w = pow(white, g);
  image.convertTo(image, CV_32F);
  double max;
  cv::minMaxIdx(image, NULL, &max);
  image /= (float)max;
  cv::pow(image, g, image);
  image = (float)max * (image - b) / (w - b);
}

LevelsFilter::LevelsFilter(ImageFilterPlugin* plugin) :
  ImageFilter(plugin) {}

void LevelsFilter::restoreSettings() {
  QSettings settings;
  gamma = settings.value("qarv_filters_levels/gamma", 1.0).toDouble();
  black = settings.value("qarv_filters_levels/black", 0.0).toDouble();
  white = settings.value("qarv_filters_levels/white", 1.0).toDouble();
}

void LevelsFilter::saveSettings() {
  QSettings settings;
  settings.setValue("qarv_filters_levels/gamma", gamma);
  settings.setValue("qarv_filters_levels/black", black);
  settings.setValue("qarv_filters_levels/white", white);
}

ImageFilterSettingsWidget* LevelsFilter::createSettingsWidget() {
  return new LevelsSettingsWidget(this);
}

LevelsSettingsWidget::LevelsSettingsWidget(ImageFilter* filter_,
                                           QWidget* parent,
                                           Qt::WindowFlags f) :
  ImageFilterSettingsWidget(filter_, parent, f) {
  setupUi(this);
  QMetaObject::connectSlotsByName(this);
  blackSpinbox->setValue(filter()->black);
  whiteSpinbox->setValue(filter()->white);
  gammaSpinbox->setValue(filter()->gamma);
}

void LevelsSettingsWidget::applySettings() {
  filter()->black = blackSpinbox->value();
  filter()->white = whiteSpinbox->value();
  filter()->gamma = gammaSpinbox->value();
}

void LevelsSettingsWidget::setLiveUpdate(bool enabled) {
  if (enabled) {
    connect(blackSpinbox, SIGNAL(valueChanged(double)), SLOT(applySettings()));
    connect(whiteSpinbox, SIGNAL(valueChanged(double)), SLOT(applySettings()));
    connect(gammaSpinbox, SIGNAL(valueChanged(double)), SLOT(applySettings()));
  } else {
    disconnect(blackSpinbox, SIGNAL(valueChanged(double)), this, SLOT(applySettings()));
    disconnect(whiteSpinbox, SIGNAL(valueChanged(double)), this, SLOT(applySettings()));
    disconnect(gammaSpinbox, SIGNAL(valueChanged(double)), this, SLOT(applySettings()));
  }
}

void LevelsSettingsWidget::on_blackSlider_valueChanged(int value) {
  blackSpinbox->setValue(intToDouble(value));
}

void LevelsSettingsWidget::on_gammaSlider_valueChanged(int value) {
  auto range = qMakePair(0.1, 10.0);
  gammaSpinbox->setValue(slider2value_log(value, range));
}

void LevelsSettingsWidget::on_whiteSlider_valueChanged(int value) {
  whiteSpinbox->setValue(intToDouble(value));
}

void LevelsSettingsWidget::on_blackSpinbox_valueChanged(double value) {
  blackSlider->setValue(doubleToInt(value));
}

void LevelsSettingsWidget::on_gammaSpinbox_valueChanged(double value) {
  auto range = qMakePair(0.1, 10.0);
  gammaSlider->setValue(value2slider_log(value, range));
}

void LevelsSettingsWidget::on_whiteSpinbox_valueChanged(double value) {
  whiteSlider->setValue(doubleToInt(value));
}

LevelsFilter* LevelsSettingsWidget::filter() {
  return static_cast<LevelsFilter*>(imageFilter);
}

int LevelsSettingsWidget::doubleToInt(double val) {
  return val * 1000;
}

double LevelsSettingsWidget::intToDouble(int val) {
  return val / 1000.0;
}

Q_EXPORT_PLUGIN2(LevelsPlugin, QArv::LevelsPlugin)
Q_IMPORT_PLUGIN(LevelsPlugin)
