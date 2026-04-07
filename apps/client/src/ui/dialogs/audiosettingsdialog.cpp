/** @file audiosettingsdialog.cpp  Dialog for audio settings.
 *
 * @authors Copyright © 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "audio/audiosystem.h"

#include "clientapp.h"
#include "configprofiles.h"

#include <de/foldpanelwidget.h>
#include <de/gridpopupwidget.h>
#include <de/dscript.h>
#include <de/sequentiallayout.h>
#include <de/variablechoicewidget.h>
#include <de/variablesliderwidget.h>
#include <de/variabletogglewidget.h>

using namespace de;
using namespace de::ui;

DE_GUI_PIMPL(AudioSettingsDialog)
{
    CVarSliderWidget *sfxVolume;
    CVarSliderWidget *musicVolume;
    CVarSliderWidget *reverbVolume;
    CVarToggleWidget *sound3D;
    CVarToggleWidget *overlapStop;
    //CVarToggleWidget *sound16bit;
    //CVarChoiceWidget *sampleRate;
    CVarChoiceWidget *musicSource;
    CVarNativePathWidget *musicSoundfont;
    VariableToggleWidget *pauseOnFocus;
    CVarToggleWidget     *soundInfo;
    GridPopupWidget      *devPopup;
    FoldPanelWidget      *backendFold;
    GuiWidget            *backendBase;
    VariableSliderWidget *sfxChannels;
    VariableChoiceWidget *audioOutput;
//    VariableChoiceWidget *fmodSpeakerMode;
    VariableChoiceWidget *soundPlugin;
    VariableChoiceWidget *musicPlugin;
#if defined (DE_WINDOWS)
    VariableChoiceWidget *cdPlugin;
#endif
    bool needAudioReinit = false;

    Impl(Public *i) : Base(i)
    {
        ScrollAreaWidget &area = self().area();
        area.enableIndicatorDraw(true);

        if (DoomsdayApp::isGameLoaded())
        {
            area.add(sfxVolume      = new CVarSliderWidget    ("sound-volume"));
            area.add(musicVolume    = new CVarSliderWidget    ("music-volume"));
            area.add(reverbVolume   = new CVarSliderWidget    ("sound-reverb-volume"));
            area.add(sound3D        = new CVarToggleWidget    ("sound-3d"));
            area.add(overlapStop    = new CVarToggleWidget    ("sound-overlap-stop"));
            //area.add(sound16bit     = new CVarToggleWidget    ("sound-16bit"));
            //area.add(sampleRate     = new CVarChoiceWidget    ("sound-rate"));
            area.add(musicSource    = new CVarChoiceWidget    ("music-source"));
            area.add(musicSoundfont = new CVarNativePathWidget("music-soundfont"));

            musicSoundfont->setBlankText("GeneralUser GS");
            musicSoundfont->setFilters({{"SF2 soundfonts", {"sf2"}},
                                        {"DLS soundfonts", {"dls"}},
                                        {"All files", {}}});

            area.add(pauseOnFocus = new VariableToggleWidget("Pause on Focus Lost",
                                                             App::config("audio.pauseOnFocus"),
                                                             "pause-on-focus"));

            // Display volumes on a 0...100 scale.
            sfxVolume  ->setDisplayFactor(100.0 / 255.0);
            musicVolume->setDisplayFactor(100.0 / 255.0);
            sfxVolume  ->setStep(1.0 / sfxVolume->displayFactor());
            musicVolume->setStep(1.0 / musicVolume->displayFactor());

            // Developer options.
            self().add(devPopup = new GridPopupWidget);
            soundInfo = new CVarToggleWidget("sound-info", "Sound Channel Status");
            *devPopup << soundInfo;
            devPopup->commit();
        }

        backendFold = FoldPanelWidget::makeOptionsGroup("audio-backend", "Audio Backend", &area);
        backendBase = new GuiWidget("fold-base");
        backendFold->setContent(backendBase);

        backendBase->add(audioOutput  = new VariableChoiceWidget(App::config("audio.output"), VariableChoiceWidget::Number));
        backendBase->add(sfxChannels  = new VariableSliderWidget(App::config("audio.channels"), Ranged(1, 64), 1.0));
        backendBase->add(soundPlugin  = new VariableChoiceWidget(App::config("audio.soundPlugin"), VariableChoiceWidget::Text));
        backendBase->add(musicPlugin  = new VariableChoiceWidget(App::config("audio.musicPlugin"), VariableChoiceWidget::Text));
#if defined (DE_WINDOWS)
        backendBase->add(cdPlugin = new VariableChoiceWidget(App::config("audio.cdPlugin"), VariableChoiceWidget::Text));
#endif
//        backendBase->add(fmodSpeakerMode = new VariableChoiceWidget(App::config("audio.fmod.speakerMode"), VariableChoiceWidget::Text));

        // Backend layout.
        {
            GridLayout layout(backendBase->rule().left(), backendBase->rule().top());
            layout.setGridSize(2, 0);
            layout.setColumnAlignment(0, ui::AlignRight);

            layout << *LabelWidget::newWithText("SFX Plugin:", backendBase) << *soundPlugin
                   << *LabelWidget::newWithText("Music Plugin:", backendBase) << *musicPlugin
#if defined (DE_WINDOWS)
                   << *LabelWidget::newWithText("CD Plugin:", backendBase) << *cdPlugin
#endif
                   << *LabelWidget::newWithText("Output:", backendBase) << *audioOutput
//                   << *LabelWidget::newWithText("Speaker Mode:", backendBase) << *fmodSpeakerMode
                   << *LabelWidget::newWithText("SFX Channels:", backendBase) << *sfxChannels;

            backendBase->rule().setSize(layout);
        }

        // Check currently available outputs.
        enumerateAudioOutputs();

        soundPlugin->items()
                << new ChoiceItem("FMOD", "fmod")
           #if !defined (DE_DISABLE_SDLMIXER)
                << new ChoiceItem("SDL_mixer", "sdlmixer")
           #endif
                << new ChoiceItem("OpenAL", "openal")
//           #if defined (WIN32)
//                << new ChoiceItem(tr("DirectSound"), "dsound")
//           #endif
                << new ChoiceItem("Disabled", "dummy");

        musicPlugin->items()
                << new ChoiceItem("FluidSynth", "fluidsynth")
                << new ChoiceItem("FMOD", "fmod")
           #if !defined (DE_DISABLE_SDLMIXER)
                << new ChoiceItem("SDL_mixer", "sdlmixer")
           #endif
//           #if defined (WIN32)
//                << new ChoiceItem("Windows Multimedia", "winmm")
//           #endif
                << new ChoiceItem("Disabled", "dummy");

#if defined (DE_WINDOWS)
        cdPlugin->items()
            //              << new ChoiceItem(tr("Windows Multimedia"), "winmm")
                << new ChoiceItem("Disabled", "dummy");

        cdPlugin->updateFromVariable();
#endif

        soundPlugin->updateFromVariable();
        musicPlugin->updateFromVariable();
        audioOutput->updateFromVariable();

        // The audio system needs reinitializing if the plugins are changed.
        auto changeFunc = [this]() {
            needAudioReinit = true;
            self().buttonWidget(Id2)->setText(_E(b) "Apply");
        };
        soundPlugin->audienceForUserSelection() += changeFunc;
        musicPlugin->audienceForUserSelection() += changeFunc;
        audioOutput->audienceForUserSelection() += changeFunc;
#if defined (DE_WINDOWS)
        cdPlugin->audienceForUserSelection() += changeFunc;
#endif
        sfxChannels->audienceForUserValue() += changeFunc;
    }

    void enumerateAudioOutputs()
    {
        audioOutput->items().clear();

        const auto &outputs =
            ScriptSystem::get()["Audio"]["outputs"].value<DictionaryValue>();

        /// @todo Currently only FMOD has outputs.
        const TextValue key("fmod");
        if (outputs.contains(key))
        {
            const auto &names = outputs.element(key).as<ArrayValue>();
            for (dsize i = 0; i < names.size(); ++i)
            {
                audioOutput->items() << new ChoiceItem(names.at(i).asText(), NumberValue(i));
            }
        }
    }

    void fetch()
    {
        if (!DoomsdayApp::isGameLoaded()) return;

        for (GuiWidget *w : self().area().childWidgets() + devPopup->content().childWidgets())
        {
            if (ICVarWidget *cv = maybeAs<ICVarWidget>(w))
            {
                cv->updateFromCVar();
            }
        }
    }
};

AudioSettingsDialog::AudioSettingsDialog(const String &name)
    : DialogWidget(name, WithHeading)
    , d(new Impl(this))
{
    const bool gameLoaded = DoomsdayApp::isGameLoaded();

    heading().setText("Audio Settings");
    heading().setImage(style().images().image("audio"));

    GridLayout layout(area().contentRule().left(), area().contentRule().top());
    layout.setGridSize(2, 0);
    layout.setColumnAlignment(0, ui::AlignRight);

    if (gameLoaded)
    {
        auto *sfxVolLabel   = LabelWidget::newWithText("SFX Volume:"     , &area());
        auto *musicVolLabel = LabelWidget::newWithText("Music Volume:"   , &area());
        auto *rvbVolLabel   = LabelWidget::newWithText("Reverb Strength:", &area());

        d->sound3D    ->setText("3D Effects & Reverb"  );
        d->overlapStop->setText("One Sound per Emitter");
        //d->sound16bit ->setText(tr("16-bit Resampling"    ));

        /*auto *rateLabel = LabelWidget::newWithText(tr("Resampling:"), &area());

        d->sampleRate->items()
                << new ChoiceItem(tr("1x @ 11025 Hz"), 11025)
                << new ChoiceItem(tr("2x @ 22050 Hz"), 22050)
                << new ChoiceItem(tr("4x @ 44100 Hz"), 44100);*/

        auto *musSrcLabel = LabelWidget::newWithText("Preferred Music:", &area());

        d->musicSource->items()
                << new ChoiceItem("MUS lumps",      NumberValue(AudioSystem::MUSP_MUS))
                << new ChoiceItem("External files", NumberValue(AudioSystem::MUSP_EXT))
                << new ChoiceItem("CD",             NumberValue(AudioSystem::MUSP_CD));

        auto *sfLabel = LabelWidget::newWithText("MIDI Sound Font:", &area());

        // Layout.
        LabelWidget::appendSeparatorWithText("Sound Effects", &area(), &layout);
        layout << *sfxVolLabel      << *d->sfxVolume                  
               << Const(0)          << *d->overlapStop
               << Const(0)          << *d->sound3D
               << *rvbVolLabel      << *d->reverbVolume;

        LabelWidget::appendSeparatorWithText("Music", &area(), &layout);
        layout << *musicVolLabel    << *d->musicVolume
               << *musSrcLabel      << *d->musicSource
               << *sfLabel          << *d->musicSoundfont
               << Const(0)          << *d->pauseOnFocus;
    }
    else
    {
        d->backendFold->open();
    }
    
    SequentialLayout layout2(area().contentRule().left(),
                             area().contentRule().top() + layout.height());
    layout2.setOverrideWidth(d->backendBase->rule().width());
    
    layout2 << d->backendFold->title()
            << *d->backendFold;
    
    area().setContentSize(OperatorRule::maximum(layout.width(), layout2.width()),
                          layout.height() + layout2.height());

    // The subheading should extend all the way.
    d->backendFold->title().rule().setInput(Rule::Width, area().contentRule().width());

    buttons()
            << new DialogButtonItem(Default | Accept | Id2, "Close")
            << new DialogButtonItem(Action, "Reset to Defaults",
                                    [this]() { resetToDefaults(); });
    if (gameLoaded)
    {
        buttons() << new DialogButtonItem(ActionPopup | Id1,
                                          style().images().image("gauge"));
        popupButtonWidget(Id1)->setPopup(*d->devPopup);
    }

    d->fetch();
}

void AudioSettingsDialog::resetToDefaults()
{
    ClientApp::audioSettings().resetToDefaults();
    d->fetch();
    d->needAudioReinit = true;
}

void AudioSettingsDialog::finish(int result)
{
    DialogWidget::finish(result);
    if (result)
    {
        if (d->needAudioReinit)
        {
            AudioSystem::get().reinitialize();
        }
    }
}
