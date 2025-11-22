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

#include "globals.h"
#include "manualcameradialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDialogButtonBox>
#include <QSettings>
#include <QHostAddress>
#include <QRegularExpression>
#include <QMessageBox>

using namespace QArv;

ManualCameraDialog::ManualCameraDialog(QWidget* parent)
    : QDialog(parent) {
    setWindowTitle(tr("Manual Camera List"));
    setMinimumWidth(400);
    setMinimumHeight(300);

    // Create main layout
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // List widget label
    QLabel* listLabel = new QLabel(tr("Camera Addresses"), this);
    mainLayout->addWidget(listLabel);

    // List widget
    listWidget = new QListWidget(this);
    listWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
    mainLayout->addWidget(listWidget);

    // Input area layout
    QHBoxLayout* inputLayout = new QHBoxLayout();

    // Address input field
    addressEdit = new QLineEdit(this);
    addressEdit->setPlaceholderText(tr("Enter IP address or hostname"));
    addressEdit->setFocus();
    inputLayout->addWidget(addressEdit);

    // Add button
    addButton = new QPushButton(this);
    setButtonIcon(addButton, "list-add");
    addButton->setEnabled(false);
    addButton->setDefault(true);  // Enter key will add the address
    inputLayout->addWidget(addButton);

    // Remove button
    removeButton = new QPushButton(this);
    setButtonIcon(removeButton, "list-remove");
    removeButton->setEnabled(false);
    inputLayout->addWidget(removeButton);

    mainLayout->addLayout(inputLayout);

    // Validation feedback label
    validationLabel = new QLabel(this);
    validationLabel->setStyleSheet("QLabel { color: red; }");
    validationLabel->setWordWrap(true);
    mainLayout->addWidget(validationLabel);

    // Dialog buttons
    QDialogButtonBox* buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        this);
    mainLayout->addWidget(buttonBox);

    // Connect signals
    connect(addressEdit, &QLineEdit::textChanged,
            this, &ManualCameraDialog::onAddressTextChanged);
    connect(addressEdit, &QLineEdit::returnPressed,
            this, &ManualCameraDialog::onAddButtonClicked);
    connect(addButton, &QPushButton::clicked,
            this, &ManualCameraDialog::onAddButtonClicked);
    connect(removeButton, &QPushButton::clicked,
            this, &ManualCameraDialog::onRemoveButtonClicked);
    connect(listWidget, &QListWidget::itemDoubleClicked,
            this, &ManualCameraDialog::onItemDoubleClicked);
    connect(listWidget, &QListWidget::itemSelectionChanged,
            this, &ManualCameraDialog::onSelectionChanged);
    connect(buttonBox, &QDialogButtonBox::accepted,
            this, &ManualCameraDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected,
            this, &QDialog::reject);

    // Load existing addresses from settings
    loadSettings();
}

ManualCameraDialog::~ManualCameraDialog() {}

void ManualCameraDialog::loadSettings() {
    QSettings settings;
    QStringList addresses =
        settings.value("qarv_manual_cameras/addresses").toStringList();

    for (const QString& address : addresses) {
        QListWidgetItem* item = new QListWidgetItem(address, listWidget);
    }
}

void ManualCameraDialog::saveSettings() {
    QSettings settings;
    QStringList addresses;

    for (int i = 0; i < listWidget->count(); ++i) {
        addresses << listWidget->item(i)->text();
    }

    settings.setValue("qarv_manual_cameras/addresses", addresses);
}

bool ManualCameraDialog::isValidAddress(const QString& address) const {
    if (address.isEmpty()) {
        return false;
    }

    // Try parsing as IPv4 address
    QHostAddress ipaddr;
    if (ipaddr.setAddress(address)) {
        if (ipaddr.protocol() == QAbstractSocket::IPv4Protocol
            || ipaddr.protocol() == QAbstractSocket::IPv6Protocol) {
            return true;
        }
    }

    // Check if it's a valid hostname (RFC-1123/RFC-952)
    // Hostname rules: 1-63 chars per label, alphanumeric and hyphen,
    // cannot start or end with hyphen, labels separated by dots
    QRegularExpression hostnameRegex(
        "^([a-zA-Z0-9]([a-zA-Z0-9\\-]{0,61}[a-zA-Z0-9])?)(\\.([a-zA-Z0-9]([a-zA-Z0-9\\-]{0,61}[a-zA-Z0-9])?))*$"
        );

    return hostnameRegex.match(address).hasMatch();
}

void ManualCameraDialog::onAddressTextChanged(const QString& text) {
    bool valid = isValidAddress(text);
    addButton->setEnabled(valid);

    if (text.isEmpty()) {
        validationLabel->clear();
    } else if (!valid) {
        validationLabel->setText(tr("Invalid IP address or hostname"));
    } else {
        validationLabel->clear();
    }
}

void ManualCameraDialog::onAddButtonClicked() {
    QString address = addressEdit->text().trimmed();

    if (!isValidAddress(address)) {
        return;
    }

    // Check for duplicates
    for (int i = 0; i < listWidget->count(); ++i) {
        if (listWidget->item(i)->text() == address) {
            QMessageBox::warning(this, tr("Duplicate Address"),
                                 tr("This address is already in the list."));
            return;
        }
    }

    // Add the new address
    QListWidgetItem* item = new QListWidgetItem(address, listWidget);
    item->setFlags(item->flags() | Qt::ItemIsEditable);

    // Clear the input field
    addressEdit->clear();
    addressEdit->setFocus();
}

void ManualCameraDialog::onRemoveButtonClicked() {
    QList<QListWidgetItem*> selectedItems = listWidget->selectedItems();
    auto row = listWidget->currentRow();

    for (QListWidgetItem* item : selectedItems) {
        delete item;
    }

    listWidget->setCurrentRow(row);
}

void ManualCameraDialog::onItemDoubleClicked(QListWidgetItem* item) {
    // Enable editing mode
    listWidget->editItem(item);
}

void ManualCameraDialog::onSelectionChanged() {
    removeButton->setEnabled(!listWidget->selectedItems().isEmpty());
}

void ManualCameraDialog::accept() {
    saveSettings();
    QDialog::accept();
}
