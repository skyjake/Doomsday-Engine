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

#ifdef MACOSX
#  define PREFS_APPLY_IMMEDIATELY
#endif

DENG2_PIMPL(Preferences)
{
    QCheckBox *useCustomIwad;
    FolderSelection *iwadFolder;
    QFont consoleFont;
    QLabel *fontDesc;

    Instance(Public &i) : Base(i), consoleFont(defaultConsoleFont())
    {
#ifndef MACOSX
        self.setWindowModality(Qt::ApplicationModal);
#endif
#ifdef WIN32
        self.setWindowFlags(self.windowFlags() & ~Qt::WindowContextHelpButtonHint);
#endif

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
        selFont->setAutoDefault(false);
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

#ifndef PREFS_APPLY_IMMEDIATELY

        // On Mac, changes to the preferences are applied immediately.
        // Other platforms use OK/Cancel buttons.

        // Buttons.
        QDialogButtonBox *bbox = new QDialogButtonBox;
        mainLayout->addWidget(bbox);
        QPushButton *yes = bbox->addButton(tr("&OK"), QDialogButtonBox::YesRole);
        QPushButton *no = bbox->addButton(tr("&Cancel"), QDialogButtonBox::RejectRole);
        //QPushButton *act = bbox->addButton(tr("&Apply"), QDialogButtonBox::ActionRole);
        QObject::connect(yes, SIGNAL(clicked()), &self, SLOT(accept()));
        QObject::connect(no, SIGNAL(clicked()), &self, SLOT(reject()));
        //QObject::connect(act, SIGNAL(clicked()), &self, SLOT(saveState()));
        yes->setDefault(true);
#endif
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
# ifdef MACOS_10_4
        font = QFont("Monaco", 12);
# else
        font = QFont("Menlo", 13);
# endif
#elif WIN32
        font = QFont("Courier New", 10);
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
#ifdef PREFS_APPLY_IMMEDIATELY
    connect(d->iwadFolder, SIGNAL(selected()), this, SLOT(saveState()));
#endif
    validate();
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

#ifdef PREFS_APPLY_IMMEDIATELY
        saveState();
#endif
    }
}
