/** @file audiosettingsdialog.cpp Dialog for audio settings.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "clientapp.h"
#include "de_audio.h"

#include <de/SignalAction>
#include <de/GridPopupWidget>

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
    CVarToggleWidget *soundInfo;
    GridPopupWidget *devPopup;

    Instance(Public *i) : Base(i)
    {
        ScrollAreaWidget &area = self.area();

        area.add(sfxVolume    = new CVarSliderWidget("sound-volume"));
        area.add(musicVolume  = new CVarSliderWidget("music-volume"));
        area.add(reverbVolume = new CVarSliderWidget("sound-reverb-volume"));
        area.add(sound3D      = new CVarToggleWidget("sound-3d"));
        area.add(overlapStop  = new CVarToggleWidget("sound-overlap-stop"));
        area.add(sound16bit   = new CVarToggleWidget("sound-16bit"));
        area.add(sampleRate   = new CVarChoiceWidget("sound-rate"));
        area.add(musicSource  = new CVarChoiceWidget("music-source"));

        // Developer options.
        self.add(devPopup = new GridPopupWidget);
        soundInfo = new CVarToggleWidget("sound-info", tr("Sound Channel Status"));
        *devPopup << soundInfo;
        devPopup->commit();
    }

    void fetch()
    {
        sfxVolume->updateFromCVar();
        musicVolume->updateFromCVar();
        reverbVolume->updateFromCVar();
        sound3D->updateFromCVar();
        overlapStop->updateFromCVar();
        sound16bit->updateFromCVar();
        soundInfo->updateFromCVar();
        sampleRate->updateFromCVar();
        musicSource->updateFromCVar();
    }
};

AudioSettingsDialog::AudioSettingsDialog(String const &name)
    : DialogWidget(name, WithHeading), d(new Instance(this))
{
    heading().setText(tr("Audio Settings"));

    LabelWidget *sfxVolLabel   = LabelWidget::newWithText(tr("SFX Volume:"), &area());
    LabelWidget *musicVolLabel = LabelWidget::newWithText(tr("Music Volume:"), &area());
    LabelWidget *rvbVolLabel   = LabelWidget::newWithText(tr("Reverb Volume:"), &area());

    d->sound3D->setText(tr("3D Effects & Reverb"));
    d->overlapStop->setText(tr("One Sound per Emitter"));
    d->sound16bit->setText(tr("16-bit Resampling"));

    LabelWidget *rateLabel = LabelWidget::newWithText(tr("Resampling:"), &area());

    d->sampleRate->items()
            << new ChoiceItem(tr("1x @ 11025 Hz"), 11025)
            << new ChoiceItem(tr("2x @ 22050 Hz"), 22050)
            << new ChoiceItem(tr("4x @ 44100 Hz"), 44100);

    LabelWidget *musSrcLabel = LabelWidget::newWithText(tr("Preferred Music:"), &area());

    d->musicSource->items()
            << new ChoiceItem(tr("MUS lumps"), MUSP_MUS)
            << new ChoiceItem(tr("External files"), MUSP_EXT)
            << new ChoiceItem(tr("CD"), MUSP_CD);


    // Layout.
    GridLayout layout(area().contentRule().left(), area().contentRule().top());
    layout.setGridSize(2, 0);
    layout.setColumnAlignment(0, ui::AlignRight);
    layout << *sfxVolLabel   << *d->sfxVolume
           << *musicVolLabel << *d->musicVolume
           << *rvbVolLabel   << *d->reverbVolume
           << Const(0)       << *d->sound3D
           << Const(0)       << *d->overlapStop
           << *musSrcLabel   << *d->musicSource
           << *rateLabel     << *d->sampleRate
           << Const(0)       << *d->sound16bit;

    area().setContentSize(layout.width(), layout.height());

    buttons()
            << new DialogButtonItem(DialogWidget::Default | DialogWidget::Accept, tr("Close"))
            << new DialogButtonItem(DialogWidget::Action, tr("Reset to Defaults"),
                                    new SignalAction(this, SLOT(resetToDefaults())))
            << new DialogButtonItem(DialogWidget::Action | Id1,
                                    style().images().image("gauge"),
                                    new SignalAction(this, SLOT(showDeveloperPopup())));

    d->devPopup->setAnchorAndOpeningDirection(buttonWidget(Id1)->rule(), ui::Up);

    d->fetch();
}

void AudioSettingsDialog::resetToDefaults()
{
    ClientApp::audioSettings().resetToDefaults();

    d->fetch();
}

void AudioSettingsDialog::showDeveloperPopup()
{
    d->devPopup->open();
}
