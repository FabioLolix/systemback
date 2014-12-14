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

#include "sbscheduler.hpp"
#include <QCoreApplication>
#include <QStringBuilder>
#include <QDateTime>
#include <QDir>
#include <unistd.h>

void scheduler::main()
{
    uchar rv(255);
    goto start;
error:
    sb::error("\n " % tr("Cannot start Systemback scheduler daemon!"));

    switch(rv) {
    case 1:
        sb::error("\n\n " % tr("Missing, wrong or too much argument(s).") % "\n\n");
        break;
    case 2:
        sb::error("\n\n " % tr("Root privileges are required.") % "\n\n");
        break;
    case 3:
        sb::error("\n\n " % tr("This system is a Live.") % "\n\n");
        break;
    case 4:
        sb::error("\n\n " % tr("Already running.") % "\n\n");
        break;
    case 5:
        sb::error("\n\n " % tr("Unable to demonize.") % "\n\n");
    }

    qApp->exit(rv);
    return;
start:
    if(qApp->arguments().count() != 2)
    {
        rv = 1;
        goto error;
    }

    if(getuid() + getgid() > 0)
    {
        rv = 2;
        goto error;
    }

    if(sb::isfile("/cdrom/casper/filesystem.squashfs") || sb::isfile("/lib/live/mount/medium/live/filesystem.squashfs"))
    {
        rv = 3;
        goto error;
    }

    if(! sb::lock(sb::Schdlrlock))
    {
        rv = 4;
        goto error;
    }

    if(daemon(0, 0) == -1)
    {
        rv = 5;
        goto error;
    }

    sb::delay(100);
    if(! sb::lock(sb::Schdlrlock)) goto error;
    QStr pfile(sb::isdir("/run") ? "/run/sbscheduler.pid" : "/var/run/sbscheduler.pid");
    if(! sb::crtfile(pfile, QStr::number(qApp->applicationPid()))) goto error;
    sleep(300);

    for(;;)
    {
        if(sb::left(sb::fload(pfile), 7) == "restart")
        {
            sb::unlock(sb::Schdlrlock);
            sb::exec("sbscheduler " % qApp->arguments().value(1), nullptr, true, true);
            break;
        }

        sb::cfgread();
        if(! sb::isdir(sb::sdir[1]) || ! sb::access(sb::sdir[1], sb::Write)) goto next;

        if(! sb::isfile(sb::sdir[1] % "/.sbschedule"))
        {
            sb::crtfile(sb::sdir[1] % "/.sbschedule");
            goto next;
        }

        if(sb::schdle[0] == "true")
        {
            if(QFileInfo(sb::sdir[1] % "/.sbschedule").lastModified().secsTo(QDateTime::currentDateTime()) / 60 < sb::schdle[1].toUShort() * 1440 + sb::schdle[2].toUShort() * 60 + sb::schdle[3].toUShort()) goto next;
            if(! sb::lock(sb::Sblock)) goto next;

            if(! sb::lock(sb::Dpkglock))
            {
                sb::unlock(sb::Sblock);
                goto next;
            }

            if(sb::schdle[5] == "true" || ! sb::execsrch("systemback"))
                newrestorepoint();
            else
            {
                QStr xauth("/tmp/sbXauthority-" % sb::rndstr()), usrhm(qgetenv("HOME"));

                if((qEnvironmentVariableIsSet("XAUTHORITY") && QFile(qgetenv("XAUTHORITY")).copy(xauth)) || (sb::isfile("/home/" % qApp->arguments().value(1) % "/.Xauthority") && QFile("/home/" % qApp->arguments().value(1) % "/.Xauthority").copy(xauth)) || (sb::isfile(usrhm % "/.Xauthority") && QFile(usrhm % "/.Xauthority").copy(xauth)))
                {
                    sb::exec("systemback schedule " % sb::schdle[6], "XAUTHORITY=" % xauth);
                    QFile::remove(xauth);
                }
            }

            sb::unlock(sb::Sblock);
            sb::unlock(sb::Dpkglock);
            sleep(50);
        }

    next:
        sleep(10);
    }

    qApp->quit();
}

void scheduler::newrestorepoint()
{
    sb::pupgrade();

    for(cQStr &item : QDir(sb::sdir[1]).entryList(QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot))
        if(sb::like(item, {"_.DELETED_*", "_.S00_*"}) && ! sb::remove(sb::sdir[1] % '/' % item)) return;

    for(uchar a(9) ; a > 1 ; --a)
        if(! sb::pnames[a].isEmpty() && (a < 9 ? (a > 2 ? sb::pnumber < a + 2 : sb::pnumber == 3) : true) && ! (QFile::rename(sb::sdir[1] % (a < 9 ? QStr("/S0" % QStr::number(a + 1)) : "/S10") % '_' % sb::pnames[a], sb::sdir[1] % "/.DELETED_" % sb::pnames[a]) && sb::remove(sb::sdir[1] % "/.DELETED_" % sb::pnames[a]))) return;

    QStr dtime(QDateTime().currentDateTime().toString("yyyy-MM-dd,hh.mm.ss"));

    if(! sb::crtrpoint(sb::sdir[1], ".S00_" % dtime))
    {
        if(sb::dfree(sb::sdir[1]) < 104857600)
        {
            sb::remove(sb::sdir[1] % "/.S00_" % dtime);
            goto end;
        }

        return;
    }

    for(uchar a(0) ; a < 9 && sb::isdir(sb::sdir[1] % "/S0" % QStr::number(a + 1) % '_' % sb::pnames[a]) ; ++a)
        if(! QFile::rename(sb::sdir[1] % "/S0" % QStr::number(a + 1) % '_' % sb::pnames[a], sb::sdir[1] % (a < 8 ? "/S0" : "/S") % QStr::number(a + 2) % '_' % sb::pnames[a])) return;

    if(! QFile::rename(sb::sdir[1] % "/.S00_" % dtime, sb::sdir[1] % "/S01_" % dtime)) return;
end:
    sb::crtfile(sb::sdir[1] % "/.sbschedule");
    sb::fssync();
    sb::crtfile("/proc/sys/vm/drop_caches", "3");
}
