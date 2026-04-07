/** @file repositorybrowserdialog.cpp  Remote package repository browser.
 *
 * @authors Copyright (c) 2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "ui/dialogs/repositorybrowserdialog.h"
#include "ui/widgets/homemenuwidget.h"
#include "ui/widgets/homeitemwidget.h"

#include <de/async.h>
#include <de/choicewidget.h>
#include <de/config.h>
#include <de/dictionaryvalue.h>
#include <de/documentwidget.h>
#include <de/filesystem.h>
#include <de/folder.h>
#include <de/lineeditwidget.h>
#include <de/progresswidget.h>
#include <de/filesys/remotefeedrelay.h>
#include <de/sequentiallayout.h>
#include <de/taskpool.h>
#include <de/togglewidget.h>
#include <de/ui/filtereddata.h>
#include <de/webrequest.h>

using namespace de;

DE_STATIC_STRING(VAR_RESOURCE_BROWSER_REPOSITORY, "resource.browserRepository");
DE_STATIC_STRING(ALL_CATEGORIES, "All Categories");

DE_GUI_PIMPL(RepositoryBrowserDialog)
, DE_OBSERVES(filesys::RemoteFeedRelay, Status)
, public ChildWidgetOrganizer::IWidgetFactory
{
    using RFRelay = filesys::RemoteFeedRelay;

    ui::ListData categoryData;
    std::shared_ptr<ui::ListData> data;
    std::unique_ptr<ui::FilteredData> shownData;
    std::unique_ptr<AsyncScope> populating;
    Lockable linkBusy;

    ProgressWidget *refreshProgress;
    ChoiceWidget *repo;
    LineEditWidget *search;
    MenuWidget *category;
    LabelWidget *statusText;
    HomeMenuWidget *nameList;
    DocumentWidget *description;
    String connectedRepository;
    String mountPath;
    Set<String> filterTerms;

    TaskPool tasks;

    Impl(Public *i) : Base(i)
    {
        auto &area = self().area();

        // The dialog contains scrollable widgets.
        area.enableScrolling(false);
        area.enableIndicatorDraw(false);
        area.enablePageKeys(false);

        self().add(refreshProgress = new ProgressWidget);

        refreshProgress->useMiniStyle();
        refreshProgress->setOpacity(0);
        refreshProgress->setColor("altaccent");
        refreshProgress->setTextColor("altaccent");
        refreshProgress->setText("Loading...");
        refreshProgress->setTextAlignment(ui::AlignLeft);
        refreshProgress->setSizePolicy(ui::Expand, ui::Expand);

        area.add(statusText  = new LabelWidget);
        area.add(repo        = new ChoiceWidget);
        area.add(search      = new LineEditWidget);
        area.add(category    = new MenuWidget);
        area.add(nameList    = new HomeMenuWidget);
        area.add(description = new DocumentWidget);

        //statusText->setFont("small");
        statusText->setTextColor("altaccent");

        // Insert known repositories.
        try
        {
            auto &repos = Config::get().getdt("resource.repositories");
            for (auto i = repos.elements().begin(); i != repos.elements().end(); ++i)
            {
                repo->items()
                        << new ChoiceItem(i->first.value->asText(), i->second->asText());
            }
            repo->setSelected(
                repo->items().findLabel(App::config().gets(VAR_RESOURCE_BROWSER_REPOSITORY(), "")));
        }
        catch (const Error &er)
        {
            LOG_MSG("Remote repositories not listed in configuration; "
                    "set Config.resource.repositories: %s")
                    << er.asText();
        }

        category->setItems(categoryData);
        category->setGridSize(0, ui::Expand, 1, ui::Expand, GridLayout::RowFirst);
        category->organizer().setWidgetFactory(*this);

        nameList->setVirtualizationEnabled(true, style().fonts().font("default").height().valuei() +
                                           rule("unit").valuei() * 2);
        nameList->organizer().setWidgetFactory(*this);
        nameList->enableScrolling(true);
        nameList->enablePageKeys(true);
        nameList->enableIndicatorDraw(true);
        nameList->setGridSize(1, ui::Filled, 0, ui::Fixed);
        nameList->layout().setRowPadding(Const(0));
        nameList->setBehavior(ChildVisibilityClipping);

        repo->audienceForUserSelection() += [this]() { updateSelectedRepository(); };
        search->audienceForContentChange() += [this]() { updateFilter(); };
        RFRelay::get().audienceForStatus() += this;

        updateSelectedRepository();
    }

    ~Impl() override
    {
        if (populating) populating->waitForFinished();
        disconnect();
    }

    bool filterItem(const ui::Item &item) const
    {
        if (filterTerms.isEmpty()) return true;

        DotPath const path(item.label());
        for (const String &term : filterTerms)
        {
            bool matched = false;
            for (int i = 0; i < path.segmentCount(); ++i)
            {
                if (path.segment(i).toLowercaseString().contains(term))
                {
                    matched = true;
                    break;
                }
            }
            if (!matched) return false;
        }
        return true;
    }

    void updateFilter()
    {
        const auto oldTerms = filterTerms;
        filterTerms.clear();
        for (const auto &term : search->text().splitRef(' '))
        {
            if (auto cleaned = term.strip())
            {
                filterTerms.insert(cleaned.lower());
            }
        }
        if (oldTerms != filterTerms && shownData)
        {
            shownData->refilter();
            shownData->stableSort([] (const ui::Item &a, const ui::Item &b)
            {
                return a.label().compare(b.label()) < 0;
            });
            updateStatusText();
        }
    }

    GuiWidget *makeItemWidget(const ui::Item &item, const GuiWidget *parent) override
    {
        if (parent == category)
        {
            auto *toggle = new ToggleWidget(ToggleWidget::WithoutIndicator);
            toggle->audienceForStateChange() += [toggle]() {
                toggle->setColorTheme(toggle->isActive() ? Inverted : Normal);
            };
            if (item.label() == ALL_CATEGORIES())
            {
                toggle->setToggleState(ToggleWidget::Active);
            }
            return toggle;
        }
        return new HomeItemWidget(HomeItemWidget::NonAnimatedHeight |
                                  HomeItemWidget::WithoutIcon);
    }

    void updateItemWidget(GuiWidget &widget, const ui::Item &item) override
    {
        if (widget.parentGuiWidget() == category)
        {
            auto &catButton = widget.as<ButtonWidget>();
            catButton.setText(item.label());
            catButton.setSizePolicy(ui::Expand, ui::Expand);
            catButton.margins().set("dialog.gap");
        }
        else
        {
            auto &w = widget.as<HomeItemWidget>();
            w.useColorTheme(Normal, Inverted);
            w.label().margins().set("unit");
            w.label().setText(item.label().fileName('.'));
            //label.setSizePolicy(ui::Expand, ui::Expand);
            //label.margins().set("unit");
            //label.set(Background());
        }
    }

    void updateSelectedRepository()
    {
        if (repo->isValidSelection())
        {
            const auto &selItem = repo->selectedItem();
            App::config().set(VAR_RESOURCE_BROWSER_REPOSITORY(), selItem.label());
            connect(selItem.data().asText());
        }
    }

    void connect(const String& address)
    {
        refreshProgress->setOpacity(1, 0.5);
        repo->disable();

        // Disconnecting may involve waiting for an operation to finish first, so
        // we'll do it async.
        tasks.async([this]() {
            disconnect();
            return Variant{};
        },
        [this, address](const Variant &) {
            RFRelay::get().addRepository(address, "/remote" / WebRequest::hostNameFromUri(address));
            connectedRepository = address;
        });
    }

    void remoteRepositoryStatusChanged(const String &repository, RFRelay::Status status) override
    {
        if (repository == connectedRepository)
        {
            repo->enable();
            refreshProgress->setOpacity(0, 0.5);

            if (status == RFRelay::Connected)
            {
                populateAsync();
            }
            else
            {
                disconnect();
            }
        }
    }

    void disconnect()
    {
        populating.reset();
        if (connectedRepository)
        {
            DE_GUARD(linkBusy);
            mountPath.clear();
            connectedRepository.clear();
            RFRelay::get().removeRepository(connectedRepository);
        }
    }

    filesys::Link &link()
    {
        auto *link = RFRelay::get().repository(connectedRepository);
        DE_ASSERT(link);
        return *link;
    }

    void populateAsync()
    {
        // If there is a previous task, it will finish but completion will not be called.
        populating.reset(new AsyncScope);

        *populating += async([this] ()
        {
            DE_GUARD(linkBusy);
            // All packages from the remote repository are inserted to the data model.
            std::shared_ptr<ui::ListData> pkgs(new ui::ListData);
            link().forPackageIds([&pkgs] (const String &id)
            {
                pkgs->append(new ui::Item(ui::Item::DefaultSemantics, id));
                return LoopContinue;
            });
            return pkgs;
        },
        [this] (std::shared_ptr<ui::ListData> pkgs)
        {
            setData(pkgs);
        });
    }

    void setData(const std::shared_ptr<ui::ListData>& newData)
    {
        DE_ASSERT_IN_MAIN_THREAD();
        //qDebug() << "got new data with" << newData->size() << "items";

        nameList->useDefaultItems();
        shownData.reset(new ui::FilteredData(*newData));
        shownData->setFilter([this] (const ui::Item &i) { return filterItem(i); });
        data = newData;
        shownData->sort();
        nameList->setItems(*shownData);

        categoryData.clear();
        categoryData.append(new ui::Item(ui::Item::ShownAsButton, ALL_CATEGORIES()));
        StringList tags = link().categoryTags();
        tags.sort();
        for (const String &category : tags)
        {
            categoryData.append(new ui::Item(ui::Item::ShownAsButton, category));
        }

        updateStatusText();

        repo->enable();
        refreshProgress->setOpacity(0, 0.5);
    }

    void updateStatusText()
    {
        statusText->setText(Stringf("showing %zu out of %zu mods", shownData->size(), data->size()));
    }
};

RepositoryBrowserDialog::RepositoryBrowserDialog()
    : DialogWidget("repository-browser", WithHeading)
    , d(new Impl(this))
{
    heading().setText("Install Mods");
    heading().setStyleImage("package.icon", heading().fontId());

    AutoRef<Rule> nameListWidth(new ConstantRule(2*175));
    AutoRef<Rule> descriptionWidth(new ConstantRule(2*525));
    AutoRef<Rule> listHeight(new ConstantRule(2*325));

    auto &acRule = area().contentRule();

    auto *searchLabel = LabelWidget::newWithText("Search:", &area());
    auto *repoLabel   = LabelWidget::newWithText("Repository:", &area());

    {
        SequentialLayout layout(acRule.left(),
                                acRule.top(),
                                ui::Right);
        layout << *searchLabel << *d->search << *repoLabel << *d->repo;

        d->search->rule().setInput(Rule::Width,
                                   acRule.width() -
                                   searchLabel->rule().width() -
                                   repoLabel->rule().width() -
                                   d->repo->rule().width());
    }

    d->category->rule()
            .setInput(Rule::Left, acRule.left())
            .setInput(Rule::Top, d->search->rule().bottom());

    d->statusText->rule()
            .setInput(Rule::Right, acRule.right())
            .setInput(Rule::Top, d->category->rule().top())
            .setInput(Rule::Height, d->category->rule().height());

    d->nameList->rule()
            .setSize(nameListWidth, listHeight)
            .setInput(Rule::Left, acRule.left())
            .setInput(Rule::Top, d->category->rule().bottom());

    d->description->rule()
            .setSize(descriptionWidth, listHeight)
            .setInput(Rule::Left, d->nameList->rule().right())
            .setInput(Rule::Top, d->nameList->rule().top());

    area().setContentSize(nameListWidth + descriptionWidth,
                          d->search->rule().height() + d->category->rule().height() +
                          listHeight);

    d->refreshProgress->rule()
            .setInput(Rule::Right, rule().right() - area().margins().right())
            .setInput(Rule::Top,   rule().top() + area().margins().top());

    buttons() << new DialogButtonItem(Default | Accept, "Close")
              << new DialogButtonItem(Action  | Id1,    "Download & Install")
              << new DialogButtonItem(Action  | Id2,    "Try in...");

    // Actions are unavaiable until something is selected.
    buttonWidget(Id1)->disable();
    buttonWidget(Id2)->disable();

    extraButtonsMenu().margins().setLeft(area().margins().left() + nameListWidth);
}

void RepositoryBrowserDialog::finish(int result)
{
    DialogWidget::finish(result);
    d->disconnect();
}
