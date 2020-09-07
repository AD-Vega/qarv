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

#ifndef LEVELS_H
#define LEVELS_H

#include "filters/filter.h"
#include "ui_levels.h"
#include <atomic>

namespace QArv
{

class LevelsFilter : public ImageFilter {
public:
    LevelsFilter(ImageFilterPlugin* plugin);
    ImageFilterSettingsWidget* createSettingsWidget() override;
    void restoreSettings() override;
    void saveSettings() override;
    void filterImage(cv::Mat& image) override;

private:
    std::atomic<double> black, white, gamma;

    friend class LevelsSettingsWidget;
};

class LevelsPlugin : public QObject, public ImageFilterPlugin {
    Q_OBJECT
    Q_INTERFACES(QArv::ImageFilterPlugin)
    Q_PLUGIN_METADATA(IID "si.ad-vega.qarv.LevelsFilter")

public:
    QString name() override;
    ImageFilter* makeFilter() override;
};

class LevelsSettingsWidget : public ImageFilterSettingsWidget,
                             private Ui_levelsSettingsWidget {
    Q_OBJECT

public:
    LevelsSettingsWidget(ImageFilter* filter,
                         QWidget* parent = 0);

protected slots:
    void setLiveUpdate(bool enabled) override;
    void applySettings() override;

private slots:
    void on_blackSlider_valueChanged(int value);
    void on_whiteSlider_valueChanged(int value);
    void on_gammaSlider_valueChanged(int value);
    void on_blackSpinbox_valueChanged(double value);
    void on_whiteSpinbox_valueChanged(double value);
    void on_gammaSpinbox_valueChanged(double value);

private:
    LevelsFilter* filter();
    int doubleToInt(double val);
    double intToDouble(int val);
};

};

#endif
