/** @file audiosettingsdialog.cpp  Dialog for audio settings.
 *
 * @authors Copyright © 2013-2015 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2015 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "ui/dialogs/audiosettingsdialog.h"

#include "ui/widgets/cvarsliderwidget.h"
#include "ui/widgets/cvartogglewidget.h"
#include "ui/widgets/cvarchoicewidget.h"
#include "ui/widgets/cvarnativepathwidget.h"

#include "audio/system.h"

#include "clientapp.h"
#include "ConfigProfiles"

#include <de/SignalAction>
#include <de/GridPopupWidget>
#include <de/VariableChoiceWidget>

using namespace de;
using namespace de::ui;

DENG_GUI_PIMPL(AudioSettingsDialog)
{
    CVarSliderWidget *sfxVolume;
    CVarSliderWidget *musicVolume;
    CVarSliderWidget *reverbVolume;
    CVarToggleWidget *sound3D;
    CVarToggleWidget *overlapStop;
    CVarToggleWidget *sound16bit;
    CVarChoiceWidget *sampleRate;
    CVarChoiceWidget *musicSource;
    CVarNativePathWidget *musicSoundfont;
    CVarToggleWidget     *soundInfo;
    GridPopupWidget      *devPopup;
    VariableChoiceWidget *soundPlugin;
    VariableChoiceWidget *musicPlugin;
    VariableChoiceWidget *cdPlugin;

    Impl(Public *i) : Base(i)
    {
        ScrollAreaWidget &area = self.area();

        if (DoomsdayApp::isGameLoaded())
        {
            area.add(sfxVolume      = new CVarSliderWidget    ("sound-volume"));
            area.add(musicVolume    = new CVarSliderWidget    ("music-volume"));
            area.add(reverbVolume   = new CVarSliderWidget    ("sound-reverb-volume"));
            area.add(sound3D        = new CVarToggleWidget    ("sound-3d"));
            area.add(overlapStop    = new CVarToggleWidget    ("sound-overlap-stop"));
            area.add(sound16bit     = new CVarToggleWidget    ("sound-16bit"));
            area.add(sampleRate     = new CVarChoiceWidget    ("sound-rate"));
            area.add(musicSource    = new CVarChoiceWidget    ("music-source"));
            area.add(musicSoundfont = new CVarNativePathWidget("music-soundfont"));

            musicSoundfont->setBlankText(tr("System default"));
            musicSoundfont->setFilters(StringList()
                                       << "SF2 soundfonts (*.sf2)"
                                       << "DLS soundfonts (*.dls)"
                                       << "All files (*)");

            // Display volumes on a 0...100 scale.
            sfxVolume  ->setDisplayFactor(100.0 / 255.0);
            musicVolume->setDisplayFactor(100.0 / 255.0);
            sfxVolume  ->setStep(1.0 / sfxVolume->displayFactor());
            musicVolume->setStep(1.0 / musicVolume->displayFactor());

            // Developer options.
            self.add(devPopup = new GridPopupWidget);
            soundInfo = new CVarToggleWidget("sound-info", tr("Sound Channel Status"));
            *devPopup << soundInfo;
            devPopup->commit();
        }

        area.add(soundPlugin    = new VariableChoiceWidget(App::config("audio.soundPlugin")));
        area.add(musicPlugin    = new VariableChoiceWidget(App::config("audio.musicPlugin")));
        area.add(cdPlugin       = new VariableChoiceWidget(App::config("audio.cdPlugin")));

        soundPlugin->items()
                << new ChoiceItem(tr("FMOD"), "fmod")
           #if !defined (DENG_DISABLE_SDLMIXER)
                << new ChoiceItem(tr("SDL_mixer"), "sdlmixer")
           #endif
                << new ChoiceItem(tr("OpenAL"), "openal")
           #if defined (WIN32)
                << new ChoiceItem(tr("DirectSound"), "dsound")
           #endif
                << new ChoiceItem(tr("Disabled"), "dummy");

        musicPlugin->items()
           #if defined (UNIX)
                << new ChoiceItem(tr("Fluidsynth"), "fluidsynth")
           #endif
                << new ChoiceItem(tr("FMOD"), "fmod")
           #if !defined (DENG_DISABLE_SDLMIXER)
                << new ChoiceItem(tr("SDL_mixer"), "sdlmixer")
           #endif
           #if defined (WIN32)
                << new ChoiceItem(tr("Windows Multimedia"), "winmm")
           #endif
                << new ChoiceItem(tr("Disabled"), "dummy");

        cdPlugin->items()
                << new ChoiceItem(tr("FMOD"), "fmod")
           #if defined (WIN32)
                << new ChoiceItem(tr("Windows Multimedia"), "winmm")
           #endif
                << new ChoiceItem(tr("Disabled"), "dummy");

        soundPlugin->updateFromVariable();
        musicPlugin->updateFromVariable();
        cdPlugin   ->updateFromVariable();
    }

    void fetch()
    {
        if (!DoomsdayApp::isGameLoaded()) return;

        foreach (Widget *w, self.area().childWidgets() + devPopup->content().childWidgets())
        {
            if (ICVarWidget *cv = w->maybeAs<ICVarWidget>())
            {
                cv->updateFromCVar();
            }
        }
    }
};

AudioSettingsDialog::AudioSettingsDialog(String const &name)
    : DialogWidget(name, WithHeading), d(new Impl(this))
{
    bool const gameLoaded = DoomsdayApp::isGameLoaded();

    heading().setText(tr("Audio Settings"));
    heading().setImage(style().images().image("audio"));

    GridLayout layout(area().contentRule().left(), area().contentRule().top());
    layout.setGridSize(2, 0);
    layout.setColumnAlignment(0, ui::AlignRight);

    if (gameLoaded)
    {
        auto *sfxVolLabel   = LabelWidget::newWithText(tr("SFX Volume:"     ), &area());
        auto *musicVolLabel = LabelWidget::newWithText(tr("Music Volume:"   ), &area());
        auto *rvbVolLabel   = LabelWidget::newWithText(tr("Reverb Strength:"), &area());

        d->sound3D    ->setText(tr("3D Effects & Reverb"  ));
        d->overlapStop->setText(tr("One Sound per Emitter"));
        d->sound16bit ->setText(tr("16-bit Resampling"    ));

        auto *rateLabel = LabelWidget::newWithText(tr("Resampling:"), &area());

        d->sampleRate->items()
                << new ChoiceItem(tr("1x @ 11025 Hz"), 11025)
                << new ChoiceItem(tr("2x @ 22050 Hz"), 22050)
                << new ChoiceItem(tr("4x @ 44100 Hz"), 44100);

        auto *musSrcLabel = LabelWidget::newWithText(tr("Preferred Music:"), &area());

        d->musicSource->items()
                << new ChoiceItem(tr("MUS lumps"),      AudioSystem::MUSP_MUS)
                << new ChoiceItem(tr("External files"), AudioSystem::MUSP_EXT)
                << new ChoiceItem(tr("CD"),             AudioSystem::MUSP_CD);

        auto *sfLabel = LabelWidget::newWithText(tr("MIDI Sound Font:"), &area());

        // Layout.
        layout << *sfxVolLabel      << *d->sfxVolume
               << *musicVolLabel    << *d->musicVolume
               << *rvbVolLabel      << *d->reverbVolume
               << Const(0)          << *d->sound3D
               << Const(0)          << *d->overlapStop
               << *rateLabel        << *d->sampleRate
               << Const(0)          << *d->sound16bit
               << *musSrcLabel      << *d->musicSource
               << *sfLabel          << *d->musicSoundfont;
    }

    auto *soundPluginLabel = LabelWidget::newWithText(tr("SFX Plugin:"  ), &area());
    auto *musicPluginLabel = LabelWidget::newWithText(tr("Music Plugin:"), &area());
    auto *cdPluginLabel    = LabelWidget::newWithText(tr("CD Plugin:"   ), &area());

    LabelWidget *pluginLabel = LabelWidget::newWithText(_E(D) + tr("Audio Backend"), &area());
    pluginLabel->setFont("separator.label");
    pluginLabel->margins().setTop("gap");
    layout.setCellAlignment(Vector2i(0, layout.gridSize().y), ui::AlignLeft);
    layout.append(*pluginLabel, 2);

    layout << *soundPluginLabel << *d->soundPlugin
           << *musicPluginLabel << *d->musicPlugin
           << *cdPluginLabel    << *d->cdPlugin;

    area().setContentSize(layout.width(), layout.height());

    buttons()
            << new DialogButtonItem(DialogWidget::Default | DialogWidget::Accept, tr("Close"))
            << new DialogButtonItem(DialogWidget::Action, tr("Reset to Defaults"),
                                    new SignalAction(this, SLOT(resetToDefaults())));
    if (gameLoaded)
    {
        buttons() << new DialogButtonItem(DialogWidget::ActionPopup | Id1,
                                          style().images().image("gauge"));
        popupButtonWidget(Id1)->setPopup(*d->devPopup);
    }

    d->fetch();
}

void AudioSettingsDialog::resetToDefaults()
{
    ClientApp::audioSettings().resetToDefaults();

    d->fetch();
}
