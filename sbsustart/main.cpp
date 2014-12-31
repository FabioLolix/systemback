/********************************************************************

 Copyright(C) 2014-2015, Krisztián Kende <nemh@freemail.hu>

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

uint sbsustart::uid(getuid());

int main(int argc, char *argv[])
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 3, 0)
    {
        if(sbsustart::uid > 0 && setuid(0) == -1 && sbsustart::uid != geteuid())
        {
            QStr arg1(argv[1]);

            if(! sb::like(arg1, {"_systemback_", "_scheduler_"}))
            {
                sb::error("\n Missing, wrong or too much argument(s).\n\n");
                return 2;
            }

            QStr emsg("Cannot start Systemback " % QStr(arg1 == "systemback" ? "graphical user interface" : "scheduler daemon") % "!\n\nUnable to get root permissions.");

            if(seteuid(sbsustart::uid) == -1)
                sb::error("\n " % emsg.replace("\n\n", "\n\n ") % "\n\n");
            else
                sb::exec((sb::execsrch("zenity") ? "zenity --title=Systemback --error --text=\"" : "kdialog --title=Systemback --error=\"") % emsg % '\"', nullptr, false, true);

            return 1;
        }
    }
#endif

    QCoreApplication a(argc, argv);
    QTranslator trnsltr;
    sb::cfgread();

    if(sb::lang == "auto")
    {
        if(! QLocale::system().name().startsWith("en")) trnsltr.load(QLocale::system(), "systemback", "_", "/usr/share/systemback/lang");
    }
    else if(! sb::lang.startsWith("en"))
        trnsltr.load("systemback_" % sb::lang, "/usr/share/systemback/lang");

    if(! trnsltr.isEmpty()) a.installTranslator(&trnsltr);
    sbsustart s;
    QTimer::singleShot(0, &s, SLOT(main()));
    return a.exec();
}
