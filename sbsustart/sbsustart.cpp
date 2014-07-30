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

    QStr cmd((qApp->arguments().value(1) == "systemback") ? "systemback authorization " : "/usr/lib/systemback/sbscheduler ");
    cmd.append(getenv("USER"));

    if(getuid() == 0)
        (qApp->arguments().value(2) == "gtk+") ? qApp->exit(sb::exec("su -lc \"env QT_STYLE_OVERRIDE=gtk+ " % cmd % "\"")) : qApp->exit(sb::exec("su -lc \"" % cmd % "\""));
    else if(cmd.startsWith("systemback"))
    {
        QStr xauth("/tmp/sbXauthority-" % sb::rndstr()), env(getenv("XAUTHORITY"));

        if(! (! env.isEmpty() && QFile(env).copy(xauth)) && ! (isfile("/home/" % QStr(getenv("USER")) % "/.Xauthority") && QFile("/home/" % QStr(getenv("USER")) % "/.Xauthority").copy(xauth)))
        {
            rv = 2;
            goto error;
        }

        if(setuid(0) == -1)
        {
            QFile::remove(xauth);
            rv = 3;
            goto error;
        }

        (qApp->arguments().value(2) == "gtk+") ? qApp->exit(sb::exec("su -lc \"env QT_STYLE_OVERRIDE=gtk+ " % cmd % "\"", "XAUTHORITY=" % xauth)) : qApp->exit(sb::exec("su -lc \"" % cmd % "\"", "XAUTHORITY=" % xauth));
    }
    else if(setuid(0) == -1)
    {
        rv = 4;
        goto error;
    }
    else if(qApp->arguments().value(2) == "gtk+")
        qApp->exit(sb::exec("su -lc \"env QT_STYLE_OVERRIDE=gtk+ " % cmd % "\n"));
    else
        qApp->exit(sb::exec("su -lc \"" % cmd % "\""));
}
