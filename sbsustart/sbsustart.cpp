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
        emsg = tr("Cannot start Systemback graphical user interface!") % "\n\n" % tr("Unable to connect to X server.");
        break;
    case 4:
        emsg = tr("Cannot start Systemback graphical user interface!") % "\n\n" % tr("Unable to get root permissions.");
        break;
    case 5:
        emsg = tr("Cannot start Systemback scheduler daemon!") % "\n\n" % tr("Unable to get root permissions.");
    }

    if(! emsg.isEmpty()) sb::exec((sb::execsrch("zenity") ? "zenity --title=Systemback --error --text=\"" : "kdialog --title=Systemback --error=\"") % emsg % "\"", nullptr, false, true);
    qApp->exit(rv);
    return;
};
start:;
    if(! sb::ilike(qApp->arguments().count(), {2, 3}) || ! sb::like(qApp->arguments().value(1), {"_systemback_", "_scheduler_"}))
    {
        rv = 2;
        goto error;
    }

    QStr usr(qEnvironmentVariableIsEmpty("USER") ? "root" : qgetenv("USER")), cmd((qApp->arguments().value(1) == "systemback" ? "systemback authorization " : "sbscheduler ") % usr);

    if(getuid() + getgid() > 0)
    {
        if(cmd.startsWith("systemback"))
        {
            QStr xauth("/tmp/sbXauthority-" % sb::rndstr()), usrhm(qgetenv("HOME"));

            if((getuid() > 0 && setuid(0) == -1) || setgid(0) == -1 || ! qgetenv("PATH").startsWith("/usr/lib/systemback"))
            {
                rv = 4;
                goto error;
            }

            if((qEnvironmentVariableIsEmpty("XAUTHORITY") || ! QFile(qgetenv("XAUTHORITY")).copy(xauth)) && (usrhm.isEmpty() || ! sb::isfile(usrhm % "/.Xauthority") || ! QFile(usrhm % "/.Xauthority").copy(xauth)))
            {
                rv = 3;
                goto error;
            }

            if(! clrenv(xauth, "/root"))
            {
                sb::remove(xauth);
                rv = 4;
                goto error;
            }
        }
        else if((getuid() > 0 && setuid(0) == -1) || setgid(0) == -1 || ! clrenv(qgetenv("XAUTHORITY"), qgetenv("HOME")))
        {
            rv = 5;
            goto error;
        }
    }

    if(qApp->arguments().value(2) == "gtk+") qputenv("QT_STYLE_OVERRIDE", "gtk+");
    qApp->exit(sb::exec(cmd));
}

bool sbsustart::clrenv(cQStr &xpath, cQStr &usrhm)
{
    QSL envvs(QProcess::systemEnvironment());

    for(cQStr &cvar : envvs)
    {
        QStr var(sb::left(cvar, sb::instr(cvar, "=") - 1));
        if(! sb::like(var, {"_DISPLAY*", "_PATH*", "_LANG*"}) && ! qunsetenv(var.toStdString().c_str())) return false;
    }

    if(! qputenv("USER", "root") || ! qputenv("HOME", usrhm.isEmpty() ? "/root" : usrhm.toLocal8Bit()) || ! qputenv("LOGNAME", "root") || ! qputenv("SHELL", "/bin/bash") || (! xpath.isEmpty() && ! qputenv("XAUTHORITY", xpath.toLocal8Bit()))) return false;
    return true;
}
