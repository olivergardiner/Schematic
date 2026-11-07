#include "preferencesdialog.h"

#include <QDialogButtonBox>
#include <QTabWidget>
#include <QVBoxLayout>

PreferencesDialog::PreferencesDialog(QWidget *parent)
    : QDialog(parent)
{
    setObjectName("preferencesDialog");
    setWindowTitle(tr("Preferences"));
    resize(480, 320);

    auto *layout = new QVBoxLayout(this);
    auto *tabs = new QTabWidget(this);
    tabs->setObjectName("preferencesTabs");
    auto *general = new QWidget(tabs);
    general->setObjectName("generalPreferencesPage");
    new QVBoxLayout(general);
    tabs->addTab(general, tr("General"));
    layout->addWidget(tabs);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}
