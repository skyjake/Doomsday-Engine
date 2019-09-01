#include "preferences.h"
#include "folderselection.h"
#include "guishellapp.h"

#include <de/Config>
#include <de/VariableToggleWidget>

//#ifdef MACOSX
//#  define PREFS_APPLY_IMMEDIATELY
//#endif

using namespace de;

DE_GUI_PIMPL(Preferences)
{
    FolderSelection *     appFolder;
    VariableToggleWidget *useCustomIwad;
    FolderSelection *     iwadFolder;

    Impl(Public &i) : Base(i)
    {
        auto &cfg = Config::get();
        if (!cfg.has("Preferences.customIwad"))
        {
            cfg.set("Preferences.customIwad", false);
        }

        //        QGroupBox *fontGroup = new QGroupBox(tr("Console Font"));
        //        {
        //            mainLayout->addWidget(fontGroup);

        //            fontDesc = new QLabel;

        //            QPushButton *selFont = new QPushButton(tr("Select..."));
        //            selFont->setAutoDefault(false);
        //            QObject::connect(selFont, SIGNAL(clicked()), thisPublic, SLOT(selectFont()));

        //            QHBoxLayout *fl = new QHBoxLayout;
        //            fl->addWidget(fontDesc, 1);
        //            fl->addWidget(selFont, 0);
        //            fontGroup->setLayout(fl);
        //        }
        //        updateFontDesc();

        //        QGroupBox *appGroup = new QGroupBox(tr("Server Location"));
        //        {
        //            mainLayout->addWidget(appGroup);

        //#ifdef MACOSX
        //            appFolder = new FolderSelection(tr("Doomsday.app Folder"));
        //            QLabel *info = new QLabel("<small>" + tr("Shell needs to know where Doomsday.app is located "
        //                                                     "to be able to start local servers. The server "
        //                                                     "executable is located inside the Doomsday.app "
        //                                                     "bundle.") +
        //                                      "</small>");
        //#else
        //            appFolder = new FolderSelection(tr("Executable Folder"));
        //            QLabel *info = new QLabel("<small>" + tr("The server executable in this folder "
        //                                                     "is used for starting local servers.") +
        //                                      "</small>");
        //#endif
        //            info->setWordWrap(true);

        //            appFolder->setPath(convert(st.value("Preferences/appFolder").toString()));

        //            QVBoxLayout *bl = new QVBoxLayout;
        //            bl->setSpacing(4);
        //            bl->addWidget(appFolder);
        //            bl->addWidget(info);
        //            appGroup->setLayout(bl);
        //        }

        //        QGroupBox *group = new QGroupBox(tr("Game Data"));
        //        {
        //            mainLayout->addWidget(group);

        //            useCustomIwad = new QCheckBox(tr("Use a custom IWAD folder"));
        //            useCustomIwad->setChecked(st.value("Preferences/customIwad", false).toBool());
        //            useCustomIwad->setToolTip(tr("Doomsday's default IWAD folder can be configured\n"
        //                                         "using configuration files, environment variables,\n"
        //                                         "or command line options."));

        // Game Data options.

        useCustomIwad = &self().area().addNew<VariableToggleWidget>("Use a custom IWAD folder",
                                                                    cfg["Preferences.customIwad"]);

        iwadFolder = &self().area().addNew<FolderSelection>("Select IWAD Folder");
        iwadFolder->rule().setInput(Rule::Width, rule("unit") * 75);
        iwadFolder->setPath(cfg.gets("Preferences.iwadFolder", ""));

        //            QVBoxLayout *bl = new QVBoxLayout;
        //            bl->setSpacing(4);
        //            bl->addWidget(useCustomIwad);
        //            bl->addWidget(iwadFolder);
        //            QLabel *info = new QLabel("<small>" +
        //                        tr("Doomsday tries to locate game data such as "
        //                           "<a href=\"http://dengine.net/dew/index.php?title=IWAD_folder\">IWAD files</a> "
        //                           "automatically, but that may fail "
        //                           "if you have the files in a custom location.") + "</small>");
        //            QObject::connect(info, SIGNAL(linkActivated(QString)), &GuiShellApp::app(), SLOT(openWebAddress(QString)));
        //            info->setWordWrap(true);
        //            bl->addWidget(info);
        //            group->setLayout(bl);
        //        }

        //        mainLayout->addStretch(1);

        //#ifndef PREFS_APPLY_IMMEDIATELY

        //        // On macOS, changes to the preferences are applied immediately.
        //        // Other platforms use OK/Cancel buttons.

        //        // Buttons.
        //        QDialogButtonBox *bbox = new QDialogButtonBox;
        //        mainLayout->addWidget(bbox);
        //        QPushButton *yes = bbox->addButton(tr("&OK"), QDialogButtonBox::YesRole);
        //        QPushButton *no = bbox->addButton(tr("&Cancel"), QDialogButtonBox::RejectRole);
        //        QObject::connect(yes, SIGNAL(clicked()), thisPublic, SLOT(accept()));
        //        QObject::connect(no, SIGNAL(clicked()), thisPublic, SLOT(reject()));
        //        yes->setDefault(true);
        //#endif

                    GridLayout layout(self().area().contentRule().left(), self().area().contentRule().top());
                    layout.setGridSize(2, 0);
                    layout.setColumnAlignment(0, ui::AlignRight);

                    LabelWidget::appendSeparatorWithText("Game Data", &self().area(), &layout);
                    layout << Const(0) << *useCustomIwad;
                    layout.append(*iwadFolder, 2);

                    self().area().setContentSize(layout);

        self().buttons()
            << new DialogButtonItem(Accept | Default, "Apply")
            << new DialogButtonItem(Reject, "Cancel");
    }

#if 0
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
#elif WIN32
        font = QFont("Courier New", 10);
#else
        font = QFont("Monospace", 11);
#endif
        return font;
    }
#endif
};

Preferences::Preferences()
    : DialogWidget("Preferences", WithHeading)
    , d(new Impl(*this))
{
    heading().setText("Preferences");
//    connect(d->useCustomIwad, SIGNAL(toggled(bool)), this, SLOT(validate()));
//    connect(this, SIGNAL(accepted()), this, SLOT(saveState()));
//#ifdef PREFS_APPLY_IMMEDIATELY
//    connect(d->iwadFolder, SIGNAL(selected()), this, SLOT(saveState()));
//    connect(d->appFolder, SIGNAL(selected()), this, SLOT(saveState()));
//#endif
    validate();
}

de::NativePath Preferences::iwadFolder()
{
    auto &cfg = Config::get();
    if (cfg.getb("Preferences.customIwad", false))
    {
        return cfg.gets("Preferences.iwadFolder", "");
    }
    return "";
}

//QFont Preferences::consoleFont()
//{
//    QFont font;
//    font.fromString(QSettings().value("Preferences/consoleFont",
//                                      Impl::defaultConsoleFont().toString()).toString());
//    return font;
//}

void Preferences::saveState()
{
    //auto &cfg = Config::get();

    //    st.setValue("Preferences/appFolder", convert(d->appFolder->path()));
//    st.setValue("Preferences/customIwad", d->useCustomIwad->isChecked());
//    st.setValue("Preferences/iwadFolder", convert(d->iwadFolder->path()));
//    st.setValue("Preferences/consoleFont", d->consoleFont.toString());

//    emit consoleFontChanged();
}

void Preferences::validate()
{
    //d->iwadFolder->setEnabled(d->useCustomIwad->isChecked());
}
