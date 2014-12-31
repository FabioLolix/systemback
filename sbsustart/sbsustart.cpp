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
#include <QProcess>
#include <unistd.h>

void sbsustart::main()
{
    uchar rv;
    goto start;
error:
{
    QStr emsg;

    switch(rv) {
    case 2:
        sb::error("\n " % tr("Missing, wrong or too much argument(s).") % "\n\n");
        break;
    case 3:
        emsg = tr("Unable to get root permissions.");
        break;
    case 4:
        emsg = tr("Unable to connect to X server.");
    }

    if(! emsg.isEmpty())
    {
        emsg.prepend((qApp->arguments().value(1) == "systemback" ? tr("Cannot start Systemback graphical user interface!") : tr("Cannot start Systemback scheduler daemon!")) % "\n\n");

#if QT_VERSION < QT_VERSION_CHECK(5, 3, 0)
        if(uid != geteuid() && seteuid(uid) == -1)
            sb::error("\n " % emsg.replace("\n\n", "\n\n ") % "\n\n");
        else
#endif
        sb::exec((sb::execsrch("zenity") ? "zenity --title=Systemback --error --text=\"" : "kdialog --title=Systemback --error=\"") % emsg % '\"', nullptr, false, true);
    }

    qApp->exit(rv);
    return;
}
start:
    if(! sb::ilike(qApp->arguments().count(), {2, 3}) || ! sb::like(qApp->arguments().value(1), {"_systemback_", "_scheduler_"}))
    {
        rv = 2;
        goto error;
    }

    QStr cmd;

    {
        QStr uname, usrhm;

        if(uid == 0)
        {
            uname = "root";
            usrhm = "/root";
        }
        else
        {
            QFile file("/etc/passwd");

            if(file.open(QIODevice::ReadOnly))
                while(! file.atEnd())
                {
                    QStr line(file.readLine().trimmed());

                    if(line.contains("x:" % QStr::number(uid) % ':'))
                    {
                        QSL uslst(line.split(':'));
                        uname = uslst.at(0);
                        usrhm = uslst.at(5);
                        break;
                    }
                }

            if(uname.isEmpty() || usrhm.isEmpty())
            {
                rv = 3;
                goto error;
            }
        }

        bool uidinr(getuid() > 0), gidinr(getgid() > 0);

        if(uidinr || gidinr)
        {
            if((uidinr && setuid(0) == -1) || (gidinr && setgid(0) == -1))
            {
                rv = 3;
                goto error;
            }

            if(qApp->arguments().value(1) == "systemback")
            {
                QStr xauth("/tmp/sbXauthority-" % sb::rndstr());

                if((qEnvironmentVariableIsEmpty("XAUTHORITY") || ! QFile(qgetenv("XAUTHORITY")).copy(xauth)) && (! sb::isfile("/home/" % uname % "/.Xauthority") || ! QFile("/home/" % uname % "/.Xauthority").copy(xauth)) && (! sb::isfile(usrhm % "/.Xauthority") || ! QFile(usrhm % "/.Xauthority").copy(xauth)))
                {
                    rv = 4;
                    goto error;
                }

                if(! clrenv("/root", xauth))
                {
                    sb::remove(xauth);
                    rv = 3;
                    goto error;
                }

                cmd = "systemback authorization " % uname;
            }
            else if(! clrenv(usrhm))
            {
                rv = 3;
                goto error;
            }
            else
                cmd = "sbscheduler " % uname;
        }
        else if(qApp->arguments().value(1) == "systemback")
            cmd = "systemback";
        else
        {
            qputenv("HOME", usrhm.toUtf8());
            cmd = "sbscheduler " % uname;
        }
    }

    if(qApp->arguments().value(2) == "gtk+") qputenv("QT_STYLE_OVERRIDE", "gtk+");
    qApp->exit(sb::exec(cmd));
}

bool sbsustart::clrenv(cQStr &usrhm, cQStr &xpath)
{
    for(cQStr &cvar : QProcess::systemEnvironment())
    {
        QStr var(sb::left(cvar, sb::instr(cvar, "=") - 1));
        if(! sb::like(var, {"_DISPLAY_", "_PATH_", "_LANG_", "_XAUTHORITY_"}) && ! qunsetenv(chr(var))) return false;
    }

    if(! qputenv("USER", "root") || ! qputenv("HOME", usrhm.toUtf8()) || ! qputenv("LOGNAME", "root") || ! qputenv("SHELL", "/bin/bash") || ! (xpath.isEmpty() || qputenv("XAUTHORITY", xpath.toUtf8()))) return false;
    return true;
}
