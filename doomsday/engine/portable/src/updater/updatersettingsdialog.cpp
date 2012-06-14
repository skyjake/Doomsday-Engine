#include "updatersettingsdialog.h"
#include "updatersettings.h"
#include <QDesktopServices>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <de/Log>
#include <QDebug>

static QString defaultLocationName()
{
    QString name = QDesktopServices::displayName(QDesktopServices::TempLocation);
    if(name.isEmpty())
    {
        name = "Temporary Files";
    }
    return name;
}

struct UpdaterSettingsDialog::Instance
{
    UpdaterSettingsDialog* self;
    QCheckBox* autoCheck;
    QComboBox* freqList;
    QLabel* lastChecked;
    QComboBox* channelList;
    QComboBox* pathList;
    QCheckBox* deleteAfter;

    Instance(UpdaterSettingsDialog* dlg) : self(dlg)
    {
        self->setWindowFlags(self->windowFlags() & ~Qt::WindowContextHelpButtonHint);
        self->setWindowTitle(tr("Updater Settings"));

        QVBoxLayout* mainLayout = new QVBoxLayout;
        self->setLayout(mainLayout);

        QFormLayout* form = new QFormLayout;
        mainLayout->addLayout(form);

        freqList = new QComboBox;
        freqList->addItem(tr("At startup"), UpdaterSettings::AtStartup);
        freqList->addItem(tr("Daily"),      UpdaterSettings::Daily);
        freqList->addItem(tr("Biweekly"),   UpdaterSettings::Biweekly);
        freqList->addItem(tr("Weekly"),     UpdaterSettings::Weekly);
        freqList->addItem(tr("Monthly"),    UpdaterSettings::Monthly);

        autoCheck = new QCheckBox(tr("&Check for updates:"));
        form->addRow(autoCheck, freqList);
        QLayoutItem* item = form->itemAt(0, QFormLayout::LabelRole);
        item->setAlignment(Qt::AlignVCenter);

        lastChecked = new QLabel;
        form->addRow(new QWidget, lastChecked);

        channelList = new QComboBox;
        channelList->addItem(tr("Stable"), UpdaterSettings::Stable);
        channelList->addItem(tr("Unstable/Candidate"), UpdaterSettings::Unstable);
        form->addRow(tr("&Release type:"), channelList);

        pathList = new QComboBox;
        pathList->addItem(defaultLocationName(),
                          UpdaterSettings::defaultDownloadPath());
        pathList->addItem(tr("Select folder..."), "");
        form->addRow(tr("&Download location:"), pathList);

        deleteAfter = new QCheckBox(tr("D&elete file after install"));
        form->addRow(new QWidget, deleteAfter);

        QDialogButtonBox* bbox = new QDialogButtonBox;
        mainLayout->addWidget(bbox);

        // Buttons.
        QPushButton* ok = bbox->addButton(QDialogButtonBox::Ok);
        QPushButton* cancel = bbox->addButton(QDialogButtonBox::Cancel);

        fetch();

        // Connections.
        QObject::connect(autoCheck, SIGNAL(toggled(bool)), self, SLOT(autoCheckToggled(bool)));
        QObject::connect(pathList, SIGNAL(activated(int)), self, SLOT(pathActivated(int)));
        QObject::connect(ok, SIGNAL(clicked()), self, SLOT(accept()));
        QObject::connect(cancel, SIGNAL(clicked()), self, SLOT(reject()));
    }

    void fetch()
    {
        UpdaterSettings st;

        lastChecked->setText(tr("<small>Last checked on %1.</small>")
                             .arg(st.lastCheckTime().asText(de::Time::FriendlyFormat)));

        autoCheck->setChecked(!st.onlyCheckManually());
        freqList->setEnabled(!st.onlyCheckManually());
        freqList->setCurrentIndex(freqList->findData(st.frequency()));
        channelList->setCurrentIndex(channelList->findData(st.channel()));
        setDownloadPath(st.downloadPath());
        deleteAfter->setChecked(st.deleteAfterUpdate());
    }

    void apply()
    {
        UpdaterSettings st;
        st.setOnlyCheckManually(!autoCheck->isChecked());
        int sel = freqList->currentIndex();
        if(sel >= 0)
        {
            st.setFrequency(UpdaterSettings::Frequency(freqList->itemData(sel).toInt()));
        }
        sel = channelList->currentIndex();
        if(sel >= 0)
        {
            st.setChannel(UpdaterSettings::Channel(channelList->itemData(sel).toInt()));
        }
        st.setDownloadPath(pathList->itemData(pathList->currentIndex()).toString());
        st.setDeleteAfterUpdate(deleteAfter->isChecked());
    }

    void setDownloadPath(const QString& dir)
    {
        if(dir != UpdaterSettings::defaultDownloadPath())
        {
            // Remove extra items.
            while(pathList->count() > 2)
            {
                pathList->removeItem(0);
            }
            pathList->insertItem(0, QDir(dir).dirName(), dir);
            pathList->setCurrentIndex(0);
        }
        else
        {
            pathList->setCurrentIndex(pathList->findData(dir));
        }
    }
};

UpdaterSettingsDialog::UpdaterSettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    d = new Instance(this);
}

UpdaterSettingsDialog::~UpdaterSettingsDialog()
{
    delete d;
}

void UpdaterSettingsDialog::fetch()
{
    d->fetch();
}

void UpdaterSettingsDialog::accept()
{
    d->apply();
    QDialog::accept();
}

void UpdaterSettingsDialog::reject()
{
    QDialog::reject();
}

void UpdaterSettingsDialog::autoCheckToggled(bool set)
{
    d->freqList->setEnabled(set);
}

void UpdaterSettingsDialog::pathActivated(int index)
{
    QString path = d->pathList->itemData(index).toString();
    if(path.isEmpty())
    {
        QString dir = QFileDialog::getExistingDirectory(this, tr("Download Folder"), QDir::homePath());
        if(!dir.isEmpty())
        {
            d->setDownloadPath(dir);
        }
        else
        {
            d->setDownloadPath(UpdaterSettings::defaultDownloadPath());
        }
    }
}
