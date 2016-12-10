/** @file modelasseteditor.cpp  Editor for model asset parameters.
 *
 * @authors Copyright (c) 2015 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "ui/editors/modelasseteditor.h"
#include "ui/editors/variablegroupeditor.h"
#include "ui/clientwindow.h"
#include "clientapp.h"
#include "world/clientserverworld.h"
#include "world/map.h"
#include "world/thinkers.h"
#include "world/clientmobjthinkerdata.h"
#include "clientplayer.h"
#include "render/rend_main.h"
#include "render/rendersystem.h"
#include "render/stateanimator.h"
#include "render/playerweaponanimator.h"

#include <de/AnimationValue>
#include <de/App>
#include <de/DialogContentStylist>
#include <de/NumberValue>
#include <de/PackageLoader>
#include <de/ScriptedInfo>
#include <de/SequentialLayout>
#include <de/SignalAction>

using namespace de;
using namespace de::ui;

struct PlayData
{
    int mobjId;
    int animationId;

    PlayData(int mobj = 0, int seq = 0) : mobjId(mobj), animationId(seq) {}
};
Q_DECLARE_METATYPE(PlayData)

DENG_GUI_PIMPL(ModelAssetEditor)
, DENG2_OBSERVES(PackageLoader, Activity)
, DENG2_OBSERVES(PanelWidget, AboutToOpen)
, public VariableGroupEditor::IOwner
{
    using Group = VariableGroupEditor;

    LabelWidget *assetLabel;
    ChoiceWidget *assetChoice;
    String assetId;
    Package::Asset asset { nullptr };
    LabelWidget *info;
    LabelWidget *instLabel;
    ChoiceWidget *instChoice;
    QList<Group *> groups; ///< Generated based on asset subrecords.
    SafeWidgetPtr<ChoiceWidget> animChoice;
    SafeWidgetPtr<SliderWidget> offsetX;
    SafeWidgetPtr<SliderWidget> offsetY;
    SafeWidgetPtr<SliderWidget> offsetZ;

    Impl(Public *i) : Base(i)
    {
        App::packageLoader().audienceForActivity() += this;

        // The contents of the editor will scroll.
        GuiWidget *container = &self().containerWidget();

        container->add(assetChoice = new ChoiceWidget);
        assetChoice->popup().useInfoStyle();

        container->add(info = new LabelWidget);
        container->add(instChoice = new ChoiceWidget);
        instChoice->setNoSelectionHint(_E(l) + tr("Click to select"));
        instChoice->popup().useInfoStyle();
        instChoice->popup().audienceForAboutToOpen() += this;
        instLabel = LabelWidget::newWithText(tr("Instance:"), container);

        instChoice->rule()
                .setInput(Rule::Left, instLabel->rule().right())
                .setInput(Rule::Top,  instLabel->rule().top());
    }

    void setOfLoadedPackagesChanged()
    {
        updateAssetsList();
    }

    Rule const &firstColumnWidthRule() const { return self().firstColumnWidth(); }
    ScrollAreaWidget &containerWidget() { return self().containerWidget(); }

    void resetToDefaults(String const &/*settingName*/)
    {}

    static char const *pluralSuffix(int count, char const *suffix = "s")
    {
        return count != 1? suffix : "";
    }

    bool isWeaponAsset() const
    {
        return assetId.startsWith("model.weapon.");
    }

    /**
     * Sets the currently active model asset.
     * @param id  Asset identifier.
     */
    void setAsset(String id)
    {
        assetId = id;
        asset = App::asset(id);

        // Collect some information about the model asset.
        render::Model const &model = ClientApp::renderSystem().modelRenderer()
                .bank().model<render::Model>(id);

        QStringList animNames;
        for (int i = 0; i < model.animationCount(); ++i)
        {
            animNames << model.animationName(i);
        }

        QStringList meshNames;
        for (int i = 0; i < model.meshCount(); ++i)
        {
            meshNames << model.meshName(i);
        }

        QStringList materialNames;
        for (String n : model.materialIndexForName.keys())
        {
            materialNames << n;
        }

        setInfoLabelParams(*info);
        String msg = QString(_E(Ta)_E(l) "Path: " _E(.)_E(Tb) "%1\n"
                             _E(Ta)_E(l) "%2 Mesh%3: " _E(.)_E(Tb) "%4\n"
                             _E(Ta)_E(l) "%5 Material%6: " _E(.)_E(Tb) "%7")
                .arg(asset.absolutePath("path"))
                .arg(model.meshCount())
                .arg(pluralSuffix(model.meshCount(), "es"))
                .arg(meshNames.join(", "))
                .arg(materialNames.size())
                .arg(pluralSuffix(materialNames.size()))
                .arg(materialNames.join(", "));
        if (!animNames.isEmpty())
        {
            msg += QString(_E(Ta)_E(l) "\n%1 Animation%2: " _E(.)_E(Tb) "%3")
                    .arg(model.animationCount())
                    .arg(pluralSuffix(model.animationCount()))
                    .arg(animNames.join(", "));
        }
        msg += QString(_E(Ta)_E(l) "\nAutoscale: " _E(.)_E(Tb) "%1")
                .arg(asset.gets("autoscale", "False"));
        info->setText(msg);

        instChoice->items().clear();
        clearGroups();
        updateInstanceList();

        if (!instChoice->items().isEmpty())
        {
            instChoice->setSelected(0);
        }

        makeGroups();
        redoLayout();
    }

    void setInfoLabelParams(LabelWidget &label)
    {
        label.setFont("small");
        label.setTextColor("altaccent");
        label.setSizePolicy(ui::Fixed, ui::Expand);
        label.setAlignment(ui::AlignLeft);
        label.set(GuiWidget::Background(style().colors().colorf("altaccent") *
                                        Vector4f(1, 1, 1, .2f)));
        Rule const &maxWidth = rule("sidebar.width");
        label.rule().setInput(Rule::Width, maxWidth);
        label.setMaximumTextWidth(maxWidth);
    }

    void clearGroups()
    {
        foreach (Group *g, groups)
        {
            // Ownership of the optional title and reset button was given
            // to the container, but the container is not being destroyed.
            g->destroyAssociatedWidgets();
            GuiWidget::destroy(g);
        }
        groups.clear();
    }

    int idNumber()
    {
        return instChoice->selectedItem().data().toInt();
    }

    render::StateAnimator *assetAnimator()
    {
        if (instChoice->items().isEmpty())
        {
            return nullptr;
        }

        int const idNum = idNumber();
        if (isWeaponAsset())
        {
            auto &weaponAnim = ClientApp::player(idNum).playerWeaponAnimator();
            if (weaponAnim.hasModel())
            {
                return &weaponAnim.animator();
            }
        }
        else
        {
            if (mobj_t const *mo = ClientApp::world().map().thinkers().mobjById(idNum))
            {
                auto &mobjData = THINKER_DATA(mo->thinker, ClientMobjThinkerData);
                return mobjData.animator();
            }
        }
        return nullptr;
    }

    void makeGroups()
    {
        clearGroups();

        if (render::StateAnimator *anim = assetAnimator())
        {
            Record &ns = anim->objectNamespace();

            // Manual animation controls.
            Group *g = new Group(this, "", tr("Animations"));
            groups << g;
            g->addLabel(tr("Play:"));
            animChoice.reset(new ChoiceWidget);
            render::Model const &model = anim->model();
            for (int i = 0; i < model.animationCount(); ++i)
            {
                QVariant var;
                var.setValue(PlayData(idNumber(), i));
                animChoice->items() << new ChoiceItem(model.animationName(i), var);
            }
            g->addWidget(animChoice);
            QObject::connect(animChoice.get(), SIGNAL(selectionChangedByUser(uint)),
                             thisPublic, SLOT(playAnimation()));
            g->commit();

            // Positioning.
            g = new Group(this, "", tr("Position"));
            groups << g;
            {
                // Sliders for the offset vector.
                offsetX.reset(new SliderWidget);
                offsetY.reset(new SliderWidget);
                offsetZ.reset(new SliderWidget);

                g->addLabel("Offset X:");
                g->addWidget(offsetX);
                g->addLabel("Offset Y:");
                g->addWidget(offsetY);
                g->addLabel("Offset Z:");
                g->addWidget(offsetZ);

                offsetX->setRange(Ranged(-50, 50), .1);
                offsetY->setRange(Ranged(-50, 50), .1);
                offsetZ->setRange(Ranged(-50, 50), .1);
                offsetX->setPrecision(1);
                offsetY->setPrecision(1);
                offsetZ->setPrecision(1);
                offsetX->setValue(anim->model().offset.x);
                offsetY->setValue(anim->model().offset.y);
                offsetZ->setValue(anim->model().offset.z);

                QObject::connect(offsetX.get(), SIGNAL(valueChangedByUser(double)),
                                 thisPublic, SLOT(updateOffsetVector()));
                QObject::connect(offsetY.get(), SIGNAL(valueChangedByUser(double)),
                                 thisPublic, SLOT(updateOffsetVector()));
                QObject::connect(offsetZ.get(), SIGNAL(valueChangedByUser(double)),
                                 thisPublic, SLOT(updateOffsetVector()));
            }
            g->commit();

            // Animator variables.
            g = makeGroup(*anim, ns, tr("Variables"));
            g->open();
            groups << g;

            // Make a variable group for each subrecord.
            QMap<String, Group *> orderedGroups;
            ns.forSubrecords([this, anim, &orderedGroups] (String const &name, Record &rec)
            {
                orderedGroups.insert(name, makeGroup(*anim, rec, name, true));
                return LoopContinue;
            });
            for (auto *g : orderedGroups.values())
            {
                groups << g;
            }
        }
    }

    static String varLabel(String const &name)
    {
        return _E(m) + name + _E(.) + ":";
    }

    static String mobjItemLabel(thid_t id)
    {
        return QString("ID %1 " _E(l) "(dist:%2)")
                .arg(id).arg(int(distanceToMobj(id)));
    }

    static coord_t distanceToMobj(thid_t id)
    {
        mobj_t const *mo = Mobj_ById(id);
        if (mo)
        {
            return (Rend_EyeOrigin().xzy() - Vector3d(mo->origin)).length();
        }
        return -1;
    }

    void populateGroup(Group *g, Record &rec, bool descend, String const &namePrefix = "")
    {
        StringList names = rec.members().keys();
        qSort(names);

        for (String const &name : names)
        {
            if (name.startsWith("__") || name == "ID" || name == "uMapTime")
                continue;

            Variable &var = rec[name];

            Ranged range(0, 2);
            double step = .01;
            int precision = 2;

            /// @todo Hardcoded... -jk

            if (name == "uGlossiness")
            {
                range = Ranged(0, 200);
                step = 1;
                precision = 1;
            }
            if (name == "uReflectionBlur")
            {
                range = Ranged(0, 40);
                step = .1;
                precision = 1;
            }
            if (name == "uSpecular")
            {
                range = Ranged(0, 10);
                step = .1;
                precision = 1;
            }

            String label = namePrefix.concatenateMember(name);

            if (NumberValue const *num = var.value().maybeAs<NumberValue>())
            {
                if (num->semanticHints().testFlag(NumberValue::Boolean))
                {
                    g->addSpace();
                    g->addToggle(var, _E(m) + label);
                }
                else
                {
                    g->addLabel(varLabel(label));
                    g->addSlider(var, range, step, precision);
                }
            }
            else if (var.value().is<TextValue>())
            {
                g->addLabel(varLabel(label));
                g->addLineEdit(var);
            }
            else if (var.value().is<AnimationValue>())
            {
                g->addLabel(varLabel(label));
                g->addSlider(var, range, step, precision);
            }
            else if (descend && var.value().is<RecordValue>())
            {
                populateGroup(g, var.valueAsRecord(), descend, label);
            }
        }
    }

    Group *makeGroup(render::StateAnimator &animator, Record &rec,
                     String titleText, bool descend = false)
    {
        LabelWidget *info = nullptr;

        // Check for a rendering pass.
        int passIndex = animator.model().passes.findName(titleText);
        if (passIndex >= 0)
        {
            titleText = QString("Render Pass \"%1\"").arg(titleText);

            // Look up the shader.
            auto const &pass = animator.model().passes.at(passIndex);
            String shaderName = ClientApp::renderSystem().modelRenderer()
                    .shaderName(*pass.program);
            Record const &shaderDef = ClientApp::renderSystem().modelRenderer()
                    .shaderDefinition(*pass.program);

            // Check the variable declarations.
            auto vars = ScriptedInfo::subrecordsOfType(QStringLiteral("variable"),
                                                       shaderDef).keys();
            qSort(vars);
            QStringList names;
            for (String const &n : vars)
            {
                // Used variables are shown in bold.
                if (rec.has(n))
                    names << _E(b) + n + _E(.);
                else
                    names << n;
            }
            String msg = QString(_E(Ta)_E(l) "Shader: " _E(.)_E(Tb) "%1\n"
                                 _E(Ta)_E(l) "Variables: " _E(.)_E(Tb) "%2")
                    .arg(shaderName)
                    .arg(names.join(", "));

            info = new LabelWidget;
            info->setText(msg);
        }

        std::unique_ptr<Group> g(new Group(this, "", titleText, info));
        g->setResetable(false);
        populateGroup(g.get(), rec, descend);
        g->commit();
        if (info)
        {
            setInfoLabelParams(*info);
        }
        return g.release();
    }

    void redoLayout()
    {
        SequentialLayout &layout = self().layout();
        layout.clear();
        layout << *info
               << *instLabel;
        foreach (Group *g, groups)
        {
            layout << g->title() << *g;
        }
        self().updateSidebarLayout(assetLabel->rule().width() +
                                 assetChoice->rule().width(),
                                 assetChoice->rule().height());
    }

    void updateInstanceList()
    {
        instChoice->items().clear();

        if (!ClientApp::world().hasMap())
            return;

        if (isWeaponAsset())
        {
            for (int idx = 0; idx < ClientApp::players().count(); ++idx)
            {
                auto &client = ClientApp::player(idx);
                auto &anim = client.playerWeaponAnimator();
                if (anim.hasModel())
                {
                    if (anim.animator()["ID"] == assetId)
                    {
                        instChoice->items()
                                << new ChoiceItem(QString("Player %1").arg(idx), idx);
                    }
                }
            }
        }
        else
        {
            ClientApp::world().map().thinkers().forAll(0x1 /* public */, [this] (thinker_t *th)
            {
                auto const *mobjData = THINKER_DATA_MAYBE(*th, ClientMobjThinkerData);
                if (mobjData && mobjData->animator())
                {
                    render::StateAnimator const *anim = mobjData->animator();

                    if (anim && (*anim)["ID"] == assetId)
                    {
                        instChoice->items() << new ChoiceItem(mobjItemLabel(th->id), th->id);
                    }
                }
                return LoopContinue;
            });
            sortInstancesByDistance(false);
        }
    }

    void sortInstancesByDistance(bool rememberSelection)
    {
        if (isWeaponAsset() || instChoice->items().isEmpty())
            return;

        int selId = (instChoice->isValidSelection()?
                     instChoice->selectedItem().data().toInt() : 0);

        // Update the distances.
        instChoice->items().forAll([this] (Item &a)
        {
            a.as<ChoiceItem>().setLabel(mobjItemLabel(a.data().toInt()));
            return LoopContinue;
        });
        instChoice->items().sort([this] (Item const &a, Item const &b)
        {
            return distanceToMobj(a.data().toInt()) < distanceToMobj(b.data().toInt());
        });

        if (rememberSelection && selId > 0)
        {
            instChoice->setSelected(instChoice->items().findData(selId));
        }
    }

    void panelAboutToOpen(PanelWidget &)
    {
        sortInstancesByDistance(true);
    }

    void updateAssetsList()
    {
        assetChoice->items().clear();

        App::rootFolder().locate<Folder const>("/packs").forContents([this] (String name, File &)
        {
            QRegExp regex("asset\\.(model\\.((thing|weapon)\\..*))");
            if (regex.exactMatch(name))
            {
                assetChoice->items() << new ChoiceItem(regex.cap(2), regex.cap(1));
            }
            return LoopContinue;
        });
    }

    void playSelectedAnimation()
    {
        if (!animChoice || !animChoice->isValidSelection())
            return;

        PlayData const data = animChoice->selectedItem().data().value<PlayData>();
        render::StateAnimator *animator = nullptr;

        if (isWeaponAsset())
        {
            auto &weapon = ClientApp::player(data.mobjId).playerWeaponAnimator();
            if (weapon.hasModel()) animator = &weapon.animator();
        }
        else
        {
            if (mobj_t const *mo = Mobj_ById(data.mobjId))
            {
                if (auto *thinker = THINKER_DATA_MAYBE(mo->thinker, ClientMobjThinkerData))
                {
                    animator = thinker->animator();
                }
            }
        }

        if (animator)
        {
            animator->startSequence(data.animationId, 10, false);
        }
    }

    void updateOffsetVector()
    {
        if (!offsetX) return;
        if (render::StateAnimator *anim = assetAnimator())
        {
            render::Model *model = const_cast<render::Model *>(&anim->model());
            model->offset = Vector3f(offsetX->value(), offsetY->value(), offsetZ->value());
        }
    }
};

ModelAssetEditor::ModelAssetEditor()
    : SidebarWidget(tr("Edit 3D Model"), "modelasseteditor")
    , d(new Impl(this))
{
    d->assetLabel = LabelWidget::newWithText(tr("Asset:"), &containerWidget());

    // Layout.
    d->assetLabel->rule()
            .setInput(Rule::Left, containerWidget().contentRule().left())
            .setInput(Rule::Top,  title().rule().bottom());
    d->assetChoice->rule()
            .setInput(Rule::Left, d->assetLabel->rule().right())
            .setInput(Rule::Top,  d->assetLabel->rule().top());

    // Update container size.
    updateSidebarLayout(d->assetLabel->rule().width() +
                        d->assetChoice->rule().width(),
                        d->assetChoice->rule().height());
    layout().setStartY(d->assetChoice->rule().bottom());

    d->updateAssetsList();

    connect(d->assetChoice, SIGNAL(selectionChangedByUser(uint)), this, SLOT(setSelectedAsset(uint)));
    connect(d->instChoice, SIGNAL(selectionChangedByUser(uint)), this, SLOT(setSelectedInstance(uint)));
}

void ModelAssetEditor::setSelectedAsset(uint pos)
{
    d->setAsset(d->assetChoice->items().at(pos).data().toString());
}

void ModelAssetEditor::setSelectedInstance(uint /*pos*/)
{
    d->makeGroups();
    d->redoLayout();
}

void ModelAssetEditor::playAnimation()
{
    d->playSelectedAnimation();
}

void ModelAssetEditor::updateOffsetVector()
{
    d->updateOffsetVector();
}
