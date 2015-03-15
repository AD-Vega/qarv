/*
    QArv, a Qt interface to aravis.
    Copyright (C) 2012-2015 Jure Varlec <jure.varlec@ad-vega.si>
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

#ifndef C41_H
#define C41_H

#include "filters/filter.h"
#include "ui_c41.h"
#include <atomic>

namespace QArv {

class C41SettingsWidget;

class C41Filter : public ImageFilter {
public:
    struct Values {
        float min_r, min_g, min_b, light, gamma_g, gamma_b;
    };

    C41Filter(ImageFilterPlugin* plugin);
    ImageFilterSettingsWidget* createSettingsWidget();
    void restoreSettings();
    void saveSettings();
    void filterImage(cv::Mat& image);

private:
    std::atomic<float> min_r, min_g, min_b, light, gamma_g, gamma_b;
    std::atomic<bool> analyze, fix;
    std::atomic<C41SettingsWidget*> settingsWidget;

    friend class C41SettingsWidget;
};

class C41Plugin : public QObject, public ImageFilterPlugin {
    Q_OBJECT
    Q_INTERFACES(QArv::ImageFilterPlugin)
    Q_PLUGIN_METADATA(IID "si.ad-vega.qarv.C41Filter")

    public:
    QString name();
    ImageFilter* makeFilter();
};

class C41SettingsWidget : public ImageFilterSettingsWidget,
                            private Ui_c41SettingsWidget
{
    Q_OBJECT

public:
    C41SettingsWidget(ImageFilter* filter,
                        QWidget* parent = 0,
                        Qt::WindowFlags f = 0);
    virtual ~C41SettingsWidget();

protected slots:
    void setLiveUpdate(bool enabled);
    void applySettings();

public slots:
    void updateAnalysis(QArv::C41Filter::Values values);

private slots:
    void on_useValuesButton_clicked(bool checked);

private:
    C41Filter* filter();
};

};

#endif
