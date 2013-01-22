/** @file cursesapp.cpp Application based on curses for input and output.
 *
 * @authors Copyright © 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include <QTimer>
#include <QProcess>
#include <QTextStream>
#include <QDebug>
#include "cursesapp.h"
#include "cursestextcanvas.h"
#include "textrootwidget.h"
#include <curses.h>
#include <stdio.h>
#include <de/Clock>
#include <de/Animation>
#include <de/Vector>

static void windowResized(int)
{
    Q_ASSERT(qApp);
    static_cast<CursesApp *>(qApp)->windowWasResized();
}

/**
 * Determines the actual current size of the terminal.
 *
 * @return Terminal columns and rows.
 */
static de::Vector2i actualTerminalSize()
{
    QByteArray result;
    FILE *p = popen("stty size", "r");
    forever
    {
        int c = fgetc(p);
        if(c == EOF) break;
        result.append(c);
    }
    pclose(p);

    QTextStream is(result);
    de::Vector2i size;
    is >> size.y >> size.x;
    return size;
}

struct CursesApp::Instance
{
    CursesApp &self;
    de::Clock clock;

    // Curses state:
    WINDOW *rootWin;
    de::Vector2i rootSize;
    QTimer *refreshTimer;

    TextRootWidget *rootWidget;

    Instance(CursesApp &a) : self(a), rootWidget(0)
    {
        de::Animation::setClock(&clock);
        de::Clock::setAppClock(&clock);

        initCurses();

        // Create the canvas.
        rootWidget = new TextRootWidget(new CursesTextCanvas(rootSize, rootWin));
        rootWidget->draw();
    }

    ~Instance()
    {
        delete rootWidget;

        shutdownCurses();

        de::Clock::setAppClock(0);
        de::Animation::setClock(0);
    }

    void initCurses()
    {
        // Initialize curses.
        if(!(rootWin = initscr()))
        {
            qFatal("Failed initializing curses.");
            return;
        }
        initCursesState();

        // Listen for window resizing.
        signal(SIGWINCH, windowResized);

        refreshTimer = new QTimer(&self);
        QObject::connect(refreshTimer, SIGNAL(timeout()), &self, SLOT(refresh()));
        refreshTimer->start(1000 / 20);
    }

    void initCursesState()
    {
        // The current size of the screen.
        getmaxyx(stdscr, rootSize.y, rootSize.x);

        scrollok(rootWin, FALSE);
        wclear(rootWin);

        // Initialize input.
        cbreak();
        noecho();
        nonl();
        nodelay(rootWin, TRUE);
        keypad(rootWin, TRUE);
    }

    void shutdownCurses()
    {
        delete refreshTimer;
        refreshTimer = 0;

        delwin(rootWin);
        rootWin = 0;

        endwin();
        refresh();
    }

    void refresh()
    {
        if(!rootWin) return;

        // Update time.
        clock.setTime(de::Time());

        // Poll for input.
        int key;
        while((key = wgetch(rootWin)) != ERR)
        {
            if(key == KEY_RESIZE)
            {
                // Terminal has been resized.
                de::Vector2i size = actualTerminalSize();

                // Curses needs to resize its buffers.
                wclear(rootWin);
                resize_term(size.y, size.x);

                // The root widget will update the UI.
                rootWidget->setViewSize(size);
                rootWidget->draw();
            }
            else if(key & KEY_CODE_YES)
            {
                //qDebug() << "Got code" << QString("0%1").arg(key, 0, 8).toAscii().constData();

                // There is a curses key code.
                switch(key)
                {
                case KEY_BTAB: // back-tab

                    break;
                default:
                    // This key code is ignored.
                    break;
                }
            }
            else if(key == 0x1b) // Escape
            {
                self.quit();
            }
            else
            {
                qDebug() << "Got key" << QString("0x%1").arg(key, 0, 16).toAscii().constData();
            }
        }

        rootWidget->draw();
    }

    void windowWasResized() // called from signal handler
    {
        ungetch(KEY_RESIZE);
    }

    void update()
    {
        rootWidget->draw();
    }
};

CursesApp::CursesApp(int &argc, char **argv)
    : QCoreApplication(argc, argv), d(new Instance(*this))
{
}

CursesApp::~CursesApp()
{
    delete d;
}

TextRootWidget &CursesApp::rootWidget()
{
    return *d->rootWidget;
}

void CursesApp::windowWasResized() // called from signal handler
{
    d->windowWasResized();
}

void CursesApp::refresh()
{
    d->refresh();
}
