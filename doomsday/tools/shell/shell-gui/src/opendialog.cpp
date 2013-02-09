/** @file opendialog.cpp  Dialog for opening server connection.
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

#include "opendialog.h"
#include "guishellapp.h"
#include <de/libdeng2.h>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QComboBox>
#include <QFont>
#include <QLayoutItem>
#include <QSettings>

using namespace de;
using namespace de::shell;

static int const MAX_HISTORY_SIZE = 10;

DENG2_PIMPL(OpenDialog)
{
    QComboBox *address;
    QLabel *localCount;
    int firstFoundIdx;
    QStringList history;
    bool edited;

    Instance(Public &i) : Private(i), edited(false)
    {
        // Restore the historical entries.
        QSettings st;
        history = st.value("OpenDialog.history", QStringList() << "localhost").toStringList();

        self.setWindowTitle(tr("Open Connection"));

        QVBoxLayout *mainLayout = new QVBoxLayout;
        self.setLayout(mainLayout);

        QFormLayout *form = new QFormLayout;
        mainLayout->addLayout(form);

        // Combobox with addresses and local servers.
        address = new QComboBox;
        address->setEditable(true);
        address->setMinimumWidth(300);
        address->setInsertPolicy(QComboBox::NoInsert);

        // Insert old user-entered addresses into the box.
        address->addItems(history);
        address->insertSeparator(address->count());
        firstFoundIdx = address->count();

        form->addRow(tr("&Address:"), address);
        QLayoutItem* item = form->itemAt(0, QFormLayout::LabelRole);
        item->setAlignment(Qt::AlignBottom);

        localCount = new QLabel;
        form->addRow(new QWidget, localCount);
        QObject::connect(&GuiShellApp::app().serverFinder(), SIGNAL(updated()),
                         &self, SLOT(updateLocalList()));

        // Buttons.
        QDialogButtonBox *bbox = new QDialogButtonBox;
        mainLayout->addWidget(bbox);
        QPushButton* yes = bbox->addButton(tr("&Connect"), QDialogButtonBox::YesRole);
        QPushButton* no = bbox->addButton(tr("&Cancel"), QDialogButtonBox::RejectRole);
        QObject::connect(yes, SIGNAL(clicked()), &self, SLOT(accept()));
        QObject::connect(no, SIGNAL(clicked()), &self, SLOT(reject()));
        yes->setDefault(true);
    }

    /**
     * Determines if a host is currently listed in the address combo box.
     */
    bool isListed(Address const &host) const
    {
        for(int i = firstFoundIdx; i < address->count(); ++i)
        {
            Q_ASSERT(address->itemData(i).canConvert<Address>());
            if(host == address->itemData(i).value<Address>())
                return true;
        }
        return false;
    }
};

OpenDialog::OpenDialog(QWidget *parent)
    : QDialog(parent), d(new Instance(*this))
{
    updateLocalList(true /* autoselect first found server */);

    connect(d->address, SIGNAL(editTextChanged(QString)), this, SLOT(textEdited(QString)));
    connect(this, SIGNAL(accepted()), this, SLOT(saveState()));
}

OpenDialog::~OpenDialog()
{
    delete d;
}

QString OpenDialog::address() const
{
    int sel = d->address->currentIndex();
    if(d->address->itemData(sel).canConvert<Address>())
    {
        return d->address->itemData(sel).value<Address>().asText();
    }

    // User-entered item.
    QString text = d->address->currentText();

    // Omit parentheses at the end.
    int pos = text.indexOf('(');
    if(pos > 0)
    {
        text = text.left(pos - 1);
    }

    text = text.trimmed();
    return text;
}

void OpenDialog::updateLocalList(bool autoselect)
{
    ServerFinder &finder = GuiShellApp::app().serverFinder();
    bool selected = false;

    if(finder.foundServers().isEmpty())
    {
        // Nothing found.
        d->localCount->setText(tr("<small>No local servers found.</small>"));
    }
    else
    {
        d->localCount->setText(tr("<small>Found %1 local server%2.</small>")
                               .arg(finder.foundServers().size())
                               .arg(finder.foundServers().size() != 1? "s" : ""));

        // Update the list of servers.
        foreach(Address const &sv, finder.foundServers())
        {
            String label = sv.asText() + String(" (%1; %2/%3)")
                    .arg(finder.name(sv).left(20))
                    .arg(finder.playerCount(sv))
                    .arg(finder.maxPlayers(sv));

            if(!d->isListed(sv))
            {
                d->address->addItem(label, QVariant::fromValue(sv));

                // Autoselect the first one?
                if(autoselect && !selected)
                {
                    d->address->setCurrentIndex(d->address->count() - 1);
                    selected = true;
                }
            }
        }
    }

    // Remove servers no longer present.
    for(int i = d->firstFoundIdx; i < d->address->count(); )
    {
        Q_ASSERT(d->address->itemData(i).canConvert<Address>());

        Address sv = d->address->itemData(i).value<Address>();
        if(!finder.foundServers().contains(sv))
        {
            d->address->removeItem(i);
            continue;
        }

        ++i;
    }
}

void OpenDialog::saveState()
{
    if(d->edited)
    {
        String text = d->address->itemText(0);
        d->history.removeAll(text);
        d->history.prepend(text);

        // Make sure there aren't too many entries.
        while(d->history.size() > MAX_HISTORY_SIZE)
        {
            d->history.removeLast();
        }
    }

    QSettings st;
    st.setValue("OpenDialog.history", d->history);
}

void OpenDialog::textEdited(QString text)
{
    if(!d->edited)
    {
        d->edited = true;
        d->address->insertItem(0, text);
        d->address->setCurrentIndex(0);
    }
    else
    {
        d->address->setItemText(0, text);
    }
}
