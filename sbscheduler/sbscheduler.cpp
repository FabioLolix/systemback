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

scheduler::scheduler()
{
    pfile = nullptr;
}

scheduler::~scheduler()
{
    if(pfile) delete pfile;
}

void scheduler::main()
{
    {
        uchar rv([&] {
                return qApp->arguments().count() != 2 ? 1
                    : sb::schdlr[1] != "false" && (sb::schdlr[1] == "everyone" || sb::right(sb::schdlr[1], -1).split(',').contains(qApp->arguments().value(1))) ? 2
                    : getuid() + getgid() > 0 ? 3
                    : sb::isfile("/cdrom/casper/filesystem.squashfs") || sb::isfile("/lib/live/mount/medium/live/filesystem.squashfs") ? 4
                    : ! sb::lock(sb::Schdlrlock) ? 5
                    : daemon(0, 0) == -1 ? 6
                    : [&]() -> uchar {
                            sb::delay(100);
                            return sb::lock(sb::Schdlrlock) && sb::crtfile(*(pfile = new QStr(sb::isdir("/run") ? "/run/sbscheduler.pid" : "/var/run/sbscheduler.pid")), QStr::number(qApp->applicationPid())) ? 0 : 255;
                        }();
            }());

        if(rv > 0)
        {
            if(rv < 255) sb::error("\n " % tr("Cannot start Systemback scheduler daemon!") % "\n\n " % [rv] {
                    switch(rv) {
                    case 1:
                        return tr("Missing, wrong or too much argument(s).");
                    case 2:
                        return tr("The process is disabled for this user.");
                    case 3:
                        return tr("Root privileges are required.");
                    case 4:
                        return tr("This system is a Live.");
                    case 5:
                        return tr("Already running.");
                    default:
                        return tr("Unable to daemonize.");
                    }
                }() % "\n\n");

            qApp->exit(rv);
            return;
        }
    }

    QDateTime pflmd(QFileInfo(*pfile).lastModified());
    sleep(290);

    forever
    {
        sleep(10);

        if(! sb::isfile(*pfile) || (pflmd != QFileInfo(*pfile).lastModified() && sb::fload(*pfile) != QBA::number(qApp->applicationPid())))
        {
            sb::unlock(sb::Schdlrlock);
            sb::exec("sbscheduler " % qApp->arguments().value(1), nullptr, sb::Silent | sb::Bckgrnd);
            break;
        }

        if(! sb::isfile("/etc/systemback.conf") || cfglmd != QFileInfo("/etc/systemback.conf").lastModified())
        {
            sb::cfgread();
            cfglmd = QFileInfo("/etc/systemback.conf").lastModified();
        }

        if(! sb::isdir(sb::sdir[1]) || ! sb::access(sb::sdir[1], sb::Write))
            sleep(50);
        else if(! sb::isfile(sb::sdir[1] % "/.sbschedule"))
            sb::crtfile(sb::sdir[1] % "/.sbschedule");
        else if(sb::schdle[0] == sb::False)
            sleep(1790);
        else if(QFileInfo(sb::sdir[1] % "/.sbschedule").lastModified().secsTo(QDateTime::currentDateTime()) / 60 >= sb::schdle[1] * 1440 + sb::schdle[2] * 60 + sb::schdle[3] && sb::lock(sb::Sblock))
        {
            if(! sb::lock(sb::Dpkglock))
                sb::unlock(sb::Sblock);
            else
            {
                if(sb::schdle[5] == sb::True || ! sb::execsrch("systemback"))
                    newrpnt();
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
        }
    }

    qApp->quit();
}

void scheduler::newrpnt()
{
    sb::pupgrade();

    for(cQStr &item : QDir(sb::sdir[1]).entryList(QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot))
        if(sb::like(item, {"_.DELETED_*", "_.S00_*"}) && ! sb::remove(sb::sdir[1] % '/' % item)) return;

    for(uchar a(9) ; a > 1 ; --a)
        if(! sb::pnames[a].isEmpty() && (a == 9 || a > 2 ? sb::pnumber < a + 2 : sb::pnumber == 3) && ! (QFile::rename(sb::sdir[1] % (a < 9 ? QStr("/S0" % QStr::number(a + 1)) : "/S10") % '_' % sb::pnames[a], sb::sdir[1] % "/.DELETED_" % sb::pnames[a]) && sb::remove(sb::sdir[1] % "/.DELETED_" % sb::pnames[a]))) return;

    QStr dtime(QDateTime().currentDateTime().toString("yyyy-MM-dd,hh.mm.ss"));

    if(sb::crtrpoint(sb::sdir[1], ".S00_" % dtime))
    {
        for(uchar a(0) ; a < 9 && sb::isdir(sb::sdir[1] % "/S0" % QStr::number(a + 1) % '_' % sb::pnames[a]) ; ++a)
            if(! QFile::rename(sb::sdir[1] % "/S0" % QStr::number(a + 1) % '_' % sb::pnames[a], sb::sdir[1] % (a < 8 ? "/S0" : "/S") % QStr::number(a + 2) % '_' % sb::pnames[a])) return;

        if(! QFile::rename(sb::sdir[1] % "/.S00_" % dtime, sb::sdir[1] % "/S01_" % dtime)) return;
    }
    else if(sb::dfree(sb::sdir[1]) < 104857600)
        sb::remove(sb::sdir[1] % "/.S00_" % dtime);
    else
        return;

    sb::crtfile(sb::sdir[1] % "/.sbschedule");
    sb::fssync();
    if(sb::ecache == sb::True) sb::crtfile("/proc/sys/vm/drop_caches", "3");
}
