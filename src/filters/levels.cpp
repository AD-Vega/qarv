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

#include "globals.h"
#include "filters/levels.h"
#include <QtPlugin>

using namespace QArv;

QString LevelsPlugin::name() {
    return tr("Levels");
}

ImageFilter* LevelsPlugin::makeFilter() {
    return new LevelsFilter(this);
}

void LevelsFilter::filterImage(cv::Mat& image) {
    float g = 1. / gamma.load(std::memory_order_relaxed);
    float b = pow(black.load(std::memory_order_relaxed), g);
    float w = pow(white.load(std::memory_order_relaxed), g);
    image.convertTo(image, CV_32F);
    double max;
    cv::minMaxIdx(image, NULL, &max);
    image /= (float)max;
    cv::pow(image, g, image);
    image = (float)max * (image - b) / (w - b);
}

LevelsFilter::LevelsFilter(ImageFilterPlugin* plugin) :
    ImageFilter(plugin) {
    black.store(0);
    white.store(1);
    gamma.store(1);
}

void LevelsFilter::restoreSettings() {
    QSettings settings;
    double g = settings.value("qarv_filters_levels/gamma", 1.0).toDouble();
    double b = settings.value("qarv_filters_levels/black", 0.0).toDouble();
    double w = settings.value("qarv_filters_levels/white", 1.0).toDouble();
    gamma.store(g, std::memory_order_relaxed);
    black.store(b, std::memory_order_relaxed);
    white.store(w, std::memory_order_relaxed);
}

void LevelsFilter::saveSettings() {
    QSettings settings;
    double g = gamma.load(std::memory_order_relaxed),
           b = black.load(std::memory_order_relaxed),
           w = white.load(std::memory_order_relaxed);
    settings.setValue("qarv_filters_levels/gamma", g);
    settings.setValue("qarv_filters_levels/black", b);
    settings.setValue("qarv_filters_levels/white", w);
}

ImageFilterSettingsWidget* LevelsFilter::createSettingsWidget() {
    return new LevelsSettingsWidget(this);
}

LevelsSettingsWidget::LevelsSettingsWidget(ImageFilter* filter_,
                                           QWidget* parent) :
    ImageFilterSettingsWidget(filter_, parent) {
    setupUi(this);
    QMetaObject::connectSlotsByName(this);
    blackSpinbox->setValue(filter()->black.load(std::memory_order_relaxed));
    whiteSpinbox->setValue(filter()->white.load(std::memory_order_relaxed));
    gammaSpinbox->setValue(filter()->gamma.load(std::memory_order_relaxed));
}

void LevelsSettingsWidget::applySettings() {
    filter()->black.store(blackSpinbox->value(), std::memory_order_relaxed);
    filter()->white.store(whiteSpinbox->value(), std::memory_order_relaxed);
    filter()->gamma.store(gammaSpinbox->value(), std::memory_order_relaxed);
}

void LevelsSettingsWidget::setLiveUpdate(bool enabled) {
    if (enabled) {
        connect(blackSpinbox,
                SIGNAL(valueChanged(double)),
                SLOT(applySettings()));
        connect(whiteSpinbox,
                SIGNAL(valueChanged(double)),
                SLOT(applySettings()));
        connect(gammaSpinbox,
                SIGNAL(valueChanged(double)),
                SLOT(applySettings()));
    } else {
        disconnect(blackSpinbox, SIGNAL(valueChanged(double)), this,
                   SLOT(applySettings()));
        disconnect(whiteSpinbox, SIGNAL(valueChanged(double)), this,
                   SLOT(applySettings()));
        disconnect(gammaSpinbox, SIGNAL(valueChanged(double)), this,
                   SLOT(applySettings()));
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

Q_IMPORT_PLUGIN(LevelsPlugin)
