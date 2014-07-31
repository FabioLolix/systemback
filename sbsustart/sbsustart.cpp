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
#include <QFileInfo>
#include <unistd.h>

void sbsustart::main()
{
    uchar rv;
    goto start;
error:
{
    QStr emsg;

    switch(rv) {
    case 1:
        sb::error("\n " % tr("Missing or too much argument(s).") % "\n\n");
        break;
    case 2:
        emsg = tr("Cannot start Systemback graphical user interface!") % "\n\n" % tr("Unable to connect to X server.");
        break;
    case 3:
        emsg = tr("Cannot start Systemback graphical user interface!") % "\n\n" % tr("Unable to get root permissions.");
        break;
    case 4:
        emsg = tr("Cannot start Systemback scheduler daemon!") % "\n\n" % tr("Unable to get root permissions.");
    }

    if(! emsg.isEmpty()) (sb::exec("which zenity", NULL, true) == 0) ? sb::exec("zenity --title=Systemback --error --text=\"" % emsg % "\"", NULL, false, true) : sb::exec("kdialog --title=Systemback --error=\"" % emsg % "\"", NULL, false, true);
    qApp->exit(rv);
    return;
};
start:;
    if(! sb::ilike(qApp->arguments().count(), QSIL() << 2 << 3))
    {
        rv = 1;
        goto error;
    }

    QStr usr(getenv("USER")), cmd((qApp->arguments().value(1) == "systemback") ? "systemback authorization " : "sbscheduler ");
    cmd.append(usr);

    if(getuid() == 0)
    {
        if(qApp->arguments().value(2) == "gtk+") setenv("QT_STYLE_OVERRIDE", "gtk+", 1);
    }
    else if(cmd.startsWith("systemback"))
    {
        QStr xauth("/tmp/sbXauthority-" % sb::rndstr()), xpath(getenv("XAUTHORITY")), usrhm(getenv("HOME"));

        if(setuid(0) == -1 || setgid(0) == -1)
        {
            rv = 3;
            goto error;
        }

        if((xpath.isEmpty() || ! QFile(xpath).copy(xauth)) && (usrhm.isEmpty() || ! isfile(usrhm % "/.Xauthority") || ! QFile(usrhm % "/.Xauthority").copy(xauth)))
        {
            rv = 2;
            goto error;
        }

        if(! clrenv(xauth, (qApp->arguments().value(2) == "gtk+")))
        {
            sb::remove(xauth);
            rv = 3;
            goto error;
        }
    }
    else if(setuid(0) == -1 || setgid(0) == -1 || ! clrenv(getenv("XAUTHORITY"), (qApp->arguments().value(2) == "gtk+")))
    {
        rv = 4;
        goto error;
    }

    qApp->exit(sb::exec(cmd));
}

bool sbsustart::clrenv(QStr xpath, bool gtk)
{
    QStr dsply(getenv("DISPLAY")), pth(getenv("PATH")), lng(getenv("LANG"));
    if(clearenv() == -1) return false;
    if(! dsply.isEmpty()) setenv("DISPLAY", dsply.toStdString().c_str(), 1);
    setenv("HOME", "/root", 1);
    if(! lng.isEmpty()) setenv("LANG", lng.toStdString().c_str(), 1);
    setenv("LOGNAME", "root", 1);
    if(! pth.isEmpty()) setenv("PATH", pth.toStdString().c_str(), 1);
    setenv("SHELL", "/bin/bash", 1);
    setenv("USER", "root", 1);
    if(! xpath.isEmpty()) setenv("XAUTHORITY", xpath.toStdString().c_str(), 1);
    if(gtk) setenv("QT_STYLE_OVERRIDE", "gtk+", 1);
    return true;
}
