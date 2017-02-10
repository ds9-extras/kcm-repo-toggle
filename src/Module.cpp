/*
  Copyright (C) 2016 Rohan Garg <apachelogger@ubuntu.com>

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

#include <QDebug>
#include <QCheckBox>
#include <QHash>

#include <KAboutData>
#include <KConfig>
#include <KCoreAddons>
#include <QApt/SourcesList>
#include <QApt/Backend>
#include <QApt/Config>
#include <QJsonDocument>
#include <QFile>
#include <QVariantMap>

Module::Module(QWidget *parent, const QVariantList &args) :
    KCModule(parent, args),
    ui(new Ui::Module),
    m_backend(new QApt::Backend)
{
    KAboutData *aboutData = new KAboutData(QStringLiteral("kcm-channel-switch"),
                                           i18nc("@title", "Distribution Channel switcher"),
                                           global_s_versionStringFull,
                                           QStringLiteral(""),
                                           KAboutLicense::LicenseKey::GPL_V3,
                                           i18nc("@info:credit", "Copyright 2016 Rohan Garg"));

    aboutData->addAuthor(i18nc("@info:credit", "Rohan Garg"),
                         i18nc("@info:credit", "Author"),
                         QStringLiteral("rohan@kde.org"));

    setAboutData(aboutData);

    ui->setupUi(this);
    m_backend->init();
    populateDists();


    // We have no help so remove the button from the buttons.
    setButtons(buttons() ^ KCModule::Help ^ KCModule::Default ^ KCModule::Apply);
}

Module::~Module()
{
    delete ui;
}

void Module::populateDists()
{
  QApt::SourcesList list;
  for (auto const &entry : list.entries()) {
    m_distMap[entry.dist()] = entry.isEnabled();
  }
}

void Module::load()
{
  OSRelease os;
  QFile file;
  file.setFileName("/usr/share/release-channels/Netrunner.json");
  file.open(QIODevice::ReadOnly | QIODevice::Text);
  auto val = file.readAll();
  file.close();
  QJsonDocument d = QJsonDocument::fromJson(val);
  auto jsonObject = d.toVariant();
  auto map = jsonObject.value<QVariantMap>();
  auto releaseMap = map[os.name].value<QVariantMap>();
  auto baseURI = releaseMap["BaseURI"].value<QString>();
  auto repos = releaseMap["Repos"].value<QStringList>();
  auto channelsMap = releaseMap["Releases"].value<QVariantList>();
  for (auto const &entry : channelsMap) {
    auto entryMap = entry.value<QVariantMap>();
    auto description = entryMap["Description"].value<QString>();
    auto dist = entryMap["dist"].value<QString>();
    QCheckBox *checkbox = new QCheckBox(description);
    if (m_distMap[dist])
      checkbox->setCheckState(Qt::Checked);
    ui->verticalLayout->addWidget(checkbox);
    checkbox->setProperty("dist", dist);
    checkbox->setProperty("baseURI", baseURI);
    checkbox->setProperty("repos", repos);
    connect(checkbox, &QCheckBox::stateChanged, this, &Module::toggleChannel);
  }
}

void Module::toggleChannel(int state)
{
  QCheckBox *checkbox = qobject_cast<QCheckBox*>(sender());
  auto dist = checkbox->property("dist").value<QString>();
  auto baseURI = checkbox->property("baseURI").value<QString>();
  auto repos = checkbox->property("repos").value<QString>();
  qDebug() << dist << baseURI << repos;
}

void Module::save()
{
}

void Module::defaults()
{
}
