/*
 * Copyright(C) 2014-2015, Krisztián Kende <nemh@freemail.hu>
 *
 * This file is part of Systemback.
 *
 * Systemback is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * Systemback is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Systemback. If not, see <http://www.gnu.org/licenses/>.
 */

#include "sbscheduler.hpp"
#include <QCoreApplication>
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
        sb::error("\n\n " % tr("The process is disabled for this user.") % "\n\n");
        break;
    case 3:
        sb::error("\n\n " % tr("Root privileges are required.") % "\n\n");
        break;
    case 4:
        sb::error("\n\n " % tr("This system is a Live.") % "\n\n");
        break;
    case 5:
        sb::error("\n\n " % tr("Already running.") % "\n\n");
        break;
    case 6:
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
    else if(sb::schdlr[1] != "false" && (sb::schdlr[1] == "everyone" || sb::right(sb::schdlr[1], -1).split(',').contains(qApp->arguments().value(1))))
    {
        rv = 2;
        goto error;
    }

    if(getuid() + getgid() > 0)
    {
        rv = 3;
        goto error;
    }

    if(sb::isfile("/cdrom/casper/filesystem.squashfs") || sb::isfile("/lib/live/mount/medium/live/filesystem.squashfs"))
    {
        rv = 4;
        goto error;
    }

    if(! sb::lock(sb::Schdlrlock))
    {
        rv = 5;
        goto error;
    }

    if(daemon(0, 0) == -1)
    {
        rv = 6;
        goto error;
    }

    sb::delay(100);
    if(! sb::lock(sb::Schdlrlock)) goto error;
    QStr pfile(sb::isdir("/run") ? "/run/sbscheduler.pid" : "/var/run/sbscheduler.pid");
    if(! sb::crtfile(pfile, QBA::number(qApp->applicationPid()))) goto error;
    QDateTime pflmd(QFileInfo(pfile).lastModified());
    sleep(300);

    forever
    {
        if(! sb::isfile(pfile) || (pflmd != QFileInfo(pfile).lastModified() && sb::fload(pfile) != QBA::number(qApp->applicationPid())))
        {
            sb::unlock(sb::Schdlrlock);
            sb::exec("sbscheduler " % qApp->arguments().value(1), nullptr, true, true);
            break;
        }

        if(! sb::isfile("/etc/systemback.conf") || cfglmd != QFileInfo("/etc/systemback.conf").lastModified())
        {
            sb::cfgread();
            cfglmd = QFileInfo("/etc/systemback.conf").lastModified();
        }

        if(! sb::isdir(sb::sdir[1]) || ! sb::access(sb::sdir[1], sb::Write)) goto next;

        if(! sb::isfile(sb::sdir[1] % "/.sbschedule"))
        {
            sb::crtfile(sb::sdir[1] % "/.sbschedule");
            goto next;
        }

        if(sb::schdle[0] == sb::True)
        {
            if(QFileInfo(sb::sdir[1] % "/.sbschedule").lastModified().secsTo(QDateTime::currentDateTime()) / 60 < sb::schdle[1] * 1440 + sb::schdle[2] * 60 + sb::schdle[3]) goto next;
            if(! sb::lock(sb::Sblock)) goto next;

            if(! sb::lock(sb::Dpkglock))
            {
                sb::unlock(sb::Sblock);
                goto next;
            }

            if(sb::schdle[5] == sb::True || ! sb::execsrch("systemback"))
                newrestorepoint();
            else
            {
                QStr xauth("/tmp/sbXauthority-" % sb::rndstr()), usrhm(qgetenv("HOME"));

                if((qEnvironmentVariableIsSet("XAUTHORITY") && QFile(qgetenv("XAUTHORITY")).copy(xauth)) || (sb::isfile("/home/" % qApp->arguments().value(1) % "/.Xauthority") && QFile("/home/" % qApp->arguments().value(1) % "/.Xauthority").copy(xauth)) || (sb::isfile(usrhm % "/.Xauthority") && QFile(usrhm % "/.Xauthority").copy(xauth)))
                {
                    sb::exec("systemback schedule", "XAUTHORITY=" % xauth);
                    QFile::remove(xauth);
                }
            }

            sb::unlock(sb::Sblock);
            sb::unlock(sb::Dpkglock);
            sleep(50);
        }
        else
            sleep(1790);

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
        if(! sb::pnames[a].isEmpty() && (a == 9 || a > 2 ? sb::pnumber < a + 2 : sb::pnumber == 3) && ! (QFile::rename(sb::sdir[1] % (a < 9 ? QStr("/S0" % QBA::number(a + 1)) : "/S10") % '_' % sb::pnames[a], sb::sdir[1] % "/.DELETED_" % sb::pnames[a]) && sb::remove(sb::sdir[1] % "/.DELETED_" % sb::pnames[a]))) return;

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

    for(uchar a(0) ; a < 9 && sb::isdir(sb::sdir[1] % "/S0" % QBA::number(a + 1) % '_' % sb::pnames[a]) ; ++a)
        if(! QFile::rename(sb::sdir[1] % "/S0" % QBA::number(a + 1) % '_' % sb::pnames[a], sb::sdir[1] % (a < 8 ? "/S0" : "/S") % QBA::number(a + 2) % '_' % sb::pnames[a])) return;

    if(! QFile::rename(sb::sdir[1] % "/.S00_" % dtime, sb::sdir[1] % "/S01_" % dtime)) return;
end:
    sb::crtfile(sb::sdir[1] % "/.sbschedule");
    sb::fssync();
    sb::crtfile("/proc/sys/vm/drop_caches", "3");
}
