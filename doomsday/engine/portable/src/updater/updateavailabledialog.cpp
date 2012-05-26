#include "updateavailabledialog.h"
#include "versioninfo.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QCheckBox>
#include <QPushButton>
#include <QFont>
#include <QLabel>

struct UpdateAvailableDialog::Instance
{
    UpdateAvailableDialog* self;

    QLabel* info;

    Instance(UpdateAvailableDialog* d) : self(d)
    {
        QVBoxLayout* mainLayout = new QVBoxLayout(self);
        self->setLayout(mainLayout);
        //mainLayout->setMargin(12);

        info = new QLabel;
        info->setTextFormat(Qt::RichText);
        info->setText(QString("<span style=\"font-weight:bold; font-size:%1pt;\">You are up to date.</span>"
                              "<p>The installed version %2 is the latest available stable build.")
                      .arg(int(self->font().pointSize() * 1.2))
                      .arg(VersionInfo().asText()));

        QCheckBox* neverCheck = new QCheckBox("Never check for updates automatically");

        QDialogButtonBox* bbox = new QDialogButtonBox;

        QPushButton* ok = bbox->addButton("Ok", QDialogButtonBox::AcceptRole);
        ok->setDefault(true);

        QPushButton* cfg = bbox->addButton("Settings...", QDialogButtonBox::ActionRole);
        cfg->setAutoDefault(false);
        cfg->setDefault(false);

        //QPushButton* whatsNew = bbox->addButton("What's new?", QDialogButtonBox::HelpRole);

        mainLayout->addWidget(info);
        mainLayout->addWidget(neverCheck);
        mainLayout->addWidget(bbox);
    }
};

UpdateAvailableDialog::UpdateAvailableDialog(QWidget *parent)
    : QDialog(parent)
{
    d = new Instance(this);
}

UpdateAvailableDialog::~UpdateAvailableDialog()
{
    delete d;
}
