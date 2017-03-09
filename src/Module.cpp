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
#include <QPushButton>
#include <QProgressBar>

class Module::Private {
public:
    Private(Module* qq)
        : q(qq)
        , backend(new QApt::Backend)
        , progress(0)
    {
        backend->init();
    }
    Module* q;
    QApt::Backend *backend;
    void populateSources();
    void checkCheckStates();

    void saveCompleted(KJob* job);
    void saveJobStatusChanged(KAuth::ExecuteJob* saveJob, KAuth::Action::AuthStatus status);
    void saveJobNewData(const QVariantMap& data);
    QProgressBar* progress;
};

Module::Module(QWidget *parent, const QVariantList &args)
    : KCModule(parent, args)
    , ui(new Ui::Module)
    , d(new Private(this))
{
    KAboutData *aboutData = new KAboutData(QStringLiteral("kcmrepotoggle"),
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
    // We also have no defaults, so get rid of that as well.
    setButtons(buttons() ^ KCModule::Help ^ KCModule::Default);
}

Module::~Module()
{
    delete d;
    delete ui;
}

void Module::Private::populateSources()
{
    // If the first item is a layout, it means we've got a progress bar thing, so get rid of that first
    if(q->ui->verticalLayout->itemAt(0)->layout() != 0) {
        QLayout* grid = q->ui->verticalLayout->itemAt(0)->layout();
        while(grid->count() > 0) {
            QLayoutItem* item = grid->takeAt(0);
            delete item->widget();
            delete item;
        }
    }
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

    // WARNING Hack time, because we might very well end up with an os-release file that isn't quite right...
    // Not keen on adding heuristics, but... if we must, then we must.
    QString realOsName;
    if(QFile::exists("/usr/share/netrunner-desktop-settings")) {
        realOsName = QLatin1String("Netrunner Desktop");
    }

    OSRelease os;
    QStringList paths = QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation);
    for(QString const& path : paths) {
        // One hard-coded channel, and one for the distribution we're actually running on
        QStringList channelTypes = QStringList() << QString("%1/release-channels/channels/general-use").arg(path) << QString("%1/release-channels/channels/%2").arg(path).arg(os.name);
        if(realOsName != os.name) {
            channelTypes << QString("%1/release-channels/channels/%2").arg(path).arg(realOsName);
        }
        for(QString const &channelsPath : channelTypes) {
            QDir channels(channelsPath);
            if(channels.exists()) {
                for(QString const& channel : channels.entryList(QDir::Files)) {
                    QCheckBox *checkbox = new QCheckBox(channel);
                    q->ui->verticalLayout->addWidget(checkbox);
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
                        QLabel* descLabel = new QLabel(lines[1].mid(1).trimmed());
                        descLabel->setWordWrap(true);
                        q->ui->verticalLayout->addWidget(descLabel);
                    }
                    checkbox->setProperty("currentState", Qt::Unchecked);
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
                    checkbox->setProperty("channelFile", QString("%1/%2").arg(channelsPath).arg(channel));
                    q->connect(checkbox, &QCheckBox::stateChanged, [this](){checkCheckStates();});
                }
            }
        }
    }
    q->ui->verticalLayout->addStretch();
    QCheckBox* refreshCheck = new QCheckBox(i18nc("Title label for the checkbox which causes an apt cache refresh when applying the new settings", "Refresh cache when applying"));
    refreshCheck->setToolTip(i18nc("Tooltip text for the checkbox which causes an apt cache refresh when applying the new settings", "Enabling this will cause the local cache of the software channels to be downloaded when applying new settings. This may cause a large amount of data to be downloaded (up to a couple of hundred MiB)."));
    refreshCheck->setCheckState(Qt::Checked);
    q->ui->verticalLayout->addWidget(refreshCheck);
}

// Let's make sure we don't let people apply/ok when there's nothing to save
void Module::Private::checkCheckStates()
{
    bool changed = false;
    for(int i = 0; i < q->ui->verticalLayout->count() - 1; ++i) {
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
    for(int i = 0; i < ui->verticalLayout->count() - 1; ++i) {
        // Because we will flush all of these later, disable them now and we won't have
        // to do any further magic to make the progress bar enabled
        QWidget* widget = ui->verticalLayout->itemAt(i)->widget();
        if(widget) {
            widget->setEnabled(false);
            QCheckBox* checkbox = qobject_cast<QCheckBox*>(widget);
            if(checkbox) {
                if(checkbox->checkState() != checkbox->property("currentState").value<Qt::CheckState>()) {
                    helperargs[checkbox->property("channelFile").value<QString>()] = checkbox->checkState();
                }
            }
        }
    }

    if(!helperargs.isEmpty()) {
        QCheckBox* refreshCheck = qobject_cast<QCheckBox*>(ui->verticalLayout->itemAt(ui->verticalLayout->count() - 1)->widget());
        refreshCheck->setEnabled(false);
        helperargs["/refreshCache"] = refreshCheck->checkState();

        KAuth::Action action = authAction();
        action.setHelperId("org.kde.kcontrol.kcmrepotoggle");
        action.setArguments(helperargs);
        action.setTimeout(1000 * 60 * 20); // twenty minutes... TODO Add a way to cancel this action, because that's a long time...

        KAuth::ExecuteJob* saveJob = action.execute();
        if(refreshCheck->checkState() == Qt::Checked) {
            QGridLayout* grid = new QGridLayout();
            QLabel* pleaseWait = new QLabel(i18nc("Label above the progress bar when updating the sources after changing the settings", "Please wait while updating your package cache..."), this);
            grid->addWidget(pleaseWait, 0, 0);
            d->progress = new QProgressBar(this);
            ui->verticalLayout->insertWidget(0, d->progress);
            grid->addWidget(d->progress, 1, 0);
            QPushButton* cancelRefresh = new QPushButton(this);
            cancelRefresh->setIcon(QIcon::fromTheme("dialog-cancel"));
            grid->addWidget(cancelRefresh, 0, 1, 2, 1);
            ui->verticalLayout->insertLayout(0, grid);
            connect(cancelRefresh, &QPushButton::clicked, saveJob, [saveJob](){
                saveJob->kill(KJob::EmitResult);
                // The above /should/ really do the trick... and because what's below doesn't work because HelperProxy is
                // not exported, cancelling does not currently work. Fun-fun. Will need to fix that in KAuth itself.
                // KAuth::HelperProxy::stopAction("org.kde.kcontrol.kcmrepotoggle", "org.kde.kcontrol.kcmrepotoggle.save");
                // Thoughts on this: Really would be nice to have the normal job kill code work, because
                // having this API different for no appreciable reason seems very strange to me. There is
                // nothing inherent in KAuth that means this has to somehow be different normal KJob code.
            });
        }

        connect(saveJob, &KJob::result, this, [this](KJob* job){ d->saveCompleted(job); });
        connect(saveJob, &KAuth::ExecuteJob::statusChanged, this, [saveJob, this](KAuth::Action::AuthStatus status){ d->saveJobStatusChanged(saveJob, status); });
        connect(saveJob, &KAuth::ExecuteJob::newData, this, [this](QVariantMap data){ d->saveJobNewData(data); });
        connect(saveJob, SIGNAL(percent(KJob*, unsigned long)), this, SLOT(percentChanged(KJob*, unsigned long)));

        saveJob->start();
    }
}

void Module::percentChanged(KJob* /*job*/, unsigned long percent)
{
    if(!d->progress) {
        return;
    }
    if(percent > 100) {
        d->progress->setMaximum(0);
    }
    else {
        d->progress->setMaximum(100);
        d->progress->setValue(percent);
    }
}

void Module::Private::saveJobStatusChanged(KAuth::ExecuteJob* saveJob, KAuth::Action::AuthStatus status)
{
    switch(status) {
    case KAuth::Action::DeniedStatus:
    case KAuth::Action::ErrorStatus:
    case KAuth::Action::InvalidStatus:
        KMessageBox::error(q, i18nc("The text used to describe an error which occurred when attempting to save the software channels setup to the user", "An error occurred when attempting to save the changes. The reported error was: %1").arg(saveJob->errorText()), i18nc("Title for the error dialog when saving changes to the software channels setup", "Error saving software channels"));
        break;
    case KAuth::Action::UserCancelledStatus:
    case KAuth::Action::AuthRequiredStatus:
    case KAuth::Action::AuthorizedStatus:
    default:
        break;
    }
}

void Module::Private::saveCompleted(KJob* /*job*/)
{
    // If we are not OKing here, only applying, this potentially becomes terribly useful, as we
    // may have .lists files with the same name. So, clear things up, so we can get impossible
    // selections disabled
    populateSources();
    progress = 0;
}

void Module::Private::saveJobNewData(const QVariantMap& /*data*/)
{
//     qDebug() << "New data" << data;
    // This would be nice at a later point in time... for now, we're not using this.
}

void Module::defaults()
{
    d->populateSources();
}
