/*
  Copyright (C) 2016 Rohan Garg <apachelogger@ubuntu.com>
  Copyright (C) 2017 Dan Leinir Turthra Jensen <admin@leinir.dk>

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License as
  published by the Free Software Foundation; either version 2 of
  the License or (at your option) version 3 or any later version
  accepted by the membership of KDE e.V. (or its successor approved
  by the membership of KDE e.V.), which shall act as a proxy
  defined in Section 14 of version 3 of the license.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "Module.h"

#include "ui_Module.h"
#include "Version.h"
#include "OSRelease.h"

#include <QApt/Backend>
#include <QApt/Config>

#include <KAboutData>
#include <KMessageBox>
#include <KAuth/KAuthExecuteJob>

#include <QDebug>
#include <QCheckBox>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QVariantMap>

class Module::Private {
public:
    Private(Module* qq)
        : q(qq)
        , backend(new QApt::Backend)
    {
        backend->init();
    }
    Module* q;
    QApt::Backend *backend;
    void populateSources();
    void checkCheckStates();

};

Module::Module(QWidget *parent, const QVariantList &args)
    : KCModule(parent, args)
    , ui(new Ui::Module)
    , d(new Private(this))
{
    KAboutData *aboutData = new KAboutData(QStringLiteral("kcmchannelswitch"),
                                           i18nc("@title", "Software Channels"),
                                           global_s_versionStringFull,
                                           QStringLiteral(""),
                                           KAboutLicense::LicenseKey::GPL_V3,
                                           i18nc("@info:credit", "Copyright 2016 Rohan Garg"));

    aboutData->addAuthor(i18nc("@info:credit", "Rohan Garg"),
                         i18nc("@info:credit", "Author"),
                         QStringLiteral("rohan@kde.org"));
    aboutData->addAuthor(i18nc("@info:credit", "Dan Leinir Turthra Jensen"),
                         i18nc("@info:credit", "Author"),
                         QStringLiteral("admin@leinir.dk"));

    setAboutData(aboutData);

    ui->setupUi(this);
    setNeedsAuthorization(true);

    // We have no help so remove the button from the buttons.
    setButtons(buttons() ^ KCModule::Help ^ KCModule::Default ^ KCModule::Apply);
}

Module::~Module()
{
    delete d;
    delete ui;
}

void Module::Private::populateSources()
{
    // clear existing checkboxes, if there are any...
    while(q->ui->verticalLayout->count() > 0) {
        QLayoutItem* item = q->ui->verticalLayout->takeAt(0);
        delete item->widget();
        delete item;
    }

    QMap<QString, QString> sldEntries;
    QString sldDir(backend->config()->findDirectory("Dir::Etc::sourceparts", QLatin1String("/etc/apt/sources.list.d/")));
    while(sldDir.endsWith("/")) {
        sldDir = sldDir.left(sldDir.length() - 1);
    }
    QDir sld(sldDir);
    if(sld.exists()) {
        for(auto const &entry : sld.entryList(QDir::Files)) {
            QFile file(QString("%1/%2").arg(sldDir).arg(entry));
            if(file.open(QIODevice::ReadOnly)) {
                sldEntries[entry] = file.readAll();
                file.close();
            }
        }
    }

    OSRelease os;
    QStringList paths = QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation);
    for(QString const& path : paths) {
        QStringList channelTypes = QStringList() << QString("%1/release-channels/channels/general-use").arg(path) << QString("%1/release-channels/channels/%2").arg(path).arg(os.name);
        for(QString const &channelsPath : channelTypes) {
            QDir channels(channelsPath);
            if(channels.exists()) {
                for(QString const& channel : channels.entryList(QDir::Files)) {
                    QCheckBox *checkbox = new QCheckBox(channel);
                    QFile file(QString("%1/%2").arg(channelsPath).arg(channel));
                    QString contents;
                    if(file.open(QIODevice::ReadOnly)) {
                        contents = file.readAll();
                        file.close();
                    }
                    QStringList lines = contents.split("\n");
                    // Is the first line a comment? Use that as the title
                    if(lines[0].startsWith("#")) {
                        checkbox->setText(lines[0].mid(1).trimmed());
                    }
                    // How about the second line? That'll be our description
                    if(lines.length() > 1 && lines[1].startsWith("#")) {
                        checkbox->setToolTip(lines[1].mid(1).trimmed());
                    }
                    checkbox->setProperty("currentState", Qt::Unchecked);
                    q->ui->verticalLayout->addWidget(checkbox);
                    // if file exists in /etc/apt/sources.lists.d/...
                    if(sldEntries.contains(channel)) {
                        // and is identical to our file, check box...
                        if(contents == sldEntries[channel]) {
                            checkbox->setCheckState(Qt::Checked);
                            checkbox->setProperty("currentState", Qt::Checked);
                        }
                        // and is different from our file, disable, describe error
                        // nb: this also ensures we can handle two channels with the same .lists filename... just don't
                        // enable the one that would otherwise replace what is already there. Needs saying in the UI somehow.
                        else {
                            checkbox->setEnabled(false);
                            checkbox->setToolTip(i18nc("Checkbox tool tip which shows when the contents differ between the channel's .lists file and the .lists file with the same name in apt's soources.lists.d", "This entry cannot be enabled, as a channel with this filename already exists, but the contents differ."));
                            QLabel *label = new QLabel(QString("The contents of the files %1 and %2/%3 differ - you will have to manually remove %2/%3 to be able to select this channel.").arg(file.fileName()).arg(sldDir).arg(channel)); // this is a placeholder, hence no i18n...
                            label->setWordWrap(true);
                            q->ui->verticalLayout->addWidget(label);
                            // TODO Make it possible to see the difference between the two files, and manually delete the
                            // other (if it's not represented by another channel...) - this will need thorough thought.
                        }
                    }
                    checkbox->setProperty("channelFile", channel);
                    checkbox->setProperty("channelsDir", channelsPath);
                    q->connect(checkbox, &QCheckBox::stateChanged, [this](){checkCheckStates();});
                }
            }
        }
    }
}

// Let's make sure we don't let people apply/ok when there's nothing to save
void Module::Private::checkCheckStates()
{
    bool changed = false;
    for(int i = 0; i < q->ui->verticalLayout->count(); ++i) {
        QCheckBox* checkbox = qobject_cast<QCheckBox*>(q->ui->verticalLayout->itemAt(i)->widget());
        if(checkbox && checkbox->checkState() != checkbox->property("currentState").value<Qt::CheckState>()) {
            changed = true;
            break;
        }
    }
    q->changed(changed);
}

void Module::load()
{
    d->populateSources();
}

void Module::save()
{
    QVariantMap helperargs;
    for(int i = 0; i < ui->verticalLayout->count(); ++i) {
        QCheckBox* checkbox = qobject_cast<QCheckBox*>(ui->verticalLayout->itemAt(i)->widget());
        if(checkbox) {
            if(checkbox->checkState() != checkbox->property("currentState").value<Qt::CheckState>()) {
                auto channelFile = checkbox->property("channelFile").value<QString>();
                auto channelDir = checkbox->property("channelsDir").value<QString>();
                helperargs[QString("%1/%2").arg(channelDir).arg(channelFile)] = checkbox->checkState();
            }
        }
    }

    if(!helperargs.isEmpty()) {
        KAuth::Action action = authAction();
        action.setHelperId("org.kde.kcontrol.kcmchannelswitch");
        action.setArguments(helperargs);

        KAuth::ExecuteJob* job = action.execute();
        if(!job->exec()) {
            KMessageBox::error(this, i18nc("The text used to describe an error which occurred when attempting to save the software channels setup to the user", "An error occurred when attempting to save the changes. The reported error was: %1").arg(job->errorText()), i18nc("Title for the error dialog when saving changes to the software channels setup", "Error saving software channels"));
        }
        // If we are not OKing here, only applying, this potentially becomes terribly useful, as we
        // may have .lists files with the same name. So, clear things up, so we can get impossible
        // selections disabled
        d->populateSources();
    }
}

void Module::defaults()
{
    d->populateSources();
}
