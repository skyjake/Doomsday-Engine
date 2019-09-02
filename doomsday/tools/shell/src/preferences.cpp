#include "preferences.h"
#include "folderselection.h"
#include "guishellapp.h"

#include <de/Config>
#include <de/ToggleWidget>

//#ifdef MACOSX
//#  define PREFS_APPLY_IMMEDIATELY
//#endif

using namespace de;

DE_GUI_PIMPL(Preferences)
{
    FolderSelection *appFolder;
    ToggleWidget *   useCustomIwad;
    FolderSelection *iwadFolder;

    Impl(Public &i) : Base(i)
    {
        auto &cfg = Config::get();

        AutoRef<Rule> dialogWidth = rule("unit") * 75;

        LabelWidget *appFolderInfo;
#if defined (MACOSX)
        appFolder = &self().area().addNew<FolderSelection>("Doomsday.app Folder");
        appFolderInfo =
            LabelWidget::newWithText("Shell needs to know where Doomsday.app is located "
                                     "to be able to start local servers. The server "
                                     "executable is located inside the Doomsday.app "
                                     "bundle.",
                                     &self().area());
#else
        appFolder    = &self().area().addNew<FolderSelection>("Executable Folder");
        appFolderInfo = LabelWidget::newWithText("The server executable in this folder "
                                                 "is used for starting local servers.",
                                                 &self().area());
#endif
        appFolder->setPath(cfg.gets("Preferences.appFolder", ""));
        appFolder->rule().setInput(Rule::Width, dialogWidth);

        appFolderInfo->setMaximumTextWidth(dialogWidth);
        appFolderInfo->setAlignment(ui::AlignLeft);
        appFolderInfo->setTextLineAlignment(ui::AlignLeft);
        appFolderInfo->setFont("small");
        appFolderInfo->setTextColor("altaccent");

        // Game Data options.
        useCustomIwad = &self().area().addNew<ToggleWidget>();
        useCustomIwad->setText("Use a custom IWAD folder");
        useCustomIwad->setActive(cfg.getb("Preferences.customIwad", false));

        iwadFolder = &self().area().addNew<FolderSelection>("Select IWAD Folder");
        iwadFolder->rule().setInput(Rule::Width, dialogWidth);
        iwadFolder->setPath(cfg.gets("Preferences.iwadFolder", ""));

        // QLabel *info = new QLabel("<small>" +
        //             tr("Doomsday tries to locate game data such as "
        //                "<a href=\"http://dengine.net/dew/index.php?title=IWAD_folder\">IWAD files</a> "
        //                "automatically, but that may fail "
        //                "if you have the files in a custom location.") + "</small>");


        GridLayout layout(self().area().contentRule().left(), self().area().contentRule().top());
        layout.setGridSize(2, 0);
        layout.setColumnAlignment(0, ui::AlignRight);

        LabelWidget::appendSeparatorWithText("Server Location", &self().area(), &layout);
        layout.append(*appFolder, 2)
              .append(*appFolderInfo, 2);
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
    d->useCustomIwad->audienceForToggle() += [this]() { validate(); };
    audienceForAccept() += [this]() { saveState(); };
    validate();
}

de::NativePath Preferences::iwadFolder()
{
    auto &cfg = Config::get();
    if (cfg.getb("Preferences.customIwad", false))
    {
        return cfg.gets("Preferences.iwadFolder", "");
    }
    return {};
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
    auto &cfg = Config::get();

    //    st.setValue("Preferences/appFolder", convert(d->appFolder->path()));
    cfg.set("Preferences.customIwad", d->useCustomIwad->isActive());
    cfg.set("Preferences.iwadFolder", d->iwadFolder->path().toString());

    //    st.setValue("Preferences/consoleFont", d->consoleFont.toString());
//    emit consoleFontChanged();
}

void Preferences::validate()
{
    d->iwadFolder->setEnabled(d->useCustomIwad->isActive());
}
