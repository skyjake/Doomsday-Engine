/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "de/LegacyCore"

#include <QCoreApplication>
#include <QThread>
#include <QDebug>

using namespace de;

/**
 * @internal Private thread for running the legacy core's event loop.
 */
class CoreEventThread : public QThread
{
public:
    CoreEventThread(QCoreApplication& app) : _app(app) {}

    void run() {
        qDebug() << "CoreEventThread: Event loop started.";

        // Start the event loop.
        _app.exec();

        qDebug() << "CoreEventThread: Event loop stopped.";
    }

private:
    QCoreApplication& _app;
};

/**
 * @internal Private instance data for LegacyCore.
 */
struct LegacyCore::Instance
{
    QCoreApplication* app;
    CoreEventThread* thread;

    Instance() : app(0), thread(0) {}
    ~Instance() {
        delete thread;
        delete app;
    }
};

LegacyCore::LegacyCore(int& argc, char** argv)
{
    d = new Instance;

    // Construct a new core application (must have one for the event loop).
    d->app = new QCoreApplication(argc, argv);

    // The event loop thread.
    d->thread = new CoreEventThread(*d->app);
}

LegacyCore::~LegacyCore()
{
    stop();

    delete d;
}

void LegacyCore::start()
{
    qDebug() << "LegacyCore: Starting...";

    // Run the separate Qt event loop thread. In the future this will be
    // replaced by the application's main Qt event loop, where deng2 will
    // hook into.
    d->thread->start();
}

void LegacyCore::stop()
{
    qDebug() << "LegacyCore: Stopping...";

    // Stop the separate Qt event loop thread.
    d->thread->quit();
    d->thread->wait(5000);

    qDebug() << "LegacyCore: Stopped.";
}
