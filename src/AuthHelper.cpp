/*
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

#include "AuthHelper.h"

#include <KLocalizedString>

#include <QApt/Backend>
#include <QApt/Config>

#include <QDebug>
#include <QFile>

ActionReply Helper::save(const QVariantMap& args)
{
    ActionReply reply;

    QApt::Backend *backend = new QApt::Backend();
    backend->init();
    QString sldDir(backend->config()->findDirectory("Dir::Etc::sourceparts", QLatin1String("/etc/apt/sources.list.d/")));
    for(const QString& key : args.keys()) {
        QString listsFile = QString("%1/%2").arg(sldDir).arg(key.split("/").last());
        bool result = false;
        switch(args.value(key).toInt()) {
        case 0:
            // disable - remove the file
            result = QFile::remove(listsFile);
            if(!result) {
                reply.setType(KAuth::ActionReply::HelperErrorType);
                reply.setErrorDescription(i18nc("Error string used when a software channel could not be removed because the file representing it could not be deleted", "Failed to disable %1 - could not remove the file %2").arg(key).arg(listsFile));
            }
            break;
        case 2:
            // enable - copy the file into its new location
            result = QFile::copy(key, listsFile);
            if(!result) {
                reply.setType(KAuth::ActionReply::HelperErrorType);
                reply.setErrorDescription(i18nc("Error string used when a software channel could not be enabled because the file representing it could not be copied to the apt sources lists directory", "Failed to enable %1 - could not copy it to %2").arg(key).arg(listsFile));
            }
            break;
        case 1:
        default:
            // nothing - this should not really be possible, but switches should handle all inputs, so...
            qWarning() << "Attempted to do nothing with an apt source lists file. This should not be possible." << key;
            reply.setType(KAuth::ActionReply::HelperErrorType);
            reply.setErrorDescription(i18nc("Error string used in the very uncommon case that an unknown configuration was attempted", "Failed to change status of %1 to the unknown middle state - this should not really be possible").arg(key));
            break;
        }
        if(!result) {
            break;
        }
    }

    return reply;
}

KAUTH_HELPER_MAIN("org.kde.kcontrol.kcmrepotoggle", Helper)
