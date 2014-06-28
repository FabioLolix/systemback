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
error:;
    sb::error("\n " % tr("Cannot start Systemback scheduler daemon!"));

    switch(rv) {
    case 1:
        sb::error("\n\n " % tr("Root privileges are required.") % "\n\n");
        break;
    case 2:
        sb::error("\n\n " % tr("This system is a Live.") % "\n\n");
        break;
    case 3:
        sb::error("\n\n " % tr("Already running.") % "\n\n");
        break;
    case 4:
        sb::error("\n\n " % tr("Unable to demonize.") % "\n\n");
    }

    qApp->exit(rv);
    return;
start:;
    if(geteuid() != 0)
    {
        rv = 1;
        goto error;
    }

    if(isfile("/cdrom/casper/filesystem.squashfs") || isfile("/live/image/live/filesystem.squashfs") || isfile("/lib/live/mount/medium/live/filesystem.squashfs"))
    {
        rv = 2;
        goto error;
    }

    if(! sb::lock(sb::Schdlrlock))
    {
        rv = 3;
        goto error;
    }

    if(daemon(0, 0) != 0)
    {
        rv = 4;
        goto error;
    }

    sb::delay(100);
    if(! sb::lock(sb::Schdlrlock)) goto error;
    QStr pfile(isdir("/run") ? "/run/sbscheduler.pid" : "/var/run/sbscheduler.pid");
    if(! sb::crtfile(pfile, QStr::number(qApp->applicationPid()))) goto error;
    sleep(300);

    while(true)
    {
        if(sb::left(sb::fload(pfile), 7) == "restart")
        {
            sb::unlock(sb::Schdlrlock);
            sb::exec("/usr/lib/systemback/sbscheduler", NULL, true, true);
            break;
        }

        sb::cfgread();
        if(! isdir(sb::sdir[1]) || ! sb::access(sb::sdir[1], sb::Write)) goto next;

        if(! isfile(sb::sdir[1] % "/.sbschedule"))
        {
            sb::crtfile(sb::sdir[1] % "/.sbschedule", NULL);
            goto next;
        }

        if(sb::schdle[0] == "on")
        {
            if(QFileInfo(sb::sdir[1] % "/.sbschedule").lastModified().secsTo(QDateTime::currentDateTime()) / 60 < sb::schdle[1].toShort() * 1440 + sb::schdle[2].toShort() * 60 + sb::schdle[3].toShort()) goto next;
            if(! sb::lock(sb::Sblock)) goto next;

            if(! sb::lock(sb::Dpkglock))
            {
                sb::unlock(sb::Sblock);
                goto next;
            }

            if(sb::schdle[5] == "on" || ! isfile("/usr/bin/systemback"))
                newrestorepoint();
            else
            {
                QStr xauth("/tmp/sbXauthority-" % sb::rndstr());

                if(QFile(getenv("XAUTHORITY")).copy(xauth))
                {
                    sb::exec("/usr/bin/systemback schedule " % sb::schdle[6], "XAUTHORITY=" % xauth);
                    QFile::remove(xauth);
                }
                else
                    sb::exec("/usr/bin/systemback schedule " % sb::schdle[6]);
            }

            sb::unlock(sb::Sblock);
            sb::unlock(sb::Dpkglock);
            sleep(50);
        }

    next:;
        sleep(10);
    }

    qApp->quit();
}

void scheduler::newrestorepoint()
{
    QSL dlst(QDir(sb::sdir[1]).entryList(QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot));

    for(uchar a(0) ; a < dlst.count() ; ++a)
    {
        QStr iname(dlst.at(a));

        if(sb::like(iname, QSL() << "_.DELETED_*" << "_.S00_*"))
            if(! sb::remove(sb::sdir[1] % '/' % iname)) return;
    }

    if(! sb::pnames[9].isEmpty())
    {
        QFile::rename(sb::sdir[1] % "/S10_" % sb::pnames[9], sb::sdir[1] % "/.DELETED_" % sb::pnames[9]);
        if(! sb::remove(sb::sdir[1] % "/.DELETED_" % sb::pnames[9])) return;
    }

    if(! sb::pnames[8].isEmpty() && sb::pnumber < 10)
    {
        QFile::rename(sb::sdir[1] % "/S09_" % sb::pnames[8], sb::sdir[1] % "/.DELETED_" % sb::pnames[8]);
        if(! sb::remove(sb::sdir[1] % "/.DELETED_" % sb::pnames[8])) return;
    }

    if(! sb::pnames[7].isEmpty() && sb::pnumber < 9)
    {
        QFile::rename(sb::sdir[1] % "/S08_" % sb::pnames[7], sb::sdir[1] % "/.DELETED_" % sb::pnames[7]);
        if(! sb::remove(sb::sdir[1] % "/.DELETED_" % sb::pnames[7])) return;
    }

    if(! sb::pnames[6].isEmpty() && sb::pnumber < 8)
    {
        QFile::rename(sb::sdir[1] % "/S07_" % sb::pnames[6], sb::sdir[1] % "/.DELETED_" % sb::pnames[6]);
        if(! sb::remove(sb::sdir[1] % "/.DELETED_" % sb::pnames[6])) return;
    }

    if(! sb::pnames[5].isEmpty() && sb::pnumber < 7)
    {
        QFile::rename(sb::sdir[1] % "/S06_" % sb::pnames[5], sb::sdir[1] % "/.DELETED_" % sb::pnames[5]);
        if(! sb::remove(sb::sdir[1] % "/.DELETED_" % sb::pnames[5])) return;
    }

    if(! sb::pnames[4].isEmpty() && sb::pnumber < 6)
    {
        QFile::rename(sb::sdir[1] % "/S05_" % sb::pnames[4], sb::sdir[1] % "/.DELETED_" % sb::pnames[4]);
        if(! sb::remove(sb::sdir[1] % "/.DELETED_" % sb::pnames[4])) return;
    }

    if(! sb::pnames[3].isEmpty() && sb::pnumber < 5)
    {
        QFile::rename(sb::sdir[1] % "/S04_" % sb::pnames[3], sb::sdir[1] % "/.DELETED_" % sb::pnames[3]);
        if(! sb::remove(sb::sdir[1] % "/.DELETED_" % sb::pnames[3])) return;
    }

    if(! sb::pnames[2].isEmpty() && sb::pnumber == 3)
    {
        QFile::rename(sb::sdir[1] % "/S03_" % sb::pnames[2], sb::sdir[1] % "/.DELETED_" % sb::pnames[2]);
        if(! sb::remove(sb::sdir[1] % "/.DELETED_" % sb::pnames[2])) return;
    }

    QStr dtime(QDateTime().currentDateTime().toString("yyyy-MM-dd,hh.mm.ss"));
    if(! sb::crtrpoint(sb::sdir[1], ".S00_" % dtime)) return;

    if(isdir(sb::sdir[1] % "/S01_" % sb::pnames[0]))
    {
        if(! QFile::rename(sb::sdir[1] % "/S01_" % sb::pnames[0], sb::sdir[1] % "/S02_" % sb::pnames[0])) return;

        if(isdir(sb::sdir[1] % "/S02_" % sb::pnames[1]))
        {
            if(! QFile::rename(sb::sdir[1] % "/S02_" % sb::pnames[1], sb::sdir[1] % "/S03_" % sb::pnames[1])) return;

            if(isdir(sb::sdir[1] % "/S03_" % sb::pnames[2]))
            {
                if(! QFile::rename(sb::sdir[1] % "/S03_" % sb::pnames[2], sb::sdir[1] % "/S04_" % sb::pnames[2])) return;

                if(isdir(sb::sdir[1] % "/S04_" % sb::pnames[3]))
                {
                    if(! QFile::rename(sb::sdir[1] % "/S04_" % sb::pnames[3], sb::sdir[1] % "/S05_" % sb::pnames[3])) return;

                    if(isdir(sb::sdir[1] % "/S05_" % sb::pnames[4]))
                    {
                        if(! QFile::rename(sb::sdir[1] % "/S05_" % sb::pnames[4], sb::sdir[1] % "/S06_" % sb::pnames[4])) return;

                        if(isdir(sb::sdir[1] % "/S06_" % sb::pnames[5]))
                        {
                            if(! QFile::rename(sb::sdir[1] % "/S06_" % sb::pnames[5], sb::sdir[1] % "/S07_" % sb::pnames[5])) return;

                            if(isdir(sb::sdir[1] % "/S07_" % sb::pnames[6]))
                            {
                                if(! QFile::rename(sb::sdir[1] % "/S07_" % sb::pnames[6], sb::sdir[1] % "/S08_" % sb::pnames[6])) return;

                                if(isdir(sb::sdir[1] % "/S08_" % sb::pnames[7]))
                                {
                                    if(! QFile::rename(sb::sdir[1] % "/S08_" % sb::pnames[7], sb::sdir[1] % "/S09_" % sb::pnames[7])) return;

                                    if(isdir(sb::sdir[1] % "/S09_" % sb::pnames[8]))
                                       if(! QFile::rename(sb::sdir[1] % "/S09_" % sb::pnames[8], sb::sdir[1] % "/S10_" % sb::pnames[8])) return;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    if(! QFile::rename(sb::sdir[1] % "/.S00_" % dtime, sb::sdir[1] % "/S01_" % dtime)) return;
    sb::crtfile(sb::sdir[1] % "/.sbschedule", NULL);
    sb::fssync();
    sb::crtfile("/proc/sys/vm/drop_caches", "3");
}
