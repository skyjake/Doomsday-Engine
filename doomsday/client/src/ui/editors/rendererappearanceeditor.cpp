/** @file rendererappearanceeditor.cpp
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

#include "ui/editors/rendererappearanceeditor.h"
#include "ui/dialogs/renderersettingsdialog.h"
#include "ui/widgets/buttonwidget.h"
#include "ui/widgets/scrollareawidget.h"
#include "ui/widgets/cvarchoicewidget.h"
#include "ui/widgets/cvarsliderwidget.h"
#include "ui/widgets/cvartogglewidget.h"
#include "ui/widgets/foldpanelwidget.h"
#include "ui/clientwindow.h"
#include "DialogContentStylist"
#include "SequentialLayout"
#include "SignalAction"
#include "clientapp.h"

using namespace de;
using namespace ui;

class Group : public FoldPanelWidget
{
public:
    Group(GuiWidget *parent, String const &titleText)
    {
        _group = new GuiWidget;
        setContent(_group);
        title().setText(titleText);
        title().setTextColor("accent");

        _layout.setGridSize(2, 0);
        _layout.setColumnAlignment(0, AlignRight);
        _layout.setLeftTop(_group->rule().left(), _group->rule().top());

        parent->add(&title());
        parent->add(this);
    }

    void addSpace()
    {
        _layout << Const(0);
    }

    void addLabel(String const &text)
    {
        _layout << *LabelWidget::newWithText(text, _group);
    }

    CVarToggleWidget *addToggle(char const *cvar, String const &label)
    {
        CVarToggleWidget *w = new CVarToggleWidget(cvar);
        w->setText(label);
        _group->add(w);
        _layout << *w;
        return w;
    }

    CVarChoiceWidget *addChoice(char const *cvar)
    {
        CVarChoiceWidget *w = new CVarChoiceWidget(cvar);
        w->setOpeningDirection(ui::Up);
        _group->add(w);
        _layout << *w;
        return w;
    }

    CVarSliderWidget *addSlider(char const *cvar)
    {
        CVarSliderWidget *w = new CVarSliderWidget(cvar);
        _group->add(w);
        _layout << *w;
        return w;
    }

    CVarSliderWidget *addSlider(char const *cvar, Ranged const &range, double step, int precision)
    {
        CVarSliderWidget *w = addSlider(cvar);
        w->setRange(range, step);
        w->setPrecision(precision);
        return w;
    }

    void fetch()
    {
        foreach(Widget *child, _group->childWidgets())
        {
            if(ICVarWidget *w = child->maybeAs<ICVarWidget>())
            {
                w->updateFromCVar();
            }
        }
    }

    void commit()
    {
        _group->rule().setSize(_layout.width(), _layout.height());
    }

private:
    GuiWidget *_group;
    GridLayout _layout;
};

DENG_GUI_PIMPL(RendererAppearanceEditor)
{
    SettingsRegister &settings;
    DialogContentStylist stylist;
    ScrollAreaWidget *container;
    ButtonWidget *conf;
    ButtonWidget *close;    

    Group *skyGroup;
    Group *shadowGroup;
    Group *lightGroup;
    Group *glowGroup;
    Group *haloGroup;
    Group *texGroup;
    Group *modelGroup;
    Group *spriteGroup;
    Group *objectGroup;
    Group *partGroup;

    Instance(Public *i)
        : Base(i),
          settings(ClientApp::rendererAppearanceSettings())
    {
        // The contents of the editor will scroll.
        container = new ScrollAreaWidget;
        stylist.setContainer(*container);

        container->add(conf  = new ButtonWidget);
        container->add(close = new ButtonWidget);

        // Button for showing renderer settings.
        conf->setImage(style().images().image("gear"));
        conf->setOverrideImageSize(style().fonts().font("default").height().value());
        conf->setAction(new SignalAction(thisPublic, SLOT(showRendererSettings())));

        close->setText(tr("Close"));
        close->setAction(new SignalAction(thisPublic, SLOT(close())));

        // Sky settings.
        skyGroup = new Group(container, tr("Sky"));

        skyGroup->addLabel(tr("Sky Sphere Radius:"));
        skyGroup->addSlider("rend-sky-distance", Ranged(0, 8000), 10, 0);

        skyGroup->commit();

        // Shadow settings.
        shadowGroup = new Group(container, tr("Shadows"));

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
        lightGroup = new Group(container, "Dynamic Lights");

        lightGroup->addLabel(tr("Dynamic Lights:"));
        lightGroup->addChoice("rend-light")->items()
                << new ChoiceItem(tr("Enabled"), 1)
                << new ChoiceItem(tr("Disabled"), 0)
                << new ChoiceItem(tr("Process without drawing"), 2);

        lightGroup->addSpace();
        lightGroup->addToggle("rend-light-decor", tr("Light Decorations"));

        lightGroup->addLabel(tr("Blending Mode:"));
        lightGroup->addChoice("rend-light-blend")->items()
                << new ChoiceItem(tr("Multiply"), 0)
                << new ChoiceItem(tr("Add"), 1)
                << new ChoiceItem(tr("Process without drawing"), 2);

        lightGroup->addLabel(tr("Number of Lights:"));
        lightGroup->addSlider("rend-light-num", Ranged(0, 2000), 1, 0)->setMinLabel("Max");

        lightGroup->addLabel(tr("Light Brightness:"));
        lightGroup->addSlider("rend-light-bright");

        lightGroup->addLabel(tr("Light Radius Factor:"));
        lightGroup->addSlider("rend-light-radius-scale");

        lightGroup->addLabel(tr("Light Max Radius:"));
        lightGroup->addSlider("rend-light-radius-max");

        lightGroup->addLabel(tr("Ambient Light:"));
        lightGroup->addSlider("rend-light-ambient");

        lightGroup->addLabel(tr("Light Compression:"));
        lightGroup->addSlider("rend-light-compression");

        lightGroup->commit();

        // Glow settings.
        glowGroup = new Group(container, tr("Surface Glow"));

        glowGroup->addLabel(tr("Material Glow:"));
        glowGroup->addSlider("rend-glow");

        glowGroup->addLabel(tr("Max Glow Height:"));
        glowGroup->addSlider("rend-glow-height");

        glowGroup->addLabel(tr("Glow Height Factor:"));
        glowGroup->addSlider("rend-glow-scale");

        glowGroup->addLabel(tr("Brightness in Fog:"));
        glowGroup->addSlider("rend-light-fog-bright");

        glowGroup->addSpace();
        glowGroup->addToggle("rend-glow-wall", tr("Glow Visible on Walls"));

        glowGroup->commit();

        // Halo and lens flare settings.
        haloGroup = new Group(container, tr("Lens Flares & Halos"));

        haloGroup->addSpace();
        haloGroup->addToggle("rend-halo-realistic", tr("Realistic Halos"));

        haloGroup->addLabel(tr("Flares per Halo:"));
        haloGroup->addSlider("rend-halo")->setMinLabel(tr("None"));

        haloGroup->addLabel(tr("Halo Brightness:"));
        haloGroup->addSlider("rend-halo-bright", Ranged(0, 100), 1, 0);

        haloGroup->addLabel(tr("Halo Size Factor:"));
        haloGroup->addSlider("rend-halo-size", Ranged(0, 100), 1, 0);

        haloGroup->addLabel(tr("Occlusion Fading:"));
        haloGroup->addSlider("rend-halo-occlusion", Ranged(1, 256), 1, 0);

        haloGroup->addLabel(tr("Min Halo Radius:"));
        haloGroup->addSlider("rend-halo-radius-min", Ranged(1, 80), .1, 1);

        haloGroup->addLabel(tr("Min Halo Size:"));
        haloGroup->addSlider("rend-halo-secondary-limit", Ranged(0, 10), .1, 1);

        haloGroup->addLabel(tr("Halo Fading Start:"));
        haloGroup->addSlider("rend-halo-dim-near", Ranged(0, 200), .1, 1);

        haloGroup->addLabel(tr("Halo Fading End:"));
        haloGroup->addSlider("rend-halo-dim-far", Ranged(0, 200), .1, 1);

        haloGroup->addLabel(tr("Z-Mag Divisor:"));
        haloGroup->addSlider("rend-halo-zmag-div", Ranged(1, 200), .1, 1);

        haloGroup->commit();

        // Texture settings.
        texGroup = new Group(container, tr("Textures"));

        texGroup->addLabel(tr("Filtering Mode:"));
        texGroup->addChoice("rend-tex-mipmap")->items()
                << new ChoiceItem(tr("None"),                       0)
                << new ChoiceItem(tr("Linear filter, no mip"),      1)
                << new ChoiceItem(tr("No filter, nearest mip"),     2)
                << new ChoiceItem(tr("Linear filter, nearest mip"), 3)
                << new ChoiceItem(tr("No filter, linear mip"),      4)
                << new ChoiceItem(tr("Linear filter, linear mip"),  5);

        texGroup->addLabel(tr("Texture Quality:"));
        texGroup->addSlider("rend-tex-quality");

        texGroup->addSpace();
        texGroup->addToggle("rend-tex-anim-smooth", tr("Smooth Blend Animation"));

        texGroup->addSpace();
        texGroup->addToggle("rend-tex-filter-smart", tr("2x Smart Filtering"));

        texGroup->addLabel(tr("Bilinear Filtering:"));
        texGroup->addToggle("rend-tex-filter-sprite", tr("Sprites"));

        texGroup->addSpace();
        texGroup->addToggle("rend-tex-filter-mag", tr("World Surfaces"));

        texGroup->addSpace();
        texGroup->addToggle("rend-tex-filter-ui", tr("User Interface"));

        texGroup->addLabel(tr("Anisotopic Filter:"));
        texGroup->addChoice("rend-tex-filter-anisotropic")->items()
                << new ChoiceItem(tr("Best available"), -1)
                << new ChoiceItem(tr("Off"), 0)
                << new ChoiceItem(tr("2x"),  1)
                << new ChoiceItem(tr("4x"),  2)
                << new ChoiceItem(tr("8x"),  3)
                << new ChoiceItem(tr("16x"), 4);

        texGroup->addSpace();
        texGroup->addToggle("rend-tex-detail", tr("Detail Textures"));

        texGroup->addLabel(tr("Scaling Factor:"));
        texGroup->addSlider("rend-tex-detail-scale", Ranged(0, 16), .01, 2);

        texGroup->addLabel(tr("Contrast:"));
        texGroup->addSlider("rend-tex-detail-strength");

        texGroup->commit();

        // Model settings.
        modelGroup = new Group(container, tr("3D Models"));

        modelGroup->addSpace();
        modelGroup->addToggle("rend-model", tr("3D Models"));

        modelGroup->addSpace();
        modelGroup->addToggle("rend-model-inter", tr("Interpolate Frames"));

        modelGroup->addLabel(tr("Max Visible Distance:"));
        modelGroup->addSlider("rend-model-distance", Ranged(0, 3000), 10, 0)->setMinLabel(tr("Inf"));

        modelGroup->addLabel(tr("LOD #0 Distance:"));
        modelGroup->addSlider("rend-model-lod", Ranged(0, 1000), 10, 0)->setMinLabel(tr("No LOD"));

        modelGroup->addLabel(tr("Number of Lights:"));
        modelGroup->addSlider("rend-model-lights");

        modelGroup->commit();

        // Sprite settings.
        spriteGroup = new Group(container, tr("Sprites"));

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
        spriteGroup->addToggle("rend-sprite-noz", tr("Disable Z-Write"));

        spriteGroup->commit();

        // Object settings.
        objectGroup = new Group(container, tr("Objects"));

        objectGroup->addLabel(tr("Smooth Movement:"));
        objectGroup->addChoice("rend-mobj-smooth-move")->items()
                << new ChoiceItem(tr("Disabled"), 0)
                << new ChoiceItem(tr("Models only"), 1)
                << new ChoiceItem(tr("Models and sprites"), 2);

        objectGroup->addSpace();
        objectGroup->addToggle("rend-mobj-smooth-turn", tr("Smooth Turning"));

        objectGroup->commit();

        // Particle settings.
        partGroup = new Group(container, tr("Particle Effects"));

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

    void fetch()
    {
        foreach(Widget *child, container->childWidgets())
        {
            if(Group *g = child->maybeAs<Group>())
            {
                g->fetch();
            }
        }
    }
};

RendererAppearanceEditor::RendererAppearanceEditor()
    : PanelWidget("rendererappearanceeditor"), d(new Instance(this))
{
    setSizePolicy(Fixed);
    setOpeningDirection(Left);
    set(Background(style().colors().colorf("background")).withSolidFillOpacity(1));

    // Set up the editor UI.
    LabelWidget *title = LabelWidget::newWithText("Renderer Appearance", d->container);
    title->setFont("title");
    title->setTextColor("accent");

    // Layout.
    RuleRectangle const &area = d->container->contentRule();
    title->rule()
            .setInput(Rule::Top,  area.top())
            .setInput(Rule::Left, area.left());
    d->close->rule()
            .setInput(Rule::Right, area.right())
            .setInput(Rule::Top,   area.top());
    d->conf->rule()
            .setInput(Rule::Right, d->close->rule().left())
            .setInput(Rule::Top,   area.top());

    SequentialLayout layout(area.left(), title->rule().bottom(), Down);

    layout
           << d->lightGroup->title()  << *d->lightGroup
           << d->haloGroup->title()   << *d->haloGroup
           << d->glowGroup->title()   << *d->glowGroup
           << d->shadowGroup->title() << *d->shadowGroup
           << d->texGroup->title()    << *d->texGroup
           << d->objectGroup->title() << *d->objectGroup
           << d->modelGroup->title()  << *d->modelGroup
           << d->spriteGroup->title() << *d->spriteGroup
           << d->partGroup->title()   << *d->partGroup
           << d->skyGroup->title()    << *d->skyGroup;

    // Update container size.
    d->container->setContentSize(OperatorRule::maximum(layout.width(),
                                                       style().rules().rule("sidebar.width")),
                                 title->rule().height() + layout.height());
    d->container->rule().setSize(d->container->contentRule().width() +
                                 d->container->margins().width(),
                                 rule().height());
    setContent(d->container);

    d->fetch();

    // Install the editor.
    ClientWindow::main().setSidebar(ClientWindow::RightEdge, this);
}

void RendererAppearanceEditor::showRendererSettings()
{
    RendererSettingsDialog *dlg = new RendererSettingsDialog;
    dlg->setDeleteAfterDismissed(true);
    dlg->setAnchorAndOpeningDirection(d->conf->rule(), Down);
    root().add(dlg);
    dlg->open();
}

void RendererAppearanceEditor::preparePanelForOpening()
{
    PanelWidget::preparePanelForOpening();
}

void RendererAppearanceEditor::panelDismissed()
{
    PanelWidget::panelDismissed();

    ClientWindow::main().unsetSidebar(ClientWindow::RightEdge);
}
