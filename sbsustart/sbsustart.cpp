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

    if(! emsg.isEmpty()) if(sb::exec("zenity --title=Systemback --error --text=\"" % emsg % "\"", NULL, false, true) == 255) sb::exec("kdialog --title=Systemback --error=\"" % emsg % "\"", NULL, false, true);
    qApp->exit(rv);
    return;
};
start:;
    if(! sb::ilike(qApp->arguments().count(), QSIL() << 2 << 3) || ! sb::like(qApp->arguments().value(1), QSL() << "_systemback_" << "_scheduler_"))
    {
        rv = 2;
        goto error;
    }

    QStr usr(getenv("USER")), cmd((qApp->arguments().value(1) == "systemback") ? "systemback authorization " : "sbscheduler ");
    cmd.append(usr);

#if QT_VERSION < QT_VERSION_CHECK(5, 3, 0)
    if(getuid() + getgid() == 0)
#else
    if(getgid() == 0)
#endif
    {
        if(qApp->arguments().value(2) == "gtk+") setenv("QT_STYLE_OVERRIDE", "gtk+", 1);
    }
    else if(cmd.startsWith("systemback"))
    {
        QStr xauth("/tmp/sbXauthority-" % sb::rndstr()), xpath(getenv("XAUTHORITY")), usrhm(getenv("HOME"));

#if QT_VERSION < QT_VERSION_CHECK(5, 3, 0)
        if((getuid() > 0 && setuid(0) == -1) || setgid(0) == -1)
#else
        if(setgid(0) == -1)
#endif
        {
            rv = 4;
            goto error;
        }

        if((xpath.isEmpty() || ! QFile(xpath).copy(xauth)) && (usrhm.isEmpty() || ! isfile(usrhm % "/.Xauthority") || ! QFile(usrhm % "/.Xauthority").copy(xauth)))
        {
            rv = 3;
            goto error;
        }

        if(! clrenv(xauth, "/root", (qApp->arguments().value(2) == "gtk+")))
        {
            sb::remove(xauth);
            rv = 4;
            goto error;
        }
    }
#if QT_VERSION < QT_VERSION_CHECK(5, 3, 0)
    else if((getuid() > 0 && setuid(0) == -1) || setgid(0) == -1 || ! clrenv(getenv("XAUTHORITY"), getenv("HOME"), (qApp->arguments().value(2) == "gtk+")))
#else
    else if(setgid(0) == -1 || ! clrenv(getenv("XAUTHORITY"), getenv("HOME"), (qApp->arguments().value(2) == "gtk+")))
#endif
    {
        rv = 5;
        goto error;
    }

    qApp->exit(sb::exec(cmd));
}

bool sbsustart::clrenv(QStr xpath, QStr usrhm, bool gtk)
{
    QStr dsply(getenv("DISPLAY")), pth(getenv("PATH")), lng(getenv("LANG"));
    if(clearenv() == -1) return false;
    if(! dsply.isEmpty()) setenv("DISPLAY", dsply.toStdString().c_str(), 1);
    setenv("HOME", usrhm.isEmpty() ? "/root" : usrhm.toStdString().c_str(), 1);
    if(! lng.isEmpty()) setenv("LANG", lng.toStdString().c_str(), 1);
    setenv("LOGNAME", "root", 1);
    if(! pth.isEmpty()) setenv("PATH", pth.toStdString().c_str(), 1);
    setenv("SHELL", "/bin/bash", 1);
    setenv("USER", "root", 1);
    if(! xpath.isEmpty()) setenv("XAUTHORITY", xpath.toStdString().c_str(), 1);
    if(gtk) setenv("QT_STYLE_OVERRIDE", "gtk+", 1);
    return true;
}
