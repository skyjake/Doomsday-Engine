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
    QCheckBox *useCustomIwad;
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

        useCustomIwad = new QCheckBox(tr("Use a custom IWAD folder"));
        useCustomIwad->setChecked(st.value("Preferences/customIwad", false).toBool());
        useCustomIwad->setToolTip(tr("Doomsday's default IWAD folder can be configured\n"
                                     "using configuration files, environment variables,\n"
                                     "or command line options."));

        iwadFolder = new FolderSelection(tr("Select IWAD Folder"));
        iwadFolder->setPath(st.value("Preferences/iwadFolder").toString());

        QVBoxLayout *bl = new QVBoxLayout;
        bl->addWidget(useCustomIwad);
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
    connect(d->useCustomIwad, SIGNAL(toggled(bool)), this, SLOT(validate()));
    connect(this, SIGNAL(accepted()), this, SLOT(saveState()));
    validate();
}

Preferences::~Preferences()
{
    delete d;
}

de::NativePath Preferences::iwadFolder() const
{
    if(d->useCustomIwad->isChecked())
    {
        return d->iwadFolder->path();
    }
    return "";
}

void Preferences::saveState()
{
    QSettings st;
    st.setValue("Preferences/customIwad", d->useCustomIwad->isChecked());
    st.setValue("Preferences/iwadFolder", d->iwadFolder->path().toString());
}

void Preferences::validate()
{
    d->iwadFolder->setEnabled(d->useCustomIwad->isChecked());
}
