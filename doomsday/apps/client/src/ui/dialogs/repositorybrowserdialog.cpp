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

#include <de/Async>
#include <de/ChoiceWidget>
#include <de/Config>
#include <de/DictionaryValue>
#include <de/DocumentWidget>
#include <de/FileSystem>
#include <de/Folder>
#include <de/LineEditWidget>
#include <de/ui/FilteredData>
#include <de/RemoteFeedRelay>

using namespace de;

DENG_GUI_PIMPL(RepositoryBrowserDialog)
, DENG2_OBSERVES(filesys::RemoteFeedRelay, Status)
, public ChildWidgetOrganizer::IWidgetFactory
, public AsyncScope
{
    using RFRelay = filesys::RemoteFeedRelay;

    ui::ListData categoryData;
    std::shared_ptr<ui::ListData> data;
    std::unique_ptr<ui::FilteredData> shownData;
    std::unique_ptr<AsyncScope> populating;
    Lockable linkBusy;

    ChoiceWidget *repo;
    LineEditWidget *search;
    MenuWidget *category;
    MenuWidget *files;
    DocumentWidget *description;
    String connectedRepository;
    String mountPath;

    Impl(Public *i) : Base(i)
    {
        auto &area = self().area();

        area.add(repo        = new ChoiceWidget);
        area.add(search      = new LineEditWidget);
        area.add(category    = new MenuWidget);
        area.add(files       = new MenuWidget);
        area.add(description = new DocumentWidget);

        // Insert known repositories.
        try
        {
            auto &repos = Config::get().getdt("resource.repositories");
            for (auto i = repos.elements().begin(); i != repos.elements().end(); ++i)
            {
                repo->items()
                        << new ChoiceItem(i->first.value->asText(), i->second->asText());
            }
            repo->setSelected(0);
        }
        catch (Error const &er)
        {
            LOG_MSG("Remote repositories not listed in configuration; "
                    "set Config.resource.repositories: %s")
                    << er.asText();
        }

        category->setItems(categoryData);
        category->setGridSize(0, ui::Expand, 1, ui::Expand, GridLayout::RowFirst);
        category->organizer().setWidgetFactory(*this);

        files->setVirtualizationEnabled(true, style().fonts().font("default").height().valuei() +
                                        rule("unit").valuei() * 2);
        files->setBehavior(ChildVisibilityClipping);
        files->organizer().setWidgetFactory(*this);
        files->rule()
                .setInput(Rule::Height, Const(200))
                .setInput(Rule::Width, Const(500));

        QObject::connect(repo, &ChoiceWidget::selectionChangedByUser, [this] (uint)
        {
            updateSelectedRepository();
        });

        RFRelay::get().audienceForStatus() += this;

        updateSelectedRepository();
    }

    ~Impl()
    {
        if (populating) populating->waitForFinished();
        disconnect();
    }

    bool filterItem(ui::Item const &item) const
    {
        return true;
    }

    GuiWidget *makeItemWidget(ui::Item const &item, GuiWidget const *parent) override
    {
        if (parent == category)
        {
            return new ButtonWidget;
        }
        return new LabelWidget;
    }

    void updateItemWidget(GuiWidget &widget, ui::Item const &item) override
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
            auto &label = widget.as<LabelWidget>();
            label.setText(item.label());
            label.setSizePolicy(ui::Expand, ui::Expand);
            label.margins().set("unit");
            label.set(Background());
        }
    }

    void updateSelectedRepository()
    {
        qDebug() << "connecting to" << repo->selectedItem().data().toString();

        connect(repo->selectedItem().data().toString());
    }

    void connect(String address)
    {
        repo->disable();

        // Disconnecting may involve waiting for an operation to finish first, so
        // we'll do it async.
        *this += async([this] ()
        {
            disconnect();
            return 0;
        },
        [this, address] (int)
        {
            QUrl const url(address);
            RFRelay::get().addRepository(address, "/remote" / url.host());
            connectedRepository = address;
        });
    }

    void remoteRepositoryStatusChanged(String const &repository, RFRelay::Status status) override
    {
        if (repository == connectedRepository)
        {
            repo->enable();
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
            DENG2_GUARD(linkBusy);
            mountPath.clear();
            connectedRepository.clear();
            RFRelay::get().removeRepository(connectedRepository);
        }
    }

    filesys::Link &link()
    {
        auto *link = RFRelay::get().repository(connectedRepository);
        DENG2_ASSERT(link);
        return *link;
    }

    void populateAsync()
    {
        // If there is a previous task, it will finish but completion will not be called.
        populating.reset(new AsyncScope);

        *populating += async([this] ()
        {
            DENG2_GUARD(linkBusy);
            // All packages from the remote repository are inserted to the data model.
            std::shared_ptr<ui::ListData> pkgs(new ui::ListData);
            link().forPackageIds([&pkgs] (String const &id)
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

    void setData(std::shared_ptr<ui::ListData> newData)
    {
        DENG2_ASSERT_IN_MAIN_THREAD();
        qDebug() << "got new data with" << newData->size() << "items";

        files->useDefaultItems();
        shownData.reset(new ui::FilteredData(*newData));
        shownData->setFilter([this] (ui::Item const &i) { return filterItem(i); });
        data = newData;
        files->setItems(*shownData);

        categoryData.clear();
        categoryData.append(new ui::Item(ui::Item::ShownAsButton, tr("All Categories")));
        StringList tags = link().categoryTags();
        qSort(tags);
        foreach (String category, tags)
        {
            categoryData.append(new ui::Item(ui::Item::ShownAsButton, category));
        }

        repo->enable();
    }
};

RepositoryBrowserDialog::RepositoryBrowserDialog()
    : DialogWidget("repository-browser", WithHeading)
    , d(new Impl(this))
{
    heading().setText(tr("Install Mods"));
    heading().setStyleImage("package.icon", heading().fontId());

    auto *repoLabel   = LabelWidget::newWithText(tr("Repository:"), &area());
    auto *searchLabel = LabelWidget::newWithText(tr("Search:"), &area());

    GridLayout layout(area().contentRule().left(),
                      area().contentRule().top());
    layout.setGridSize(2, 0);
    layout << *repoLabel << *d->repo
           << *searchLabel << *d->search;
    layout.append(*d->category, 2)
           << *d->files << *d->description;

    area().setContentSize(layout);

    buttons() << new DialogButtonItem(Default | Accept, tr("Close"));
}

void RepositoryBrowserDialog::finish(int result)
{
    DialogWidget::finish(result);
    d->disconnect();
}
