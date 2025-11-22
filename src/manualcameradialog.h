/*
    QArv, a Qt interface to aravis.
    Copyright (C) 2012-2025 Jure Varlec <jure.varlec@ad-vega.si>

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

#ifndef MANUALCAMERADIALOG_H
#define MANUALCAMERADIALOG_H

#include <QDialog>
#include <QListWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>

namespace QArv
{

class ManualCameraDialog : public QDialog {
    Q_OBJECT

public:
    explicit ManualCameraDialog(QWidget* parent = nullptr);
    ~ManualCameraDialog();

private slots:
    void onAddressTextChanged(const QString &text);
    void onAddButtonClicked();
    void onRemoveButtonClicked();
    void onItemDoubleClicked(QListWidgetItem* item);
    void onSelectionChanged();
    void accept() override;

private:
    void loadSettings();
    void saveSettings();
    bool isValidAddress(const QString &address) const;

    QListWidget* listWidget;
    QLineEdit* addressEdit;
    QPushButton* addButton;
    QPushButton* removeButton;
    QLabel* validationLabel;
};

} // namespace QArv

#endif // MANUALCAMERADIALOG_H
