#include "preferences.h"
#include "folderselection.h"
#include <de/libdeng2.h>
#include <QCheckBox>
#include <QFormLayout>
#include <QSettings>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QGroupBox>
#include <QLabel>
#include <QFontDialog>

DENG2_PIMPL(Preferences)
{
    QCheckBox *useCustomIwad;
    FolderSelection *iwadFolder;
    QFont consoleFont;
    QLabel *fontDesc;

    Instance(Public &i) : Base(i), consoleFont(defaultConsoleFont())
    {
        QSettings st;
        if(st.contains("Preferences/consoleFont"))
        {
            consoleFont.fromString(st.value("Preferences/consoleFont").toString());
        }

        self.setWindowTitle(tr("Preferences"));

        QVBoxLayout *mainLayout = new QVBoxLayout;
        self.setLayout(mainLayout);

        mainLayout->addStretch(1);

        QGroupBox *fontGroup = new QGroupBox(tr("Console Font"));
        mainLayout->addWidget(fontGroup);

        fontDesc = new QLabel;

        QPushButton *selFont = new QPushButton(tr("Select..."));
        QObject::connect(selFont, SIGNAL(clicked()), thisPublic, SLOT(selectFont()));

        QHBoxLayout *fl = new QHBoxLayout;
        fl->addWidget(fontDesc, 1);
        fl->addWidget(selFont, 0);
        fontGroup->setLayout(fl);

        updateFontDesc();

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

    void updateFontDesc()
    {
        fontDesc->setText(QString("%1 %2 pt.").arg(consoleFont.family()).arg(consoleFont.pointSize()));
        fontDesc->setFont(consoleFont);
    }

    static QFont defaultConsoleFont()
    {
        QFont font;
#ifdef MACOSX
        font = QFont("Menlo", 13);
        if(!font.exactMatch())
        {
            font = QFont("Monaco", 12);
        }
#elif WIN32
        font = QFont("Fixedsys", 9);
#else
        font = QFont("Monospace", 11);
#endif
        return font;
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

de::NativePath Preferences::iwadFolder()
{
    QSettings st;
    if(st.value("Preferences/customIwad", false).toBool())
    {
        return st.value("Preferences/iwadFolder").toString();
    }
    return "";
}

QFont Preferences::consoleFont()
{
    QFont font;
    font.fromString(QSettings().value("Preferences/consoleFont",
                                      Instance::defaultConsoleFont().toString()).toString());
    return font;
}

void Preferences::saveState()
{
    QSettings st;
    st.setValue("Preferences/customIwad", d->useCustomIwad->isChecked());
    st.setValue("Preferences/iwadFolder", d->iwadFolder->path().toString());
    st.setValue("Preferences/consoleFont", d->consoleFont.toString());

    emit consoleFontChanged();
}

void Preferences::validate()
{
    d->iwadFolder->setEnabled(d->useCustomIwad->isChecked());
}

void Preferences::selectFont()
{
    bool ok;
    QFont font = QFontDialog::getFont(&ok, d->consoleFont, this);
    if(ok)
    {
        d->consoleFont = font;
        d->updateFontDesc();
    }
}
