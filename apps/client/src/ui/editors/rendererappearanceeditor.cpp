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

#include <de/buttonwidget.h>
#include <de/scrollareawidget.h>
#include <de/dialogcontentstylist.h>
#include <de/sequentiallayout.h>
#include <de/variablesliderwidget.h>

using namespace de;
using namespace de::ui;

DE_GUI_PIMPL(RendererAppearanceEditor)
, DE_OBSERVES(ConfigProfiles, ProfileChange)
, public VariableGroupEditor::IOwner
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
        , settings(ClientApp::render().appearanceSettings())
    {
        settings.audienceForProfileChange += this;

        GuiWidget *container = &self().containerWidget();

        // The contents of the editor will scroll.
        container->add(profile = new ProfilePickerWidget(settings, "appearance"));
        profile->useInvertedStyleForPopups();

        // Sky settings.
        skyGroup = new Group(this, "sky", "Sky");

        skyGroup->addLabel("Sky Sphere Radius:");
        skyGroup->addSlider("rend-sky-distance", Ranged(0, 8000), 10, 0);

        skyGroup->commit();

        // Shadow settings.
        shadowGroup = new Group(this, "shadow", "Shadows");

        shadowGroup->addSpace();
        shadowGroup->addToggle("rend-fakeradio", "Ambient Occlusion");

        shadowGroup->addLabel("Occlusion Darkness:");
        shadowGroup->addSlider("rend-fakeradio-darkness");

        shadowGroup->addSpace();
        shadowGroup->addToggle("rend-shadow", "Objects Cast Shadows");

        shadowGroup->addLabel("Shadow Darkness:");
        shadowGroup->addSlider("rend-shadow-darkness");

        shadowGroup->addLabel("Max Visible Distance:");
        shadowGroup->addSlider("rend-shadow-far", Ranged(0, 3000), 10, 0);

        shadowGroup->addLabel("Maximum Radius:");
        shadowGroup->addSlider("rend-shadow-radius-max", Ranged(1, 128), 1, 0);

        shadowGroup->commit();

        // Dynamic light settings.
        lightGroup = new Group(this, "plight", "Point Lighting");

        lightGroup->addSpace();
        lightGroup->addToggle("rend-light", "Dynamic Lights");

        lightGroup->addSpace();
        lightGroup->addToggle("rend-light-decor", "Light Decorations");

        lightGroup->addLabel("Blending Mode:");
        lightGroup->addChoice("rend-light-blend")->items()
                << new ChoiceItem("Multiply", 0)
                << new ChoiceItem("Add", 1)
                << new ChoiceItem("Process without drawing", 2);

        lightGroup->addLabel("Number of Lights:");
        lightGroup->addSlider("rend-light-num", Ranged(0, 2000), 1, 0)->setMinLabel("Max");

        lightGroup->addLabel("Brightness:");
        lightGroup->addSlider("rend-light-bright");

        lightGroup->addLabel("Brightness in Fog:");
        lightGroup->addSlider("rend-light-fog-bright");

        lightGroup->addLabel("Light Radius Factor:");
        lightGroup->addSlider("rend-light-radius-scale");

        lightGroup->addLabel("Light Max Radius:");
        lightGroup->addSlider("rend-light-radius-max");

        lightGroup->commit();

        // Volume lighting group.
        volLightGroup = new Group(this, "vlight", "Volume Lighting");

        volLightGroup->addSpace();
        volLightGroup->addToggle("rend-light-sky-auto", "Apply Sky Color");

        volLightGroup->addLabel("Sky Color Factor:");
        volLightGroup->addSlider("rend-light-sky");

        volLightGroup->addLabel("Attenuation Distance:");
        volLightGroup->addSlider("rend-light-attenuation", Ranged(0, 4000), 1, 0)->setMinLabel("Off");

        volLightGroup->addLabel("Light Compression:");
        volLightGroup->addSlider("rend-light-compression");

        volLightGroup->addLabel("Ambient Light:");
        volLightGroup->addSlider("rend-light-ambient");

        volLightGroup->addLabel("Wall Angle Factor:");
        volLightGroup->addSlider("rend-light-wall-angle", Ranged(0, 3), .01, 2);

        volLightGroup->addSpace();
        volLightGroup->addToggle("rend-light-wall-angle-smooth", "Smoothed Angle");

        volLightGroup->commit();

        // Glow settings.
        glowGroup = new Group(this, "glow", "Surface Glow");

        glowGroup->addLabel("Material Glow:");
        glowGroup->addSlider("rend-glow");

        glowGroup->addLabel("Max Glow Height:");
        glowGroup->addSlider("rend-glow-height");

        glowGroup->addLabel("Glow Height Factor:");
        glowGroup->addSlider("rend-glow-scale");

        glowGroup->addSpace();
        glowGroup->addToggle("rend-glow-wall", "Glow Visible on Walls");

        glowGroup->commit();

        // Camera lens settings.
        lensGroup = new Group(this, "lens", "Camera Lens");

        lensGroup->addLabel("Pixel Doubling:");
        lensGroup->addSlider(App::config("render.fx.resize.factor"), Ranged(1, 8), .1, 1);

        lensGroup->addSpace();
        lensGroup->addToggle("rend-bloom", "Bloom");

        lensGroup->addLabel("Bloom Threshold:");
        lensGroup->addSlider("rend-bloom-threshold");

        lensGroup->addLabel("Bloom Intensity:");
        lensGroup->addSlider("rend-bloom-intensity");

        lensGroup->addLabel("Bloom Dispersion:");
        lensGroup->addSlider("rend-bloom-dispersion");

        lensGroup->addSpace();
        lensGroup->addToggle("rend-vignette", "Vignetting");

        lensGroup->addLabel("Vignette Darkness:");
        lensGroup->addSlider("rend-vignette-darkness", Ranged(0, 2), .01, 2);

        lensGroup->addLabel("Vignette Width:");
        lensGroup->addSlider("rend-vignette-width");

        lensGroup->addSpace();
        lensGroup->addToggle("rend-halo-realistic", "Realistic Halos");

        lensGroup->addLabel("Flares per Halo:");
        lensGroup->addSlider("rend-halo")->setMinLabel("None");

        lensGroup->addLabel("Halo Brightness:");
        lensGroup->addSlider("rend-halo-bright", Ranged(0, 100), 1, 0);

        lensGroup->addLabel("Halo Size Factor:");
        lensGroup->addSlider("rend-halo-size");

        lensGroup->addLabel("Occlusion Fading:");
        lensGroup->addSlider("rend-halo-occlusion", Ranged(1, 256), 1, 0);

        lensGroup->addLabel("Min Halo Radius:");
        lensGroup->addSlider("rend-halo-radius-min", Ranged(1, 80), .1, 1);

        lensGroup->addLabel("Min Halo Size:");
        lensGroup->addSlider("rend-halo-secondary-limit", Ranged(0, 10), .1, 1);

        lensGroup->addLabel("Halo Fading Start:");
        lensGroup->addSlider("rend-halo-dim-near", Ranged(0, 200), .1, 1);

        lensGroup->addLabel("Halo Fading End:");
        lensGroup->addSlider("rend-halo-dim-far", Ranged(0, 200), .1, 1);

        lensGroup->addLabel("Z-Mag Divisor:");
        lensGroup->addSlider("rend-halo-zmag-div", Ranged(1, 100), .1, 1);

        lensGroup->commit();

        // Material settings.
        matGroup = new Group(this, "material", "Materials");

        matGroup->addSpace();
        matGroup->addToggle("rend-tex-shiny", "Shiny Surfaces");

        matGroup->addSpace();
        matGroup->addToggle("rend-tex-anim-smooth", "Smooth Animation");

        matGroup->addLabel("Texture Quality:");
        matGroup->addSlider("rend-tex-quality");

        matGroup->addLabel("Texture Filtering:");
        matGroup->addChoice("rend-tex-mipmap")->items()
                << new ChoiceItem("None",                       0)
                << new ChoiceItem("Linear filter, no mip",      1)
                << new ChoiceItem("No filter, nearest mip",     2)
                << new ChoiceItem("Linear filter, nearest mip", 3)
                << new ChoiceItem("No filter, linear mip",      4)
                << new ChoiceItem("Linear filter, linear mip",  5);

        matGroup->addSpace();
        matGroup->addToggle("rend-tex-filter-smart", "2x Smart Filtering");

        matGroup->addLabel("Bilinear Filtering:");
        matGroup->addToggle("rend-tex-filter-sprite", "Sprites");

        matGroup->addSpace();
        matGroup->addToggle("rend-tex-filter-mag", "World Surfaces");

        matGroup->addSpace();
        matGroup->addToggle("rend-tex-filter-ui", "User Interface");

        matGroup->addLabel("Anisotropic Filter:");
        matGroup->addChoice("rend-tex-filter-anisotropic")->items()
                << new ChoiceItem("Best available", -1)
                << new ChoiceItem("Off", 0)
                << new ChoiceItem("2x",  1)
                << new ChoiceItem("4x",  2)
                << new ChoiceItem("8x",  3)
                << new ChoiceItem("16x", 4);

        matGroup->addSpace();
        matGroup->addToggle("rend-tex-detail", "Detail Textures");

        matGroup->addLabel("Scaling Factor:");
        matGroup->addSlider("rend-tex-detail-scale", Ranged(0, 16), .01, 2);

        matGroup->addLabel("Contrast:");
        matGroup->addSlider("rend-tex-detail-strength");

        matGroup->commit();

        // Model settings.
        modelGroup = new Group(this, "model", "3D Models");

        modelGroup->addSpace();
        modelGroup->addToggle("rend-model", "3D Models");

        modelGroup->addSpace();
        modelGroup->addToggle("rend-model-inter", "Interpolate Frames");

        modelGroup->addSpace();
        modelGroup->addToggle("rend-model-mirror-hud", "Mirror Player Weapon");

        modelGroup->addLabel("Max Visible Distance:");
        modelGroup->addSlider("rend-model-distance", Ranged(0, 3000), 10, 0)->setMinLabel("Inf");

        modelGroup->addLabel("LOD #0 Distance:");
        modelGroup->addSlider("rend-model-lod", Ranged(0, 1000), 1, 0)->setMinLabel("No LOD");

        modelGroup->addLabel("Number of Lights:");
        modelGroup->addSlider("rend-model-lights");

        modelGroup->commit();

        // Sprite settings.
        spriteGroup = new Group(this, "sprite", "Sprites");

        spriteGroup->addSpace();
        spriteGroup->addToggle("rend-sprite-blend", "Additive Blending");

        spriteGroup->addLabel("Number of Lights:");
        spriteGroup->addSlider("rend-sprite-lights")->setMinLabel("Inf");

        spriteGroup->addLabel("Sprite Alignment:");
        spriteGroup->addChoice("rend-sprite-align")->items()
                << new ChoiceItem("Camera", 0)
                << new ChoiceItem("View plane", 1)
                << new ChoiceItem("Camera (limited)", 2)
                << new ChoiceItem("View plane (limited)", 3);

        spriteGroup->addSpace();
        spriteGroup->addToggle("rend-sprite-mode", "Sharp Edges");

        spriteGroup->addSpace();
        spriteGroup->addToggle("rend-sprite-noz", "Disable Z-Write");

        spriteGroup->commit();

        // Object settings.
        objectGroup = new Group(this, "object", "Objects");

        objectGroup->addLabel("Smooth Movement:");
        objectGroup->addChoice("rend-mobj-smooth-move")->items()
                << new ChoiceItem("Disabled", 0)
                << new ChoiceItem("Models only", 1)
                << new ChoiceItem("Models and sprites", 2);

        objectGroup->addSpace();
        objectGroup->addToggle("rend-mobj-smooth-turn", "Smooth Turning");

        objectGroup->commit();

        // Particle settings.
        partGroup = new Group(this, "ptcfx", "Particle Effects");

        partGroup->addSpace();
        partGroup->addToggle("rend-particle", "Particle Effects");

        partGroup->addLabel("Max Particles:");
        partGroup->addSlider("rend-particle-max", Ranged(0, 10000), 100, 0)->setMinLabel("Inf");

        partGroup->addLabel("Spawn Rate:");
        partGroup->addSlider("rend-particle-rate");

        partGroup->addLabel("Diffusion:");
        partGroup->addSlider("rend-particle-diffuse", Ranged(0, 20), .01, 2);

        partGroup->addLabel("Near Clip Distance:");
        partGroup->addSlider("rend-particle-visible-near", Ranged(0, 1000), 1, 0)->setMinLabel("None");

        partGroup->commit();
    }

//    ~Impl()
//    {
//        settings.audienceForProfileChange -= this;
//    }

    const Rule &firstColumnWidthRule() const
    {
        return self().firstColumnWidth();
    }

    ScrollAreaWidget &containerWidget()
    {
        return self().containerWidget();
    }

    void resetToDefaults(const String &settingName)
    {
        settings.resetSettingToDefaults(settingName);
    }

    void currentProfileChanged(const String &)
    {
        // Update with values from the new profile.
        fetch();
    }

    void fetch()
    {
        const bool isReadOnly = settings.find(settings.currentProfile()).isReadOnly();

        for (GuiWidget *child : self().containerWidget().childWidgets())
        {
            if (Group *g = maybeAs<Group>(child))
            {
                g->setResetable(!isReadOnly);
                g->fetch();
                g->resetButton().enable(!isReadOnly && g->isOpen());

                // Enable or disable settings based on read-onlyness.
                for (GuiWidget *st : g->content().childWidgets())
                {
                    st->enable(!isReadOnly);
                }
            }
        }
    }

    void saveFoldState(PersistentState &toState)
    {
        for (GuiWidget *child : self().containerWidget().childWidgets())
        {
            if (Group *g = maybeAs<Group>(child))
            {
                toState.objectNamespace().set(self().name() + "." + g->name() + ".open",
                                              g->isOpen());
            }
        }
    }

    void restoreFoldState(const PersistentState &fromState)
    {
        bool gotState = false;

        for (GuiWidget *child : self().containerWidget().childWidgets())
        {
            if (Group *g = maybeAs<Group>(child))
            {
                const String var = self().name() + "." + g->name() + ".open";
                if (fromState.objectNamespace().has(var))
                {
                    gotState = true;
                    if (fromState.objectNamespace().getb(var))
                        g->open();
                    else
                        g->close(0.0);
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
    : SidebarWidget("Renderer Appearance", "rendererappearanceeditor")
    , d(new Impl(this))
{
    d->profile->setOpeningDirection(Down);

    LabelWidget *profLabel = LabelWidget::newWithText("Profile:", &containerWidget());

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
                        d->profile->button().rule().width());

    d->fetch();
}

void RendererAppearanceEditor::operator >> (PersistentState &toState) const
{
    d->saveFoldState(toState);
}

void RendererAppearanceEditor::operator << (const PersistentState &fromState)
{
    d->restoreFoldState(fromState);
}
