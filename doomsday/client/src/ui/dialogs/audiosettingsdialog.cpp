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
#include "ui/widgets/choicewidget.h"

#include "de_audio.h"
#include "con_main.h"
#include "SignalAction"

using namespace de;
using namespace ui;

DENG_GUI_PIMPL(AudioSettingsDialog)
{
    CVarSliderWidget *sfxVolume;
    CVarSliderWidget *musicVolume;
    CVarSliderWidget *reverbVolume;
    CVarToggleWidget *sound3D;
    CVarToggleWidget *sound16bit;
    ChoiceWidget *sampleRate;
    ChoiceWidget *musicSource;
    CVarToggleWidget *soundInfo;

    Instance(Public *i) : Base(i)
    {
        ScrollAreaWidget &area = self.area();

        area.add(sfxVolume    = new CVarSliderWidget("sound-volume"));
        area.add(musicVolume  = new CVarSliderWidget("music-volume"));
        area.add(reverbVolume = new CVarSliderWidget("sound-reverb-volume"));
        area.add(sound3D      = new CVarToggleWidget("sound-3d"));
        area.add(sound16bit   = new CVarToggleWidget("sound-16bit"));
        area.add(sampleRate   = new ChoiceWidget);
        area.add(musicSource  = new ChoiceWidget);
        area.add(soundInfo    = new CVarToggleWidget("sound-info"));
    }

    void fetch()
    {
        sfxVolume->updateFromCVar();
        musicVolume->updateFromCVar();
        reverbVolume->updateFromCVar();
        sound3D->updateFromCVar();
        sound16bit->updateFromCVar();
        soundInfo->updateFromCVar();

        sampleRate->setSelected(sampleRate->items().findData(Con_GetInteger("sound-rate")));
        musicSource->setSelected(musicSource->items().findData(Con_GetInteger("music-source")));
    }
};

AudioSettingsDialog::AudioSettingsDialog(String const &name)
    : DialogWidget(name, WithHeading), d(new Instance(this))
{
    heading().setText(tr("Audio Settings"));

    LabelWidget *sfxVolLabel = new LabelWidget;
    sfxVolLabel->setText(tr("SFX Volume:"));
    area().add(sfxVolLabel);

    LabelWidget *musicVolLabel = new LabelWidget;
    musicVolLabel->setText(tr("Music Volume:"));
    area().add(musicVolLabel);

    LabelWidget *rvbVolLabel = new LabelWidget;
    rvbVolLabel->setText(tr("Reverb Volume:"));
    area().add(rvbVolLabel);

    d->sound3D->setText(tr("3D Effects & Reverb"));
    d->sound16bit->setText(tr("16-bit Resampling"));

    LabelWidget *rateLabel = new LabelWidget;
    rateLabel->setText(tr("Resampling:"));
    area().add(rateLabel);

    d->sampleRate->items()
            << new ChoiceItem(tr("1x @ 11025 Hz"), 11025)
            << new ChoiceItem(tr("2x @ 22050 Hz"), 22050)
            << new ChoiceItem(tr("4x @ 44100 Hz"), 44100);

    connect(d->sampleRate, SIGNAL(selectionChangedByUser(uint)),
            this, SLOT(sampleRateChanged(uint)));

    LabelWidget *musSrcLabel = new LabelWidget;
    musSrcLabel->setText(tr("Preferred Music:"));
    area().add(musSrcLabel);

    d->musicSource->items()
            << new ChoiceItem(tr("MUS lumps"), MUSP_MUS)
            << new ChoiceItem(tr("External files"), MUSP_EXT)
            << new ChoiceItem(tr("CD"), MUSP_CD);

    connect(d->musicSource, SIGNAL(selectionChangedByUser(uint)),
            this, SLOT(musicSourceChanged(uint)));

    d->soundInfo->setText(tr("Developer Info"));

    // Layout.
    GridLayout layout(area().contentRule().left(), area().contentRule().top());
    layout.setGridSize(2, 0);
    layout.setColumnAlignment(0, ui::AlignRight);
    layout << *sfxVolLabel   << *d->sfxVolume
           << *musicVolLabel << *d->musicVolume
           << *rvbVolLabel   << *d->reverbVolume
           << Const(0)       << *d->sound3D
           << *musSrcLabel   << *d->musicSource
           << *rateLabel     << *d->sampleRate
           << Const(0)       << *d->sound16bit
           << Const(0)       << *d->soundInfo;

    area().setContentSize(layout.width(), layout.height());

    buttons().items()
            << new DialogButtonItem(DialogWidget::Default | DialogWidget::Accept, tr("Close"))
            << new DialogButtonItem(DialogWidget::Action, tr("Reset to Defaults"),
                                    new SignalAction(this, SLOT(resetToDefaults())));

    d->fetch();
}

void AudioSettingsDialog::sampleRateChanged(uint)
{
    Con_SetInteger("sound-rate", d->sampleRate->selectedItem().data().toInt());
}

void AudioSettingsDialog::musicSourceChanged(uint)
{
    Con_SetInteger("music-source", d->musicSource->selectedItem().data().toInt());
}

void AudioSettingsDialog::resetToDefaults()
{
    Con_SetInteger("sound-volume",        255  );
    Con_SetInteger("music-volume",        255  );
    Con_SetFloat  ("sound-reverb-volume", 0.5f );
    Con_SetInteger("sound-info",          0    );
    Con_SetInteger("sound-rate",          11025);
    Con_SetInteger("sound-16bit",         0    );
    Con_SetInteger("sound-3d",            0    );
    Con_SetInteger("music-source",        MUSP_EXT);

    d->fetch();
}
