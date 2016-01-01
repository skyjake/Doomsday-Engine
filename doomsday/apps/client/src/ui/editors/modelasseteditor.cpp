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
#include "render/rend_main.h"
#include "render/stateanimator.h"

#include <de/NumberValue>
#include <de/NativeValue>
#include <de/DialogContentStylist>
#include <de/SequentialLayout>
#include <de/SignalAction>
#include <de/App>

using namespace de;
using namespace de::ui;

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

    Instance(Public *i) : Base(i)
    {
        App::packageLoader().audienceForActivity() += this;

        // The contents of the editor will scroll.
        GuiWidget *container = &self.containerWidget();

        container->add(assetChoice = new ChoiceWidget);
        assetChoice->popup().useInfoStyle();

        container->add(info = new LabelWidget);
        info->setFont("small");
        info->setTextColor("altaccent");

        container->add(instChoice = new ChoiceWidget);
        instChoice->popup().useInfoStyle();
        instChoice->popup().audienceForAboutToOpen() += this;
        instLabel = LabelWidget::newWithText(tr("Instance:"), container);

        instChoice->rule()
                .setInput(Rule::Left, instLabel->rule().right())
                .setInput(Rule::Top,  instLabel->rule().top());
    }

    ~Instance()
    {
        App::packageLoader().audienceForActivity() -= this;
    }

    void setOfLoadedPackagesChanged()
    {
        updateAssetsList();
    }

    Rule const &firstColumnWidthRule() const { return self.firstColumnWidth(); }
    ScrollAreaWidget &containerWidget() { return self.containerWidget(); }

    void resetToDefaults(String const &/*settingName*/)
    {}

    void setAsset(String id)
    {
        assetId = id;
        asset = App::asset(id);

        instChoice->items().clear();

        info->setSizePolicy(ui::Fixed, ui::Expand);
        info->setAlignment(ui::AlignLeft);
        Rule const &maxWidth = style().rules().rule("sidebar.width");
        info->rule().setInput(Rule::Width, maxWidth);
        info->setMaximumTextWidth(maxWidth);
        info->setText(QString(_E(Ta)_E(l) "Path: " _E(.)_E(Tb) "%1\n"
                              _E(Ta)_E(l) "Autoscale: " _E(.)_E(Tb) "%2")
                      .arg(asset.absolutePath("path"))
                      .arg(asset.gets("autoscale", "False")));

        clearGroups();
        updateInstanceList();

        if(!instChoice->items().isEmpty())
        {
            instChoice->setSelected(0);
        }

        makeGroups();
        redoLayout();
    }

    void clearGroups()
    {
        foreach(Group *g, groups)
        {
            // Ownership of the optional title and reset button was given
            // to the container, but the container is not being destroyed.
            g->destroyAssociatedWidgets();
            GuiWidget::destroy(g);
        }
        groups.clear();
    }

    void makeGroups()
    {
        clearGroups();

        if(instChoice->items().isEmpty()) return;

        int const mobjId = instChoice->selectedItem().data().toInt();
        mobj_t const *mo = ClientApp::world().map().thinkers().mobjById(mobjId);
        if(mo)
        {
            auto &mobjData = THINKER_DATA(mo->thinker, ClientMobjThinkerData);
            render::StateAnimator *anim = mobjData.animator();
            DENG2_ASSERT(anim);
            Record &ns = anim->objectNamespace();

            Group *g = makeGroup(ns, tr("Variables"));
            g->open();
            groups << g;

            // Make a variable group for each subrecord.
            QMap<String, Group *> orderedGroups;
            ns.forSubrecords([this, &orderedGroups] (String const &name, Record &rec)
            {
                orderedGroups.insert(name, makeGroup(rec, name));
                return LoopContinue;
            });
            for(auto *g : orderedGroups.values())
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
        if(mo)
        {
            return (Rend_EyeOrigin().xzy() - Vector3d(mo->origin)).length();
        }
        return -1;
    }

    Group *makeGroup(Record &rec, String const &titleText)
    {
        Group *g = new Group(this, "", titleText);
        g->setResetable(false);

        StringList names = rec.members().keys();
        qSort(names);

        for(String const &name : names)
        {
            if(name.startsWith("__") || name == "ID" || name == "uMapTime")
                continue;

            Variable &var = rec[name];

            Ranged range(0, 2);
            double step = .01;
            int precision = 2;

            if(name == "uGlossiness")
            {
                range = Ranged(0, 200);
                step = 1;
                precision = 1;
            }
            if(name == "uReflectionBlur")
            {

                range = Ranged(0, 40);
                step = .1;
                precision = 1;
            }
            if(name == "uSpecular")
            {
                range = Ranged(0, 10);
                step = .1;
                precision = 1;
            }

            if(NumberValue const *num = var.value().maybeAs<NumberValue>())
            {
                if(num->semanticHints().testFlag(NumberValue::Boolean))
                {
                    g->addSpace();
                    g->addToggle(var, _E(m) + name);
                }
                else
                {
                    g->addLabel(varLabel(name));
                    g->addSlider(var, range, step, precision);
                }
            }
            else if(var.value().is<TextValue>())
            {
                g->addLabel(varLabel(name));
                g->addLineEdit(var);
            }
            else if(var.value().is<NativeValue>())
            {
                g->addLabel(varLabel(name));
                g->addSlider(var, range, step, precision);
            }
        }

        g->commit();
        return g;
    }

    void redoLayout()
    {
        SequentialLayout &layout = self.layout();
        layout.clear();
        layout << *info;
        layout << *instLabel;
        foreach(Group *g, groups)
        {
            layout << g->title() << *g;
        }

        self.updateSidebarLayout(assetLabel->rule().width() +
                                 assetChoice->rule().width());
    }

    void updateInstanceList()
    {
        instChoice->items().clear();

        if(!ClientApp::world().hasMap())
            return;

        ClientApp::world().map().thinkers().forAll(0x1 /* public */, [this] (thinker_t *th)
        {
            auto const *mobjData = THINKER_DATA_MAYBE(*th, ClientMobjThinkerData);
            if(mobjData && mobjData->animator())
            {
                render::StateAnimator const *anim = mobjData->animator();

                if(anim && (*anim)[QStringLiteral("ID")] == assetId)
                {
                    instChoice->items() << new ChoiceItem(mobjItemLabel(th->id), th->id);
                }
            }
            return LoopContinue;
        });

        sortInstancesByDistance(false);
    }

    void sortInstancesByDistance(bool rememberSelection)
    {
        if(instChoice->items().isEmpty()) return;

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

        if(rememberSelection && selId > 0)
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

        for(auto i : App::rootFolder().locate<Folder const>("/packs").contents())
        {
            QRegExp regex("asset\\.(model\\.(thing\\..*))");
            if(regex.exactMatch(i.first))
            {
                assetChoice->items() << new ChoiceItem(regex.cap(2), regex.cap(1));
            }
        }
    }
};

ModelAssetEditor::ModelAssetEditor()
    : SidebarWidget(tr("Model Asset"), "modelasseteditor")
    , d(new Instance(this))
{
    d->assetLabel = LabelWidget::newWithText(tr("Asset:"), &containerWidget());

    // Layout.
    layout().append(*d->assetLabel, SequentialLayout::IgnoreMinorAxis);
    d->assetChoice->rule()
            .setInput(Rule::Left, d->assetLabel->rule().right())
            .setInput(Rule::Top,  d->assetLabel->rule().top());

    // Update container size.
    updateSidebarLayout(d->assetLabel->rule().width() +
                        d->assetChoice->rule().width());
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
