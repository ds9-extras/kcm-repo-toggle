/*
  Copyright (C) 2016 Rohan Garg <rohan@kde.org>

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

#include "SuiteInfo.h"

#include <QDirIterator>
#include <QDebug>

#define PYTHON_APT_TEMPLATE_DIR "/usr/share/python-apt/templates/"

SuiteInfo::SuiteInfo()
{
  QDir dir(PYTHON_APT_TEMPLATE_DIR);
  if (!dir.exists()) {
    return;
  }

  QStringList nameFilters;
  nameFilters << "*.info";
  dir.setNameFilters(nameFilters);
  dir.setFilter(QDir::Files | QDir::NoDotAndDotDot);

  for (const auto &entry : dir.entryList()) {
    readFile(PYTHON_APT_TEMPLATE_DIR + entry);
  }
}

void SuiteInfo::readFile(QString filePath)
{
    QString suite;
    QString description;

    QFile file(filePath);
    file.open(QIODevice::ReadOnly | QIODevice::Text);
    QString line;
    QStringList comps;
    while (!file.atEnd()) {
        line = file.readLine();

        if (line.startsWith(QChar('#'))) {
            // Comment line
            continue;
        }

        comps = line.split(QChar(':'));

        if (comps.size() != 2) {
            // Invalid line.
            continue;
        }

        QString key = comps.at(0);
        QString value = comps.at(1).trimmed();
        if (key == "Suite") {
          suite = value;
        }

        if (key == "Description") {
          description = value;
        }
        suiteHash[description] = suite;
  }
}
