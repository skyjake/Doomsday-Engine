#include "updatersettingsdialog.h"
#include "updatersettings.h"
#include <QDesktopServices>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QPushButton>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <de/Log>

struct UpdaterSettingsDialog::Instance
{
    UpdaterSettingsDialog* self;
    QCheckBox* neverCheck;
    QComboBox* freqList;
    QComboBox* channelList;
    QComboBox* pathList;
    QCheckBox* deleteAfter;

    Instance(UpdaterSettingsDialog* dlg) : self(dlg)
    {
        QVBoxLayout* mainLayout = new QVBoxLayout;
        self->setLayout(mainLayout);

        QFormLayout* form = new QFormLayout;
        mainLayout->addLayout(form);

/*
    bool deleteAfterUpdate() const;
    bool isDefaultDownloadPath() const;
    de::String downloadPath() const;
*/
        neverCheck = new QCheckBox("Never check for updates automatically");
        form->addRow(neverCheck);

        freqList = new QComboBox;
        freqList->addItem("Daily",    UpdaterSettings::Daily);
        freqList->addItem("Biweekly", UpdaterSettings::Biweekly);
        freqList->addItem("Weekly",   UpdaterSettings::Weekly);
        freqList->addItem("Monthly",  UpdaterSettings::Monthly);
        form->addRow("Check for updates:", freqList);

        channelList = new QComboBox;
        channelList->addItem("Stable", UpdaterSettings::Stable);
        channelList->addItem("Unstable/Candidate", UpdaterSettings::Unstable);
        form->addRow("Release type:", channelList);

        pathList = new QComboBox;
        pathList->addItem(QDesktopServices::displayName(QDesktopServices::TempLocation),
                          QDesktopServices::storageLocation(QDesktopServices::TempLocation));
        pathList->addItem("Select folder...", "");
        form->addRow("Download location:", pathList);

        deleteAfter = new QCheckBox("Delete file after install");
        form->addRow(new QWidget, deleteAfter);

        QDialogButtonBox* bbox = new QDialogButtonBox;
        mainLayout->addWidget(bbox);

        // Buttons.
        //QPushButton* apply = bbox->addButton(QDialogButtonBox::Apply);
        QPushButton* ok = bbox->addButton(QDialogButtonBox::Ok);
        QPushButton* cancel = bbox->addButton(QDialogButtonBox::Cancel);

        fetch();

        // Connections.
        QObject::connect(neverCheck, SIGNAL(toggled(bool)), self, SLOT(neverCheckToggled(bool)));
        QObject::connect(pathList, SIGNAL(activated(int)), self, SLOT(pathActivated(int)));
        QObject::connect(cancel, SIGNAL(clicked()), self, SLOT(reject()));
    }

    ~Instance()
    {

    }

    void fetch()
    {
        neverCheck->setChecked(UpdaterSettings().onlyCheckManually());
        freqList->setEnabled(!UpdaterSettings().onlyCheckManually());
        freqList->setCurrentIndex(freqList->findData(UpdaterSettings().frequency()));
        channelList->setCurrentIndex(channelList->findData(UpdaterSettings().channel()));
        deleteAfter->setChecked(UpdaterSettings().deleteAfterUpdate());
    }

    void apply()
    {

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

void UpdaterSettingsDialog::neverCheckToggled(bool set)
{
    d->freqList->setEnabled(!set);
}

void UpdaterSettingsDialog::pathActivated(int index)
{
    QString path = d->pathList->itemData(index).toString();
    if(path.isEmpty())
    {
        QString dir = QFileDialog::getExistingDirectory(this, "Download Folder", QDir::homePath());
        if(!dir.isEmpty())
        {
            // Remove extra items.
            while(d->pathList->count() > 2)
                d->pathList->removeItem(0);

            d->pathList->insertItem(0, QDir(dir).dirName(), dir);
            d->pathList->setCurrentIndex(0);
        }
    }
}
