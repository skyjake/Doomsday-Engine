#include "preferences.h"
#include "folderselection.h"
#include <de/libdeng2.h>
#include <QCheckBox>
#include <QFormLayout>
#include <QSettings>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QGroupBox>

DENG2_PIMPL(Preferences)
{
    QCheckBox *useDefaultIwad;
    FolderSelection *iwadFolder;

    Instance(Public &i) : Base(i)
    {
        QSettings st;

        self.setWindowTitle(tr("Preferences"));

        QVBoxLayout *mainLayout = new QVBoxLayout;
        self.setLayout(mainLayout);

        mainLayout->addStretch(1);

        QGroupBox *group = new QGroupBox(tr("IWAD Folder"));
        mainLayout->addWidget(group);

        useDefaultIwad = new QCheckBox(tr("Use Doomsday's configured IWAD folder"));
        useDefaultIwad->setChecked(st.value("Preferences/defaultIwad", true).toBool());
        useDefaultIwad->setToolTip(tr("Doomsday's IWAD folder can be configured using "
                               "configuration files or environment variables."));

        iwadFolder = new FolderSelection(tr("Select IWAD Folder"));
        iwadFolder->setPath(st.value("Preferences/iwadFolder").toString());

        QVBoxLayout *bl = new QVBoxLayout;
        bl->addWidget(useDefaultIwad);
        bl->addWidget(iwadFolder);
        group->setLayout(bl);

        mainLayout->addStretch(1);

        // Buttons.
        QDialogButtonBox *bbox = new QDialogButtonBox;
        mainLayout->addWidget(bbox);
        QPushButton *yes = bbox->addButton(tr("&OK"), QDialogButtonBox::YesRole);
        QPushButton *no = bbox->addButton(tr("&Cancel"), QDialogButtonBox::RejectRole);
        QPushButton *act = bbox->addButton(tr("&Apply"), QDialogButtonBox::ActionRole);
        QObject::connect(yes, SIGNAL(clicked()), &self, SLOT(accept()));
        QObject::connect(no, SIGNAL(clicked()), &self, SLOT(reject()));
        QObject::connect(act, SIGNAL(clicked()), &self, SLOT(saveState()));
        yes->setDefault(true);
    }
};

Preferences::Preferences(QWidget *parent) :
    QDialog(parent), d(new Instance(*this))
{
    connect(d->useDefaultIwad, SIGNAL(toggled(bool)), this, SLOT(validate()));
    connect(this, SIGNAL(accepted()), this, SLOT(saveState()));
    validate();
}

Preferences::~Preferences()
{
    delete d;
}

de::NativePath Preferences::iwadFolder() const
{
    if(!d->useDefaultIwad->isChecked())
    {
        return d->iwadFolder->path();
    }
    return "";
}

void Preferences::saveState()
{
    QSettings st;
    st.setValue("Preferences/defaultIwad", d->useDefaultIwad->isChecked());
    st.setValue("Preferences/iwadFolder", iwadFolder().toString());
}

void Preferences::validate()
{
    d->iwadFolder->setDisabled(d->useDefaultIwad->isChecked());
}
