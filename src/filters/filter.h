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

/*
 * ATTENTION!!!!
 * The filter will run in a different thread than the GUI and must be
 * written with this in mind, especially the live updates!
 * It may not matter, however: e.g. the Levels plugin does not really care
 * about partial parameter updates.
 */

#ifndef FILTER_H
#define FILTER_H

#include <gio/gio.h>  // Workaround for gdbusintrospection's use of "signal".
#include <QDockWidget>
#include <QSettings>
#include <opencv2/opencv.hpp>
#include <atomic>

namespace QArv
{

class ImageFilter;
class ImageFilterPlugin;

typedef QSharedPointer<ImageFilter> ImageFilterPtr;

class ImageFilterSettingsWidget : public QWidget {
    Q_OBJECT

public:
    explicit ImageFilterSettingsWidget(ImageFilter* filter,
                                       QWidget* parent = 0,
                                       Qt::WindowFlags f = 0);
    virtual ~ImageFilterSettingsWidget() {}

protected slots:
    //! Implemented by each plugin to update the filter continuously.
    virtual void setLiveUpdate(bool enabled) = 0;

    //! Implemented by each plugin to update the filter once.
    virtual void applySettings() = 0;

protected:
    ImageFilter* imageFilter;

    friend class ImageFilterSettingsDialog;
};

class ImageFilter {
public:
    ImageFilter(ImageFilterPlugin* plugin);
    virtual ~ImageFilter() {}

    //! Links back to the plugin, to get the plugin name etc.
    ImageFilterPlugin* plugin();

    //! Shows the settings dialog.
    virtual ImageFilterSettingsWidget* createSettingsWidget() = 0;

    //! Called when the filter is instantiated.
    virtual void restoreSettings() = 0;

    //! Called by the ImageFilterSettingsDialog when it is closed.
    virtual void saveSettings() = 0;

    //! The gist of the matter. It works in-place. It can return a float CV_TYPE!
    virtual void filterImage(cv::Mat& image) = 0;

    //! Used by the main window to mark filter as enabled.
    bool isEnabled() { return enabled.load(std::memory_order_relaxed); }
    void setEnabled(bool enable) {
        enabled.store(enable, std::memory_order_relaxed);
    }

private:
    ImageFilterPlugin* pluginPtr;
    std::atomic<bool> enabled;
};

class ImageFilterPlugin {
public:
    virtual QString name() = 0;
    virtual ImageFilter* makeFilter() = 0;
    static ImageFilter* makeFilter(QString name);
};

class ImageFilterSettingsDialog : public QDockWidget {
    Q_OBJECT

public:
    explicit ImageFilterSettingsDialog(ImageFilterSettingsWidget* settings,
                                       QWidget* parent = 0,
                                       Qt::WindowFlags f = 0);

private slots:
    void accept();
    void reject();
    void apply();

private:
    ImageFilterSettingsWidget* settings;
};

}

Q_DECLARE_INTERFACE(QArv::ImageFilterPlugin,
                    "si.ad-vega.qarv.QArvImageFilterPlugin/0.1")
Q_DECLARE_METATYPE(QArv::ImageFilterPlugin*)
Q_DECLARE_METATYPE(QArv::ImageFilter*)
Q_DECLARE_METATYPE(QArv::ImageFilterSettingsWidget*)
Q_DECLARE_METATYPE(QArv::ImageFilterSettingsDialog*)
Q_DECLARE_METATYPE(QVector<QArv::ImageFilterPtr>)

#endif
