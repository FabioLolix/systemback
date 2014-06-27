/********************************************************************

 Copyright(C) 2014, Krisztián Kende <nemh@freemail.hu>

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program. If not, see <http://www.gnu.org/licenses/>.

********************************************************************/

#include "../libsystemback/sblib.hpp"
#include <QCoreApplication>
#include <QStringBuilder>
#include <QTranslator>
#include <QLocale>

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    QTranslator trnsltr;

    if(trnsltr.load("/usr/share/systemback/lang/systemback_" % QLocale::system().name()))
    {
        a.installTranslator(&trnsltr);
        sb::trn[0] = QTranslator::tr("An error occurred while upgrading the system!");
        sb::trn[1] = QTranslator::tr("Restart upgrade ...");
    }

    sb::supgrade();
}
