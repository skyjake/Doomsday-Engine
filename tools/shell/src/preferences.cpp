#include "preferences.h"
#include "folderselection.h"
#include "guishellapp.h"

#include <de/config.h>
#include <de/sequentiallayout.h>
#include <de/togglewidget.h>

//#ifdef MACOSX
//#  define PREFS_APPLY_IMMEDIATELY
//#endif

using namespace de;

DE_GUI_PIMPL(Preferences)
{
    FolderSelection *appFolder;
    ToggleWidget *   useCustomIwad;
    FolderSelection *iwadFolder;
    ToggleWidget *   recurseIwad;

    Impl(Public &i) : Base(i)
    {
        auto &cfg = Config::get();

        AutoRef<Rule> dialogWidth = rule("unit") * 100;

        LabelWidget *appFolderInfo;
#if defined (MACOSX)
        appFolder = &self().area().addNew<FolderSelection>("Doomsday.app Folder");
        appFolderInfo =
            LabelWidget::newWithText("Shell needs to know where Doomsday.app is located "
                                     "to be able to start local servers, because the "
                                     "doomsday-server executable "
                                     "is in the Doomsday.app bundle.",
                                     &self().area());
#else
        appFolder    = &self().area().addNew<FolderSelection>("Executable Folder");
        appFolderInfo = LabelWidget::newWithText("The server executable in this folder "
                                                 "is used when starting local servers.",
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
        useCustomIwad->setAlignment(ui::AlignLeft);
        useCustomIwad->setActive(cfg.getb("Preferences.customIwad", false));

        iwadFolder = &self().area().addNew<FolderSelection>("Select IWAD Folder");
        iwadFolder->rule().setInput(Rule::Width, dialogWidth);
        iwadFolder->setPath(cfg.gets("Preferences.iwadFolder", ""));

        recurseIwad = &self().area().addNew<ToggleWidget>();
        recurseIwad->setText("Include subdirectories");
        recurseIwad->setAlignment(ui::AlignLeft);
        recurseIwad->setActive(cfg.getb("Preferences.recurseIwad", false));
        
        SequentialLayout layout(
            self().area().contentRule().left(), self().area().contentRule().top(), ui::Down);
        //layout.setGridSize(1, 0);
        layout.setOverrideWidth(dialogWidth);
        //        layout.setColumnAlignment(0, ui::AlignRight);

        layout << *LabelWidget::appendSeparatorWithText("Server Location", &self().area());
        layout << *appFolder << *appFolderInfo;
        layout << *LabelWidget::appendSeparatorWithText("Game Data", &self().area());
        layout << *useCustomIwad;
        layout << *iwadFolder << *recurseIwad;

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

NativePath Preferences::iwadFolder()
{
    auto &cfg = Config::get();
    if (cfg.getb("Preferences.customIwad", false))
    {
        return cfg.gets("Preferences.iwadFolder", "");
    }
    return {};
}

bool Preferences::isIwadFolderRecursive()
{
    return Config::get().getb("Preferences.recurseIwad", false);
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

    cfg.set("Preferences.appFolder", d->appFolder->path().toString());
    cfg.set("Preferences.customIwad", d->useCustomIwad->isActive());
    cfg.set("Preferences.iwadFolder", d->iwadFolder->path().toString());
    cfg.set("Preferences.recurseIwad", d->recurseIwad->isActive());

    //    st.setValue("Preferences/consoleFont", d->consoleFont.toString());
//    emit consoleFontChanged();
}

void Preferences::validate()
{
    d->iwadFolder->setEnabled(d->useCustomIwad->isActive());
    d->recurseIwad->enable(d->useCustomIwad->isActive());
}
