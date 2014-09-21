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

#include "sbsustart.hpp"
#include <QCoreApplication>
#include <QStringBuilder>
#include <QTranslator>
#include <QLocale>
#include <QTimer>
#include <unistd.h>

int main(int argc, char *argv[])
{
    if (getuid() > 0 && QStr(qVersion()).replace(".", nullptr).toShort() >= 530
        && setuid(0) == -1) {
        QStr arg1(argv[1]);

        if (arg1.isEmpty()
            || !sb::like(arg1, { "_systemback_", "_scheduler_" })) {
            sb::error("\n Missing, wrong or too much argument(s).\n\n");
            return 2;
        }

        QStr emsg((arg1 == "systemback")
                      ? "Cannot start Systemback graphical user interface!"
                      : "Cannot start Systemback scheduler daemon!");
        emsg.append("\n\nUnable to get root permissions.");
        sb::exec((sb::execsrch("zenity")
                      ? "zenity --title=Systemback --error --text=\""
                      : "kdialog --title=Systemback --error=\"") % emsg % "\"",
                 nullptr, false, true);
        return 1;
    }

    QCoreApplication a(argc, argv);
    QTranslator trnsltr;
    if (trnsltr.load("/usr/share/systemback/lang/systemback_"
                     % QLocale::system().name()))
        a.installTranslator(&trnsltr);
    sbsustart s;
    QTimer::singleShot(0, &s, SLOT(main()));
    return a.exec();
}
