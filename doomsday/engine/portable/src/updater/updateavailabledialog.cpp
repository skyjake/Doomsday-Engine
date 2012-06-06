#include "updateavailabledialog.h"
#include "updatersettings.h"
#include "updatersettingsdialog.h"
#include "versioninfo.h"
#include <de/Log>
#include <QUrl>
#include <QDesktopServices>
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
    QCheckBox* neverCheck;
    VersionInfo latestVersion;
    de::String changeLog;

    Instance(UpdateAvailableDialog* d) : self(d)
    {
        QVBoxLayout* mainLayout = new QVBoxLayout(self);
        self->setLayout(mainLayout);

        info = new QLabel;
        info->setTextFormat(Qt::RichText);
        VersionInfo currentVersion;
        int bigFontSize = self->font().pointSize() * 1.2;
        de::String channel = (UpdaterSettings().channel() == UpdaterSettings::Stable?
                                  "stable" : "unstable");
        bool askUpgrade = false;

        if(latestVersion > currentVersion)
        {
            askUpgrade = true;
            info->setText(("<span style=\"font-weight:bold; font-size:%1pt;\">" +
                           tr("There is an update available.") + "</span><p>" +
                           tr("The latest %2 release is %3, while you are running %4."))
                          .arg(bigFontSize)
                          .arg(channel)
                          .arg(latestVersion.asText())
                          .arg(currentVersion.asText()));
        }
        else
        {
            info->setText(("<span style=\"font-weight:bold; font-size:%1pt;\">" +
                           tr("You are up to date.") + "</span><p>" +
                           tr("The installed %2 is the latest available %3 build."))
                          .arg(bigFontSize)
                          .arg(currentVersion.asText())
                          .arg(channel));
        }

        neverCheck = new QCheckBox(tr("N&ever check for updates automatically"));
        neverCheck->setChecked(UpdaterSettings().onlyCheckManually());
        QObject::connect(neverCheck, SIGNAL(toggled(bool)), self, SLOT(neverCheckToggled(bool)));

        QDialogButtonBox* bbox = new QDialogButtonBox;

        if(!askUpgrade)
        {
            QPushButton* ok = bbox->addButton(tr("&Close"), QDialogButtonBox::RejectRole);
            QObject::connect(ok, SIGNAL(clicked()), self, SLOT(reject()));
        }
        else
        {
            QPushButton* yes = bbox->addButton(tr("&Download Update"), QDialogButtonBox::YesRole);
            QPushButton* no = bbox->addButton(tr("&Not Now"), QDialogButtonBox::NoRole);
            QObject::connect(yes, SIGNAL(clicked()), self, SLOT(accept()));
            QObject::connect(no, SIGNAL(clicked()), self, SLOT(reject()));
        }

        QPushButton* cfg = bbox->addButton(tr("&Settings..."), QDialogButtonBox::ActionRole);
        QObject::connect(cfg, SIGNAL(clicked()), self, SLOT(editSettings()));
        cfg->setAutoDefault(false);

        if(askUpgrade)
        {
            QPushButton* whatsNew = bbox->addButton(tr("&What's New?"), QDialogButtonBox::HelpRole);
            QObject::connect(whatsNew, SIGNAL(clicked()), self, SLOT(showWhatsNew()));
            whatsNew->setAutoDefault(false);
        }

        mainLayout->addWidget(info);
        mainLayout->addWidget(neverCheck);
        mainLayout->addWidget(bbox);
    }
};

UpdateAvailableDialog::UpdateAvailableDialog(const VersionInfo& latestVersion, de::String changeLogUri,
                                             QWidget *parent)
    : QDialog(parent)
{
    d = new Instance(this);
    d->latestVersion = latestVersion;
    d->changeLog = changeLogUri;
}

UpdateAvailableDialog::~UpdateAvailableDialog()
{
    delete d;
}

void UpdateAvailableDialog::neverCheckToggled(bool set)
{
    LOG_DEBUG("Never check for updates: %b") << set;
    UpdaterSettings().setOnlyCheckManually(set);
}

void UpdateAvailableDialog::showWhatsNew()
{
    QDesktopServices::openUrl(QUrl(d->changeLog));
}

void UpdateAvailableDialog::editSettings()
{
    UpdaterSettingsDialog st;
    if(st.exec())
    {
        d->neverCheck->setChecked(UpdaterSettings().onlyCheckManually());
    }
}
