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
#include <curses.h>
#include <stdio.h>
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

    // Curses state:
    WINDOW *rootWin;
    de::Vector2i rootSize;
    QTimer *inputPollTimer;

    CursesTextCanvas *canvas;

    Instance(CursesApp &a) : self(a), canvas(0)
    {
        initCurses();
    }

    ~Instance()
    {
        shutdownCurses();
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

        inputPollTimer = new QTimer(&self);
        QObject::connect(inputPollTimer, SIGNAL(timeout()), &self, SLOT(pollForInput()));
        inputPollTimer->start(1000 / 20);

        // Create the canvas.
        canvas = new CursesTextCanvas(rootSize, rootWin);
        canvas->show();
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
        delete canvas;
        canvas = 0;

        delete inputPollTimer;
        inputPollTimer = 0;

        delwin(rootWin);
        rootWin = 0;

        endwin();
        refresh();
    }

    void pollForInput()
    {
        if(!rootWin) return;

        int key;
        while((key = wgetch(rootWin)) != ERR)
        {
            if(key == KEY_RESIZE)
            {
                // Terminal has been resized.
                de::Vector2i size = actualTerminalSize();
                resize_term(size.y, size.x);

                emit self.viewResized(size.x, size.y);
            }
            else if(key & KEY_CODE_YES)
            {
                // There is a curses key code.
                switch(key)
                {
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
    }

    void windowWasResized() // called from signal handler
    {
        ungetch(KEY_RESIZE);
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

void CursesApp::windowWasResized() // called from signal handler
{
    d->windowWasResized();
}

void CursesApp::pollForInput()
{
    d->pollForInput();
}
