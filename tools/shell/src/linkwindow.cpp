/** @file linkwindow.cpp  Window for a server link.
 *
 * @authors Copyright © 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "linkwindow.h"
#include "statuswidget.h"
#include "guishellapp.h"
#include "optionspage.h"
#include "preferences.h"

#include <de/commandwidget.h>
#include <de/eventloop.h>
#include <de/garbage.h>
#include <de/inputdialog.h>
#include <de/keyactions.h>
#include <de/logbuffer.h>
#include <de/logwidget.h>
#include <de/messagedialog.h>
#include <de/nativefile.h>
#include <de/popupmenuwidget.h>
#include <de/sequentiallayout.h>
#include <de/styledlogsinkformatter.h>
#include <de/ui/subwidgetitem.h>
#include <de/tabwidget.h>
#include <de/timer.h>
#include <de/term/commandlinewidget.h>
#include <de/term/logwidget.h>

//#ifndef MACOSX
//#  define MENU_IN_LINK_WINDOW
//#endif

using namespace de;

//static String statusText(const String &txt)
//{
//#ifdef MACOSX
//    return "<small>" + txt + "</small>";
//#else
//    return txt;
//#endif
//}

DE_STATIC_STRING(PROMPT_ID, "prompt")

struct ServerCommandWidget : public CommandWidget
{
    DE_DEFINE_AUDIENCE(Command, void commandSubmitted(const String &))

    ServerCommandWidget()
        : CommandWidget(PROMPT_ID())
    {}

    bool isAcceptedAsCommand(const String &) override { return true; }
    void executeCommand(const String &cmd) override
    {
        DE_NOTIFY_VAR(Command, i) { i->commandSubmitted(cmd); }
    }
};

DE_PIMPL(LinkWindow)
, DE_OBSERVES(term::CommandLineWidget, Command)
, DE_OBSERVES(ServerFinder, Update)
, DE_OBSERVES(GuiShellApp, LocalServerStop)
, DE_OBSERVES(ServerCommandWidget, Command)
, DE_OBSERVES(OptionsPage, Commands)
{
    GuiRootWidget root;
    LogBuffer serverLogBuffer; // remote log
    network::Link *link = nullptr;
    duint16 waitingForLocalPort = 0;
    Time startedWaitingAt;
    Timer waitTimeout;
    Timer updateTimer;
    String linkName;
    NativePath errorLog;
    GuiWidget *tools = nullptr;
    TabWidget *pageTabs = nullptr;
    GuiWidget *newLocalServerPage = nullptr;
    GuiWidget *consolePage = nullptr;
    List<GuiWidget *> pages;
    StatusWidget *statusPage = nullptr;
    OptionsPage *optionsPage = nullptr;
    StyledLogSinkFormatter logFormatter{LogEntry::Styled | LogEntry::OmitLevel};
    LogWidget *logWidget = nullptr;
    ServerCommandWidget *commandWidget = nullptr;
    LabelWidget *statusMessage = nullptr;
    LabelWidget *gameStatus = nullptr;
    LabelWidget *timeCounter = nullptr;
    LabelWidget *currentHost = nullptr;
    PopupMenuWidget *menu = nullptr;
#ifdef MENU_IN_LINK_WINDOW
//    QAction *disconnectAction;
#endif

    enum Tab { /*NewServer,*/ Status, Options, Console };

    Impl(Public &i)
        : Base(i)
        , root(&i)
    {
        // Configure the log buffer.
        serverLogBuffer.setMaxEntryCount(50); // buffered here rather than appBuffer
        serverLogBuffer.setAutoFlushInterval(100_ms);

        waitTimeout.setSingleShot(false);
        waitTimeout.setInterval(1.0_s);

        updateTimer.setSingleShot(false);
        updateTimer.setInterval(1.0_s);
        updateTimer.audienceForTrigger() += [this](){ updateWhenConnected(); };
    }

    ~Impl() override
    {
        serverLogBuffer.removeSink(logWidget->logSink());
        LogBuffer::get().removeSink(logWidget->logSink());     
    }

    ButtonWidget *createToolbarButton(const String &label)
    {
        auto *button = new ButtonWidget;
        button->setText(label);
        button->setTextAlignment(ui::AlignRight);
        button->setOverrideImageSize(Style::get().fonts().font("default").height());
        button->setSizePolicy(ui::Expand, ui::Expand);
        return button;
    }

    void createWidgets()
    {
        auto &style = Style::get();

        LabelWidget *background = &root.addNew<LabelWidget>();
        background->set(GuiWidget::Background(Vec4f(0.15f, 0.15f, 0.15f, 1.0f)));
        background->rule().setRect(root.viewRule());

        // Toolbar + menu bar.
        {
            tools = new GuiWidget;
            root.add(tools);

            pageTabs = new TabWidget;
            pageTabs->setTabFont("heading", "heading");
            tools->add(pageTabs);

            pageTabs->rule().setRect(tools->rule());

            auto &appImages = GuiShellApp::app().imageBank();

            pageTabs->items() //<< new TabItem(appImages.image("create"), "New Server")
                              << new TabItem(appImages.image("toolbar.status"), "Status")
                              << new TabItem(appImages.image("toolbar.options"), "Options")
                              << new TabItem(appImages.image("toolbar.console"), "Console");
            pageTabs->setCurrent(0);
            pageTabs->audienceForTab() += [this]() {
                GuiWidget *page = nullptr;
                switch (pageTabs->current())
                {
                    case Tab::Status:  page = statusPage; break;
                    case Tab::Options: page = optionsPage; break;
                    case Tab::Console: page = consolePage; break;
                    default: break;
                }
                setCurrentPage(page);
            };
            pageTabs->disable();

            tools->rule()
                    .setInput(Rule::Left, root.viewLeft())
                    .setInput(Rule::Right, root.viewRight())
                    .setInput(Rule::Top, root.viewTop())
                    .setInput(Rule::Height, pageTabs->rule().height());
        }

        // Status page.
        {
            statusPage = new StatusWidget;
            root.add(statusPage);
            pages << statusPage;
        }

        // Game options page.
        {
            optionsPage = new OptionsPage;
            root.add(optionsPage);
            pages << optionsPage;

            optionsPage->audienceForCommands() += this;
        }

        // Console page.
        {
            consolePage = new GuiWidget;
            root.add(consolePage);
            pages << consolePage;

            const auto &pageRule = consolePage->rule();

            logWidget = new LogWidget;
            logFormatter.setShowMetadata(true);
            logWidget->setLogFormatter(logFormatter);
            consolePage->add(logWidget);

            commandWidget = new ServerCommandWidget;
            commandWidget->rule()
                    .setInput(Rule::Left, pageRule.left())
                    .setInput(Rule::Right, pageRule.right())
                    .setInput(Rule::Bottom, pageRule.bottom());
            consolePage->add(commandWidget);
            commandWidget->setEmptyContentHint("Enter commands");
            commandWidget->audienceForCommand += this;

            logWidget->rule()
                    .setInput(Rule::Left, pageRule.left())
                    .setInput(Rule::Right, pageRule.right())
                    .setInput(Rule::Top, pageRule.top())
                    .setInput(Rule::Bottom, commandWidget->rule().top());

            LogBuffer::get().addSink(logWidget->logSink()); // local log entries
            serverLogBuffer.addSink(logWidget->logSink());
        }

        root.moveToTop(*pageTabs);

        // Page for quickly starting a new local server.
        {
            newLocalServerPage = new GuiWidget;
            root.add(newLocalServerPage);
            pages << newLocalServerPage;

            auto *newButton = new ButtonWidget("newserverbutton");
            newLocalServerPage->add(newButton);
            newButton->setSizePolicy(ui::Expand, ui::Expand);
            newButton->setText("New Local Server...");
            newButton->rule().setCentered(newLocalServerPage->rule());

            newButton->audienceForPress() += []() { GuiShellApp::app().startLocalServer(); };
        }

        auto *statusBar = new GuiWidget;
        statusBar->set(GuiWidget::Background(Style::get().colors().colorf("background")));

        // Status bar.
        {
            menu = &root.addNew<PopupMenuWidget>();
            menu->items()
                << new ui::ActionItem("Connect...", []() { GuiShellApp::app().connectToServer(); })
                << new ui::ActionItem("Disconnect", [this]() { self().closeConnection(); })
                << new ui::Item(ui::Item::Separator)
                << new ui::ActionItem("New Local Server...", []() { GuiShellApp::app().startLocalServer(); })
                << new ui::ActionItem("Stop Server", [this]() { self().stopServer(); })
                << new ui::SubwidgetItem("Local Servers", ui::Left, []() -> PopupWidget * {
                       auto *pop = new PopupMenuWidget;
                       pop->menu().setItems(GuiShellApp::app().localServerMenuItems());
                       return pop;
                   })
                << new ui::Item(ui::Item::Separator)
                << new ui::ActionItem("Preferences...", []() { GuiShellApp::app().showPreferences(); })
                << new ui::ActionItem("Help...", [](){ GuiShellApp::app().showHelp(); })
                << new ui::ActionItem("About Doomsday Shell", [](){ GuiShellApp::app().aboutShell(); })
                << new ui::ActionItem("Quit", []() { GuiShellApp::app().quit(0); });
            auto *menuButton = &root.addNew<PopupButtonWidget>();
            menuButton->setSizePolicy(ui::Expand, ui::Expand);
            menuButton->setText("Menu");
            menuButton->setPopup(*menu, ui::Up);
            menuButton->setFont("small");
            // menuButton->setStyleImage("", menuButton->fontId());

            root.add(statusBar);

            statusMessage = new LabelWidget;
            gameStatus    = new LabelWidget;
            timeCounter   = new LabelWidget;
            currentHost   = new LabelWidget;

            statusMessage->setText(Stringf("Doomsday Shell %s", SHELL_VERSION));

            AutoRef<Rule> statusHeight{style.fonts().font("small").height() +
                                      statusMessage->margins().height()};

            timeCounter->setFont("monospace");
            timeCounter->setText("0:00:00");
            timeCounter->margins().setTop(style.rules().rule("gap") +
                                          style.fonts().font("small").ascent() -
                                          style.fonts().font("monospace").ascent());
//            timeCounter->set(GuiWidget::Background(Vec4f(1, 0, 0, 1)));

            //statusMessage->setText("Status message");
            statusMessage->setAlignment(ui::AlignLeft);
//            statusMessage->set(GuiWidget::Background(Vec4f(0, 0, 1, 1)));
            //gameStatus->setText("");
            gameStatus->setOpacity(0.6f);
            //currentHost->setText("localhost");

            SequentialLayout layout(menuButton->rule().left(), statusBar->rule().top(), ui::Left);

            for (auto *label : {timeCounter, currentHost, gameStatus, statusMessage})
            {
                if (label != statusMessage) label->setSizePolicy(ui::Expand, ui::Fixed);
                if (label != timeCounter) label->setFont("small");
                label->rule().setInput(Rule::Height, statusHeight);
                statusBar->add(label);
                layout << *label;
            }
            statusMessage->rule()
                .setInput(Rule::Left, statusBar->rule().left());

            statusBar->rule()
                    .setInput(Rule::Left, root.viewLeft())
                    .setInput(Rule::Right, root.viewRight())
                    .setInput(Rule::Bottom, root.viewBottom())
                    .setInput(Rule::Height, statusHeight);

            menuButton->rule()
                .setInput(Rule::Right, root.viewRight())
                .setInput(Rule::Bottom, root.viewBottom());
        }

        for (auto *page : pages)
        {
            page->set(GuiWidget::Background());
            page->rule()
                    .setRect(root.viewRule())
                    .setInput(Rule::Top, tools->rule().bottom())
                    .setInput(Rule::Bottom, statusBar->rule().top());
        }

        setCurrentPage(newLocalServerPage);

        // Keyboard shortcuts.
        {
            auto *keys = new KeyActions;
            keys->add(KeyEvent::press(',', KeyEvent::Command),
                      []() { GuiShellApp::app().showPreferences(); });
            keys->add(KeyEvent::press('n', KeyEvent::Command),
                      []() { GuiShellApp::app().startLocalServer(); });
            keys->add(KeyEvent::press('o', KeyEvent::Command),
                      []() { GuiShellApp::app().connectToServer(); });
            keys->add(KeyEvent::press('d', KeyEvent::Command),
                      [this]() { self().closeConnection(); });
            keys->add(KeyEvent::press('w', KeyEvent::Command),
                      [this]() { self().windowAboutToClose(); });
            keys->add(KeyEvent::press('1', KeyEvent::Command), [this]() { self().switchToStatus(); });
            keys->add(KeyEvent::press('2', KeyEvent::Command), [this]() { self().switchToOptions(); });
            keys->add(KeyEvent::press('3', KeyEvent::Command), [this]() { self().switchToConsole(); });
            root.add(keys);
        }
    }

    void commandSubmitted(const String &command) override
    {
        self().sendCommandToServer(command);
    }

    void commandsSubmitted(const StringList &commands) override
    {
        self().sendCommandsToServer(commands);
    }

    void setCurrentPage(GuiWidget *page)
    {
        for (size_t i = 0; i < pages.size(); ++i)
        {
            pages[i]->show(pages[i] == page);
            pages[i]->enable(pages[i] == page);
        }

        // Focus on the appropriate widget.
        if (page == newLocalServerPage)
        {
            root.setFocus(root.guiFind("newserverbutton"));
        }
        else if (page == consolePage)
        {
            root.setFocus(root.guiFind(PROMPT_ID()));
        }
    }

    void updateStyle()
    {
        if (self().isConnected())
        {
//            console->root().canvas().setBackgroundColor(Qt::white);
//            console->root().canvas().setForegroundColor(Qt::black);
        }
        else
        {
//            console->root().canvas().setBackgroundColor(QColor(192, 192, 192));
//            console->root().canvas().setForegroundColor(QColor(64, 64, 64));
        }
    }

    void updateCurrentHost()
    {
        String txt;
        if (link)
        {
            if (self().isConnected() && !link->address().isNull())
            {
                txt = Stringf(_E(b) "%s" _E(.) ":%u",
                              link->address().isLocal() ? "localhost"
                                                        : link->address().hostName().c_str(),
                              link->address().port());
            }
            else if (self().isConnected() && link->address().isNull())
            {
                txt = "Looking up host...";
            }
        }
        currentHost->setText(txt);
    }

    void disconnected()
    {
        self().setTitle("Disconnected");
//        console->root().setOverlaidMessage(tr("Disconnected"));
        statusMessage->setText("");
//        stopAction->setDisabled(true);
//#ifdef MENU_IN_LINK_WINDOW
//        disconnectAction->setDisabled(true);
//#endif

        gameStatus->setText("");
        statusPage->linkDisconnected();
        updateCurrentHost();
        updateStyle();

        pageTabs->disable();
        setCurrentPage(newLocalServerPage);

//        checkCurrentTab(false);
    }

    String readErrorLogContents() const
    {
        try
        {
            std::unique_ptr<const NativeFile> file(NativeFile::newStandalone(errorLog));
            Block text;
            *file >> text;
            return String::fromUtf8(text);
        }
        catch (const Error &er)
        {
            LOG_WARNING("%s") << er.asText();
            return {};
        }
    }

    bool checkForErrors()
    {
        return !readErrorLogContents().isEmpty();
    }

    void showErrorLog()
    {
        const String text = readErrorLogContents();
        if (text)
        {
            debug("Error log from server:%s", text.c_str());
            // Show a message box.
            auto *dlg = new MessageDialog;
            dlg->setDeleteAfterDismissed(true);
            dlg->title().setText("Server Error");
            dlg->title().setStyleImage("alert");
            dlg->message().setText("Failed to start the server. Error log contents:\n\n" + text);
            dlg->buttons() << new DialogButtonItem(DialogWidget::Accept | DialogWidget::Default);
            dlg->exec(self().root());
        }
    }

    void updateWhenConnected()
    {
        if (self().isConnected())
        {
            TimeSpan elapsed = link->connectedAt().since();
            String time = Stringf("%d:%02d:%02d", int(elapsed.asHours()),
                                  int(elapsed.asMinutes()) % 60,
                                  int(elapsed) % 60);
            timeCounter->setText(time);
        }
    }

    void updateStatusBarWithGameState(Record &rec)
    {
        String gameMode = rec["mode"].value().asText();
        String mapId    = rec["mapId"].value().asText();
        String rules    = rec["rules"].value().asText();

        String msg; // = gameMode;
        //if (!mapId.isEmpty()) msg += " " + mapId;
        //if (!rules.isEmpty()) msg += " (" + rules + ")";
        msg = mapId;
        msg += " ";
        msg += rules;

        gameStatus->setText(msg);
    }

    void commandEntered(const String &command) override
    {
        self().sendCommandToServer(command);
    }

    void foundServersUpdated() override
    {
        checkFoundServers();
    }

    void checkFoundServers()
    {
        if (!waitingForLocalPort) return;

        const auto &finder = GuiShellApp::app().serverFinder();
        for (const auto &addr : finder.foundServers())
        {
            if (addr.isLocal() && addr.port() == waitingForLocalPort)
            {
                // This is the one!
                const Address dest = addr;
                Loop::timer(100_ms, [this, dest]() { self().openConnection(new network::Link(dest)); });
                waitingForLocalPort = 0;
            }
        }
    }

    void localServerStopped(int port) override
    {
        if (waitingForLocalPort == port)
        {
            waitingForLocalPort = 0;
            if (!errorLog.isEmpty())
            {
                if (checkForErrors())
                {
                    showErrorLog();
                }
            }
            self().closeConnection();
        }
    }
};

LinkWindow::LinkWindow(const String &id)
    : BaseWindow(id)
    , d(new Impl(*this))
{
    d->createWidgets();

    audienceForResize() += [this]()
    {
        // Resize the root.
        const Size size = pixelSize();
        LOG_MSG("Window resized to %s pixels") << size.asText();

        d->root.setViewSize(size);
    };

    GuiShellApp &app = GuiShellApp::app();

    setIcon(app.imageBank().image("logo"));


#if 0
    d->stopAction = new QAction(tr("S&top"), this);
    connect(d->stopAction, SIGNAL(triggered()), app, SLOT(stopServer()));

#ifdef MENU_IN_LINK_WINDOW
    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(tr("&Settings..."), app, SLOT(showPreferences()));
    fileMenu->addAction(tr("E&xit"), app, SLOT(quit()), QKeySequence(tr("Ctrl+Q")));

    // Menus are window-specific on non-Mac platforms.
    QMenu *menu = menuBar()->addMenu(tr("&Connection"));
    menu->addAction(tr("C&onnect..."), app, SLOT(connectToServer()),
                    QKeySequence(tr("Ctrl+O", "Connection|Connect")));
    d->disconnectAction = menu->addAction(tr("&Disconnect"), this, SLOT(closeConnection()),
                                          QKeySequence(tr("Ctrl+D", "Connection|Disconnect")));
    d->disconnectAction->setDisabled(true);

    QMenu *svMenu = menuBar()->addMenu(tr("&Server"));
    svMenu->addAction(tr("&New Local Server..."), app, SLOT(startLocalServer()),
                      QKeySequence(tr("Ctrl+N", "Server|New Local Server")));
    svMenu->addAction(d->stopAction);
    svMenu->addSeparator();
    svMenu->addMenu(app->localServersMenu());

    connect(svMenu, SIGNAL(aboutToShow()), app, SLOT(updateLocalServerMenu()));

    QMenu *helpMenu = app->makeHelpMenu();
    menuBar()->addMenu(helpMenu);
    helpMenu->addSeparator();
    helpMenu->addAction(tr("About Doomsday Shell"), app, SLOT(aboutShell()));
#endif

    d->stack = new QStackedWidget;
#endif

    // Status page.
//    d->status = new StatusWidget;
//    d->stack->addWidget(d->status);

    // Game options page.

    // Console page.
//    d->console = new ConsolePage;
//    d->stack->addWidget(d->console);
//    d->logBuffer.addSink(d->console->log().logSink());
//    d->console->cli().audienceForCommand() += d;

    d->updateStyle();

//    d->stack->setCurrentIndex(0); // status

//    setCentralWidget(d->stack);

#if 0
    // Status bar.
#ifdef MACOSX
    QFont statusFont(font());
    statusFont.setPointSize(font().pointSize() * 4 / 5);
    statusBar()->setFont(statusFont);
#endif
    d->gameStatus = new QLabel;
    d->gameStatus->setContentsMargins(6, 0, 6, 0);
    d->currentHost = new QLabel;
    d->currentHost->setContentsMargins(6, 0, 6, 0);
    d->timeCounter = new QLabel(statusText("0:00:00"));
    d->timeCounter->setContentsMargins(6, 0, 0, 0);
    statusBar()->addPermanentWidget(d->gameStatus);
    statusBar()->addPermanentWidget(d->currentHost);
    statusBar()->addPermanentWidget(d->timeCounter);

    d->tools = addToolBar(tr("View"));
    d->tools->setMovable(false);
    d->tools->setFloatable(false);

    d->statusButton = d->addToolButton(tr("Status"), QIcon(imageResourcePath(":/images/toolbar_status.png")));
    d->statusButton->setShortcut(QKeySequence(tr("Ctrl+1")));
    connect(d->statusButton, SIGNAL(pressed()), this, SLOT(switchToStatus()));
    d->statusButton->setChecked(true);

#ifdef DE_DEBUG
    QIcon icon(imageResourcePath(":/images/toolbar_placeholder.png"));

    QToolButton *btn = d->addToolButton(tr("Frags"), icon);
    btn->setDisabled(true);

    btn = d->addToolButton(tr("Chat"), icon);
    btn->setDisabled(true);
#endif

    d->optionsButton = d->addToolButton(tr("Options"), QIcon(imageResourcePath(":/images/toolbar_options.png")));
    d->optionsButton->setShortcut(QKeySequence(tr("Ctrl+2")));
    connect(d->optionsButton, SIGNAL(pressed()), this, SLOT(switchToOptions()));

    d->consoleButton = d->addToolButton(tr("Console"), QIcon(imageResourcePath(":/images/toolbar_console.png")));
    d->consoleButton->setShortcut(QKeySequence(tr("Ctrl+3")));
    connect(d->consoleButton, SIGNAL(pressed()), this, SLOT(switchToConsole()));

    // Initial state for the window.
    resize(QSize(640, 480));

    d->console->root().setOverlaidMessage(tr("Disconnected"));
    d->stopAction->setDisabled(true);
#endif

    // Observer local servers.
    GuiShellApp::app().serverFinder().audienceForUpdate() += d;
    GuiShellApp::app().audienceForLocalServerStop() += d;
    d->waitTimeout.audienceForTrigger() += [this]() { d->checkFoundServers(); };
    d->waitTimeout.start();

    setTitle("Disconnected");
}

GuiRootWidget &LinkWindow::root()
{
    return d->root;
}

Vec2f LinkWindow::windowContentSize() const
{
    // Current root widget size.
    return d->root.viewRule().size();
}

void LinkWindow::drawWindowContent()
{
    auto &gls = GLState::current();
    const Size size = pixelSize();

    gls.target().clear(GLFramebuffer::ColorDepth);
    gls.setViewport(Rectangleui(0, 0, size.x, size.y));

    d->root.draw();
}

void LinkWindow::setTitle(const String &title)
{
    BaseWindow::setTitle(title + " - Doomsday Shell");
}

bool LinkWindow::isConnected() const
{
    return d->link && d->link->status() != network::Link::Disconnected;
}

#if 0
void LinkWindow::changeEvent(QEvent *ev)
{
    if (ev->type() == QEvent::ActivationChange)
    {
        if (isActiveWindow())
        {
            // Log local messages here.
            LogBuffer::get().addSink(d->console->log().logSink());
        }
        else
        {
            LogBuffer::get().removeSink(d->console->log().logSink());
        }
    }
}

void LinkWindow::closeEvent(QCloseEvent *event)
{
    /*
    if (isConnected())
    {
        if (QMessageBox::question(
                    this,
                    tr("Close Connection?"),
                    tr("Connection is still open. Do you want to close the window regardless?"),
                    QMessageBox::Close | QMessageBox::Cancel) == QMessageBox::Cancel)
        {
            event->ignore();
            return;
        }
    }
    */

    closeConnection();
    event->accept();

    emit closed(this);

    QMainWindow::closeEvent(event);
}
#endif

void LinkWindow::waitForLocalConnection(duint16 localPort,
                                        const NativePath &errorLogPath,
                                        const String &name)
{
    closeConnection();

    d->serverLogBuffer.flush();
//    d->console->log().clear();

    d->waitingForLocalPort = localPort;
    d->startedWaitingAt = Time();
    d->errorLog = errorLogPath;

    d->linkName = Stringf("%s - Local Server %d", name.c_str(), localPort);
    setTitle(d->linkName);

//    d->console->root().setOverlaidMessage(tr("Waiting for local server..."));
    d->statusMessage->setText("Waiting for local server...");
//    d->checkCurrentTab(true);
}

void LinkWindow::openConnection(network::Link *link, const String &name)
{
    closeConnection();

    d->serverLogBuffer.flush();
//    d->console->log().clear();

    d->link = link;

    d->link->audienceForAddressResolved() += [this]() { addressResolved(); };
    d->link->audienceForConnected()       += [this]() { connected(); };
    d->link->audienceForPacketsReady()    += [this]() { handleIncomingPackets(); };
    d->link->audienceForDisconnected()    += [this]() { disconnected(); };

    if (!name.isEmpty())
    {
        d->linkName = name;
        setTitle(d->linkName);
    }
//    d->console->root().setOverlaidMessage(tr("Looking up host..."));
    d->statusMessage->setText("Looking up host...");

    d->link->connectLink();
    d->statusPage->linkConnected(d->link);
//    d->checkCurrentTab(true);
    d->updateStyle();
}

void LinkWindow::openConnection(const String &address)
{
    debug("Opening connection to %s", address.c_str());

    // Keep trying to connect to 30 seconds.
//    const String addr{address};
    openConnection(new network::Link(address, 30.0_s), address);
}

void LinkWindow::closeConnection()
{
    d->waitingForLocalPort = 0;
    d->errorLog.clear();

    if (d->link)
    {
        debug("Closing existing connection to %s", d->link->address().asText().c_str());

//        // Get rid of the old connection.
        d->link->audienceForPacketsReady() += [this](){ handleIncomingPackets(); };
        d->link->audienceForDisconnected() += [this](){ disconnected(); };

        delete d->link;
        d->link = nullptr;

//        emit linkClosed(this);
    }

    d->disconnected();
}

void LinkWindow::switchToStatus()
{
    if (isConnected())
    {
        d->pageTabs->setCurrent(Impl::Tab::Status);
        d->setCurrentPage(d->statusPage);
    }

//    d->optionsButton->setChecked(false);
//    d->consoleButton->setChecked(false);
//    d->stack->setCurrentWidget(d->link? d->status : d->newLocalServerPage);
}

void LinkWindow::switchToOptions()
{
    if (isConnected())
    {
        d->pageTabs->setCurrent(Impl::Tab::Options);
    }

//    d->statusButton->setChecked(false);
//    d->consoleButton->setChecked(false);
//    d->stack->setCurrentWidget(d->options);
}

void LinkWindow::switchToConsole()
{
    if (isConnected())
    {
        d->pageTabs->setCurrent(Impl::Tab::Console);
    }

//    d->statusButton->setChecked(false);
//    d->optionsButton->setChecked(false);
//    d->stack->setCurrentWidget(d->console);
//    d->console->root().setFocus();
}

void LinkWindow::handleIncomingPackets()
{
    using namespace network;

    glActivate();

    while (isConnected())
    {
        std::unique_ptr<Packet> packet(d->link->nextPacket());
        if (!packet) break;

        //qDebug() << "Packet:" << packet->type();

        using Protocol = network::Protocol;

        // Process packet contents.
        Protocol &protocol = d->link->protocol();
        switch (protocol.recognize(packet.get()))
        {
            case Protocol::PasswordChallenge: askForPassword(); break;

            case Protocol::LogEntries:
            {
                // Add the entries into the local log buffer.
                LogEntryPacket *pkt = static_cast<LogEntryPacket *>(packet.get());
                for (const LogEntry *e : pkt->entries())
                {
                    d->serverLogBuffer.add(new LogEntry(*e, LogEntry::Remote));
                }
                // Flush immediately so we don't have to wait for the autoflush
                // to occur a bit later.
                d->serverLogBuffer.flush();
                break;
            }

            case Protocol::ConsoleLexicon:
                // Terms for auto-completion.
                d->commandWidget->setLexicon(protocol.lexicon(*packet));
                break;

            case Protocol::GameState:
            {
                Record &     rec      = static_cast<RecordPacket *>(packet.get())->record();
                const String rules    = rec["rules"];
                String       gameType = rules.containsWord("dm") ? "Deathmatch"
                                      : rules.containsWord("dm2") ? "Deathmatch II" : "Co-op";

                d->statusPage->setGameState(rec["mode"].value().asText(),
                                            gameType,
                                            rec["mapId"].value().asText(),
                                            rec["mapTitle"].value().asText());

                d->updateStatusBarWithGameState(rec);
                d->optionsPage->updateWithGameState(rec);
                break;
            }
            case Protocol::MapOutline:
                d->statusPage->setMapOutline(*static_cast<MapOutlinePacket *>(packet.get()));
                break;

            case Protocol::PlayerInfo:
                d->statusPage->setPlayerInfo(*static_cast<PlayerInfoPacket *>(packet.get()));
                break;

            default: break;
        }
    }
}

void LinkWindow::sendCommandToServer(const String &command)
{
    if (isConnected())
    {
        // Echo the command locally.
        LogEntry *e = new LogEntry(LogEntry::Generic | LogEntry::Note, "", 0, ">",
                                   LogEntry::Args() << LogEntry::Arg::newFromPool(command));
        d->serverLogBuffer.add(e);

        std::unique_ptr<RecordPacket> packet(d->link->protocol().newCommand(command));
        *d->link << *packet;
    }
}

void LinkWindow::sendCommandsToServer(const StringList &commands)
{
    for (const String &c : commands)
    {
        sendCommandToServer(c);
    }
}

void LinkWindow::addressResolved()
{
//    d->console->root().setOverlaidMessage(tr("Connecting..."));
    d->statusMessage->setText("Connecting...");
    d->updateCurrentHost();
    d->updateStyle();
}

void LinkWindow::connected()
{
    // Once successfully connected, we don't want to show error log any more.
    d->errorLog = "";

    if (d->linkName.isEmpty()) d->linkName = d->link->address().asText();
    setTitle(d->linkName);
    d->updateCurrentHost();
//    d->console->root().setOverlaidMessage("");
    d->statusPage->linkConnected(d->link);
    d->statusMessage->setText("");

    d->updateWhenConnected();
    d->updateTimer.start();
//    d->stopAction->setEnabled(true);
//#ifdef MENU_IN_LINK_WINDOW
//    d->disconnectAction->setEnabled(true);
//#endif
//    d->checkCurrentTab(true);
    d->pageTabs->enable();
    switchToStatus();
//    emit linkOpened(this);
}

void LinkWindow::disconnected()
{
    if (!d->link) return;

    d->updateTimer.stop();

    // The link was disconnected.
    d->link->audienceForPacketsReady() += [this](){ handleIncomingPackets(); };

    trash(d->link);
    d->link = nullptr;

    d->disconnected();

//    emit linkClosed(this);
}

void LinkWindow::askForPassword()
{
    auto *dlg = new InputDialog;
    dlg->setDeleteAfterDismissed(true);
    dlg->title().setText("Password Required");
    dlg->message().setText("Enter the server shell login password:");
    if (dlg->exec(root()))
    {
        if (isConnected())
        {
            *d->link << d->link->protocol().passwordResponse(dlg->editor().text());
        }
    }
    else
    {
        /*EventLoop::callback([this]() { */ closeConnection(); //});
    }
}

void LinkWindow::windowAboutToClose()
{
    BaseWindow::windowAboutToClose();

    if (!isConnected())
    {
        close();
        return;
    }

    auto *dlg = new MessageDialog;
    dlg->setDeleteAfterDismissed(true);
    dlg->title().setText("Closing?");
    dlg->message().setText("You are still connected to a server. Do you want to keep the server "
                           "running after disconnecting?");
    dlg->buttons()
        << new DialogButtonItem(DialogWidget::Accept | DialogWidget::Default,
                                "Keep Running")
        << new DialogButtonItem(DialogWidget::Action, "Stop Server",
                                [dlg]() { dlg->accept(2); })
        << new DialogButtonItem(DialogWidget::Reject, "Cancel");

    if (int result = dlg->exec(root()))
    {
        if (result == 2)
        {
            sendCommandToServer("quit");
        }
        closeConnection();
        close();
    }
}

void LinkWindow::stopServer()
{
    if (isConnected())
    {
        auto *dlg = new MessageDialog;
        dlg->setDeleteAfterDismissed(true);
        dlg->title().setText("Stop Server?");
        dlg->message().setText("Are you sure you want to stop this server?");
        dlg->buttons() << new DialogButtonItem(DialogWidget::Default | DialogWidget::Accept,
                                               "Stop Server")
                       << new DialogButtonItem(DialogWidget::Reject, "Cancel");
        if (dlg->exec(root()))
        {
            sendCommandToServer("quit");
        }
    }
}
