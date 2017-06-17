/** @file rendererappearanceeditor.cpp
 *
 * @authors Copyright (c) 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "ui/editors/rendererappearanceeditor.h"
#include "ui/editors/variablegroupeditor.h"
#include "ui/dialogs/renderersettingsdialog.h"
#include "ui/widgets/profilepickerwidget.h"
#include "ui/widgets/cvarchoicewidget.h"
#include "ui/widgets/cvarsliderwidget.h"
#include "ui/widgets/cvartogglewidget.h"
#include "ui/clientwindow.h"
#include "render/rendersystem.h"
#include "clientapp.h"

#include <de/ButtonWidget>
#include <de/ScrollAreaWidget>
#include <de/FoldPanelWidget>
#include <de/DialogContentStylist>
#include <de/SequentialLayout>
#include <de/SignalAction>
#include <de/VariableSliderWidget>

using namespace de;
using namespace de::ui;

DENG_GUI_PIMPL(RendererAppearanceEditor),
DENG2_OBSERVES(ConfigProfiles, ProfileChange),
public VariableGroupEditor::IOwner
{
    using Group = VariableGroupEditor;

    ConfigProfiles &settings;
    ProfilePickerWidget *profile;

    Group *skyGroup;
    Group *shadowGroup;
    Group *lightGroup;
    Group *volLightGroup;
    Group *glowGroup;
    Group *lensGroup;
    Group *matGroup;
    Group *modelGroup;
    Group *spriteGroup;
    Group *objectGroup;
    Group *partGroup;

    Impl(Public *i)
        : Base(i)
        , settings(ClientApp::renderSystem().appearanceSettings())
    {
        settings.audienceForProfileChange += this;

        GuiWidget *container = &self().containerWidget();

        // The contents of the editor will scroll.
        container->add(profile = new ProfilePickerWidget(settings, tr("appearance")));
        profile->useInvertedStyleForPopups();

        // Sky settings.
        skyGroup = new Group(this, "sky", tr("Sky"));

        skyGroup->addLabel(tr("Sky Sphere Radius:"));
        skyGroup->addSlider("rend-sky-distance", Ranged(0, 8000), 10, 0);

        skyGroup->commit();

        // Shadow settings.
        shadowGroup = new Group(this, "shadow", tr("Shadows"));

        shadowGroup->addSpace();
        shadowGroup->addToggle("rend-fakeradio", tr("Ambient Occlusion"));

        shadowGroup->addLabel(tr("Occlusion Darkness:"));
        shadowGroup->addSlider("rend-fakeradio-darkness");

        shadowGroup->addSpace();
        shadowGroup->addToggle("rend-shadow", tr("Objects Cast Shadows"));

        shadowGroup->addLabel(tr("Shadow Darkness:"));
        shadowGroup->addSlider("rend-shadow-darkness");

        shadowGroup->addLabel(tr("Max Visible Distance:"));
        shadowGroup->addSlider("rend-shadow-far", Ranged(0, 3000), 10, 0);

        shadowGroup->addLabel(tr("Maximum Radius:"));
        shadowGroup->addSlider("rend-shadow-radius-max", Ranged(1, 128), 1, 0);

        shadowGroup->commit();

        // Dynamic light settings.
        lightGroup = new Group(this, "plight", tr("Point Lighting"));

        lightGroup->addSpace();
        lightGroup->addToggle("rend-light", tr("Dynamic Lights"));

        lightGroup->addSpace();
        lightGroup->addToggle("rend-light-decor", tr("Light Decorations"));

        lightGroup->addLabel(tr("Blending Mode:"));
        lightGroup->addChoice("rend-light-blend")->items()
                << new ChoiceItem(tr("Multiply"), 0)
                << new ChoiceItem(tr("Add"), 1)
                << new ChoiceItem(tr("Process without drawing"), 2);

        lightGroup->addLabel(tr("Number of Lights:"));
        lightGroup->addSlider("rend-light-num", Ranged(0, 2000), 1, 0)->setMinLabel(tr("Max"));

        lightGroup->addLabel(tr("Brightness:"));
        lightGroup->addSlider("rend-light-bright");

        lightGroup->addLabel(tr("Brightness in Fog:"));
        lightGroup->addSlider("rend-light-fog-bright");

        lightGroup->addLabel(tr("Light Radius Factor:"));
        lightGroup->addSlider("rend-light-radius-scale");

        lightGroup->addLabel(tr("Light Max Radius:"));
        lightGroup->addSlider("rend-light-radius-max");

        lightGroup->commit();

        // Volume lighting group.
        volLightGroup = new Group(this, "vlight", tr("Volume Lighting"));

        volLightGroup->addSpace();
        volLightGroup->addToggle("rend-light-sky-auto", tr("Apply Sky Color"));

        volLightGroup->addLabel(tr("Sky Color Factor:"));
        volLightGroup->addSlider("rend-light-sky");

        volLightGroup->addLabel(tr("Attenuation Distance:"));
        volLightGroup->addSlider("rend-light-attenuation", Ranged(0, 4000), 1, 0)->setMinLabel(tr("Off"));

        volLightGroup->addLabel(tr("Light Compression:"));
        volLightGroup->addSlider("rend-light-compression");

        volLightGroup->addLabel(tr("Ambient Light:"));
        volLightGroup->addSlider("rend-light-ambient");

        volLightGroup->addLabel(tr("Wall Angle Factor:"));
        volLightGroup->addSlider("rend-light-wall-angle", Ranged(0, 3), .01, 2);

        volLightGroup->addSpace();
        volLightGroup->addToggle("rend-light-wall-angle-smooth", tr("Smoothed Angle"));

        volLightGroup->commit();

        // Glow settings.
        glowGroup = new Group(this, "glow", tr("Surface Glow"));

        glowGroup->addLabel(tr("Material Glow:"));
        glowGroup->addSlider("rend-glow");

        glowGroup->addLabel(tr("Max Glow Height:"));
        glowGroup->addSlider("rend-glow-height");

        glowGroup->addLabel(tr("Glow Height Factor:"));
        glowGroup->addSlider("rend-glow-scale");

        glowGroup->addSpace();
        glowGroup->addToggle("rend-glow-wall", tr("Glow Visible on Walls"));

        glowGroup->commit();

        // Camera lens settings.
        lensGroup = new Group(this, "lens", tr("Camera Lens"));

        lensGroup->addLabel(tr("Pixel Doubling:"));
        lensGroup->addSlider(App::config("render.fx.resize.factor"), Ranged(1, 8), .1, 1);

        lensGroup->addSpace();
        lensGroup->addToggle("rend-bloom", tr("Bloom"));

        lensGroup->addLabel(tr("Bloom Threshold:"));
        lensGroup->addSlider("rend-bloom-threshold");

        lensGroup->addLabel(tr("Bloom Intensity:"));
        lensGroup->addSlider("rend-bloom-intensity");

        lensGroup->addLabel(tr("Bloom Dispersion:"));
        lensGroup->addSlider("rend-bloom-dispersion");

        lensGroup->addSpace();
        lensGroup->addToggle("rend-vignette", tr("Vignetting"));

        lensGroup->addLabel(tr("Vignette Darkness:"));
        lensGroup->addSlider("rend-vignette-darkness", Ranged(0, 2), .01, 2);

        lensGroup->addLabel(tr("Vignette Width:"));
        lensGroup->addSlider("rend-vignette-width");

        lensGroup->addSpace();
        lensGroup->addToggle("rend-halo-realistic", tr("Realistic Halos"));

        lensGroup->addLabel(tr("Flares per Halo:"));
        lensGroup->addSlider("rend-halo")->setMinLabel(tr("None"));

        lensGroup->addLabel(tr("Halo Brightness:"));
        lensGroup->addSlider("rend-halo-bright", Ranged(0, 100), 1, 0);

        lensGroup->addLabel(tr("Halo Size Factor:"));
        lensGroup->addSlider("rend-halo-size");

        lensGroup->addLabel(tr("Occlusion Fading:"));
        lensGroup->addSlider("rend-halo-occlusion", Ranged(1, 256), 1, 0);

        lensGroup->addLabel(tr("Min Halo Radius:"));
        lensGroup->addSlider("rend-halo-radius-min", Ranged(1, 80), .1, 1);

        lensGroup->addLabel(tr("Min Halo Size:"));
        lensGroup->addSlider("rend-halo-secondary-limit", Ranged(0, 10), .1, 1);

        lensGroup->addLabel(tr("Halo Fading Start:"));
        lensGroup->addSlider("rend-halo-dim-near", Ranged(0, 200), .1, 1);

        lensGroup->addLabel(tr("Halo Fading End:"));
        lensGroup->addSlider("rend-halo-dim-far", Ranged(0, 200), .1, 1);

        lensGroup->addLabel(tr("Z-Mag Divisor:"));
        lensGroup->addSlider("rend-halo-zmag-div", Ranged(1, 100), .1, 1);

        lensGroup->commit();

        // Material settings.
        matGroup = new Group(this, "material", tr("Materials"));

        matGroup->addSpace();
        matGroup->addToggle("rend-tex-shiny", tr("Shiny Surfaces"));

        matGroup->addSpace();
        matGroup->addToggle("rend-tex-anim-smooth", tr("Smooth Animation"));

        matGroup->addLabel(tr("Texture Quality:"));
        matGroup->addSlider("rend-tex-quality");

        matGroup->addLabel(tr("Texture Filtering:"));
        matGroup->addChoice("rend-tex-mipmap")->items()
                << new ChoiceItem(tr("None"),                       0)
                << new ChoiceItem(tr("Linear filter, no mip"),      1)
                << new ChoiceItem(tr("No filter, nearest mip"),     2)
                << new ChoiceItem(tr("Linear filter, nearest mip"), 3)
                << new ChoiceItem(tr("No filter, linear mip"),      4)
                << new ChoiceItem(tr("Linear filter, linear mip"),  5);

        matGroup->addSpace();
        matGroup->addToggle("rend-tex-filter-smart", tr("2x Smart Filtering"));

        matGroup->addLabel(tr("Bilinear Filtering:"));
        matGroup->addToggle("rend-tex-filter-sprite", tr("Sprites"));

        matGroup->addSpace();
        matGroup->addToggle("rend-tex-filter-mag", tr("World Surfaces"));

        matGroup->addSpace();
        matGroup->addToggle("rend-tex-filter-ui", tr("User Interface"));

        matGroup->addLabel(tr("Anisotropic Filter:"));
        matGroup->addChoice("rend-tex-filter-anisotropic")->items()
                << new ChoiceItem(tr("Best available"), -1)
                << new ChoiceItem(tr("Off"), 0)
                << new ChoiceItem(tr("2x"),  1)
                << new ChoiceItem(tr("4x"),  2)
                << new ChoiceItem(tr("8x"),  3)
                << new ChoiceItem(tr("16x"), 4);

        matGroup->addSpace();
        matGroup->addToggle("rend-tex-detail", tr("Detail Textures"));

        matGroup->addLabel(tr("Scaling Factor:"));
        matGroup->addSlider("rend-tex-detail-scale", Ranged(0, 16), .01, 2);

        matGroup->addLabel(tr("Contrast:"));
        matGroup->addSlider("rend-tex-detail-strength");

        matGroup->commit();

        // Model settings.
        modelGroup = new Group(this, "model", tr("3D Models"));

        modelGroup->addSpace();
        modelGroup->addToggle("rend-model", tr("3D Models"));

        modelGroup->addSpace();
        modelGroup->addToggle("rend-model-inter", tr("Interpolate Frames"));

        modelGroup->addSpace();
        modelGroup->addToggle("rend-model-mirror-hud", tr("Mirror Player Weapon"));

        modelGroup->addLabel(tr("Max Visible Distance:"));
        modelGroup->addSlider("rend-model-distance", Ranged(0, 3000), 10, 0)->setMinLabel(tr("Inf"));

        modelGroup->addLabel(tr("LOD #0 Distance:"));
        modelGroup->addSlider("rend-model-lod", Ranged(0, 1000), 1, 0)->setMinLabel(tr("No LOD"));

        modelGroup->addLabel(tr("Number of Lights:"));
        modelGroup->addSlider("rend-model-lights");

        modelGroup->commit();

        // Sprite settings.
        spriteGroup = new Group(this, "sprite", tr("Sprites"));

        spriteGroup->addSpace();
        spriteGroup->addToggle("rend-sprite-blend", tr("Additive Blending"));

        spriteGroup->addLabel(tr("Number of Lights:"));
        spriteGroup->addSlider("rend-sprite-lights")->setMinLabel(tr("Inf"));

        spriteGroup->addLabel(tr("Sprite Alignment:"));
        spriteGroup->addChoice("rend-sprite-align")->items()
                << new ChoiceItem(tr("Camera"), 0)
                << new ChoiceItem(tr("View plane"), 1)
                << new ChoiceItem(tr("Camera (limited)"), 2)
                << new ChoiceItem(tr("View plane (limited)"), 3);

        spriteGroup->addSpace();
        spriteGroup->addToggle("rend-sprite-mode", tr("Sharp Edges"));

        spriteGroup->addSpace();
        spriteGroup->addToggle("rend-sprite-noz", tr("Disable Z-Write"));

        spriteGroup->commit();

        // Object settings.
        objectGroup = new Group(this, "object", tr("Objects"));

        objectGroup->addLabel(tr("Smooth Movement:"));
        objectGroup->addChoice("rend-mobj-smooth-move")->items()
                << new ChoiceItem(tr("Disabled"), 0)
                << new ChoiceItem(tr("Models only"), 1)
                << new ChoiceItem(tr("Models and sprites"), 2);

        objectGroup->addSpace();
        objectGroup->addToggle("rend-mobj-smooth-turn", tr("Smooth Turning"));

        objectGroup->commit();

        // Particle settings.
        partGroup = new Group(this, "ptcfx", tr("Particle Effects"));

        partGroup->addSpace();
        partGroup->addToggle("rend-particle", tr("Particle Effects"));

        partGroup->addLabel(tr("Max Particles:"));
        partGroup->addSlider("rend-particle-max", Ranged(0, 10000), 100, 0)->setMinLabel(tr("Inf"));

        partGroup->addLabel(tr("Spawn Rate:"));
        partGroup->addSlider("rend-particle-rate");

        partGroup->addLabel(tr("Diffusion:"));
        partGroup->addSlider("rend-particle-diffuse", Ranged(0, 20), .01, 2);

        partGroup->addLabel(tr("Near Clip Distance:"));
        partGroup->addSlider("rend-particle-visible-near", Ranged(0, 1000), 1, 0)->setMinLabel(tr("None"));

        partGroup->commit();
    }

//    ~Impl()
//    {
//        settings.audienceForProfileChange -= this;
//    }

    Rule const &firstColumnWidthRule() const
    {
        return self().firstColumnWidth();
    }

    ScrollAreaWidget &containerWidget()
    {
        return self().containerWidget();
    }

    void resetToDefaults(String const &settingName)
    {
        settings.resetSettingToDefaults(settingName);
    }

    void currentProfileChanged(String const &)
    {
        // Update with values from the new profile.
        fetch();
    }

    void fetch()
    {
        bool const isReadOnly = settings.find(settings.currentProfile()).isReadOnly();

        foreach (GuiWidget *child, self().containerWidget().childWidgets())
        {
            if (Group *g = maybeAs<Group>(child))
            {
                g->setResetable(!isReadOnly);
                g->fetch();
                g->resetButton().enable(!isReadOnly && g->isOpen());

                // Enable or disable settings based on read-onlyness.
                foreach (GuiWidget *st, g->content().childWidgets())
                {
                    st->enable(!isReadOnly);
                }
            }
        }
    }

    void saveFoldState(PersistentState &toState)
    {
        foreach (GuiWidget *child, self().containerWidget().childWidgets())
        {
            if (Group *g = maybeAs<Group>(child))
            {
                toState.objectNamespace().set(self().name() + "." + g->name() + ".open",
                                              g->isOpen());
            }
        }
    }

    void restoreFoldState(PersistentState const &fromState)
    {
        bool gotState = false;

        foreach (GuiWidget *child, self().containerWidget().childWidgets())
        {
            if (Group *g = maybeAs<Group>(child))
            {
                String const var = self().name() + "." + g->name() + ".open";
                if (fromState.objectNamespace().has(var))
                {
                    gotState = true;
                    if (fromState.objectNamespace().getb(var))
                        g->open();
                    else
                        g->close(0);
                }
            }
        }

        if (!gotState)
        {
            // Could be the first time.
            lightGroup->open();
        }
    }
};

RendererAppearanceEditor::RendererAppearanceEditor()
    : SidebarWidget(tr("Renderer Appearance"), "rendererappearanceeditor")
    , d(new Impl(this))
{
    d->profile->setOpeningDirection(Down);

    LabelWidget *profLabel = LabelWidget::newWithText(tr("Profile:"), &containerWidget());

    // Layout.
    layout().append(*profLabel, SequentialLayout::IgnoreMinorAxis);
    d->profile->rule()
            .setInput(Rule::Left, profLabel->rule().right())
            .setInput(Rule::Top,  profLabel->rule().top());

    layout() << d->lightGroup->title()    << *d->lightGroup
             << d->volLightGroup->title() << *d->volLightGroup
             << d->glowGroup->title()     << *d->glowGroup
             << d->shadowGroup->title()   << *d->shadowGroup
             << d->lensGroup->title()     << *d->lensGroup
             << d->matGroup->title()      << *d->matGroup
             << d->objectGroup->title()   << *d->objectGroup
             << d->modelGroup->title()    << *d->modelGroup
             << d->spriteGroup->title()   << *d->spriteGroup
             << d->partGroup->title()     << *d->partGroup
             << d->skyGroup->title()      << *d->skyGroup;

    updateSidebarLayout(profLabel->rule().width() +
                        d->profile->rule().width() +
                        d->profile->button().rule().width(), Const(0));

    d->fetch();
}

void RendererAppearanceEditor::operator >> (PersistentState &toState) const
{
    d->saveFoldState(toState);
}

void RendererAppearanceEditor::operator << (PersistentState const &fromState)
{
    d->restoreFoldState(fromState);
}
