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

#include "sbsustart.hpp"
#include <QProcess>

void sustart::main()
{
    {
        uchar rv(! sb::like(qApp->arguments().count(), {2, 3}) || ! sb::like(qApp->arguments().at(1), {"_systemback_", "_finstall_", "_scheduler_"}) ? 2 : [&] {
                QStr uname, usrhm;

                if(uid == 0)
                    uname = "root", usrhm = "/root";
                else
                {
                    QFile file("/etc/passwd");

                    if(sb::fopen(file))
                        while(! file.atEnd())
                        {
                            QStr line(file.readLine().trimmed());

                            if(line.contains("x:" % QStr::number(uid) % ':'))
                            {
                                QSL uslst(line.split(':'));
                                uname = uslst.at(0), usrhm = uslst.at(5);
                                break;
                            }
                        }

                    if(uname.isEmpty() || usrhm.isEmpty()) return 3;
                }

                bool uidinr(getuid() > 0), gidinr(getgid() > 0);

                if(uidinr || gidinr)
                {
                    if((uidinr && setuid(0) == -1) || (gidinr && setgid(0) == -1)) return 3;

                    auto clrenv([](cQBA &uhm, cQStr &xpath = nullptr) {
                            QSL excl{"_DISPLAY_", "_PATH_", "_LANG_", "_XAUTHORITY_"};

                            for(cQStr &cvar : QProcess::systemEnvironment())
                            {
                                QStr var(sb::left(cvar, sb::instr(cvar, "=") - 1));
                                if(! sb::like(var, excl) && ! qunsetenv(bstr(var))) return false;
                            }

                            if(! qputenv("USER", "root") || ! qputenv("HOME", uhm) || ! qputenv("LOGNAME", "root") || ! qputenv("SHELL", "/bin/bash") || ! (xpath.isEmpty() || qputenv("XAUTHORITY", xpath.toUtf8()))) return false;
                            return true;
                        });

                    if(qApp->arguments().at(1) == "scheduler")
                    {
                        if(! clrenv(usrhm.toUtf8())) return 3;
                        cmd = new QStr("sbscheduler " % uname);
                    }
                    else
                    {
                        QStr xauth("/tmp/sbXauthority-" % sb::rndstr());
                        if((qEnvironmentVariableIsEmpty("XAUTHORITY") || ! QFile(qgetenv("XAUTHORITY")).copy(xauth)) && (! sb::isfile("/home/" % uname % "/.Xauthority") || ! QFile("/home/" % uname % "/.Xauthority").copy(xauth)) && (! sb::isfile(usrhm % "/.Xauthority") || ! QFile(usrhm % "/.Xauthority").copy(xauth))) return 4;
                        if(! clrenv("/root", xauth)) return 3;
                        cmd = new QStr("systemback " % (qApp->arguments().at(1) == "systemback" ? "authorization " % uname : QStr("finstall ")));
                    }
                }
                else
                {
                    cmd = new QStr(qApp->arguments().at(1) == "scheduler" ? [&]() -> QStr {
                            qputenv("HOME", usrhm.toUtf8());
                            return "sbscheduler " % uname;
                        }() : "systemback" % QStr(qApp->arguments().at(1) == "finstall" ? " finstall" : nullptr));
                }

                return 0;
            }());

        if(rv > 0)
        {
            if(rv == 2)
                sb::error("\n " % sb::tr("Missing, wrong or too much argument(s).") % "\n\n");
            else
            {
                QStr emsg((qApp->arguments().at(1) == "scheduler" ? sb::tr("Cannot start the Systemback scheduler daemon!") : sb::tr("Cannot start the Systemback graphical user interface!")) % "\n\n" % (rv == 3 ? sb::tr("Unable to get root permissions.") : sb::tr("Unable to connect to the X server.")));

#if QT_VERSION < QT_VERSION_CHECK(5, 3, 0)
                if(uid != geteuid() && seteuid(uid) == -1)
                    sb::error("\n " % emsg.replace("\n\n", "\n\n ") % "\n\n");
                else
#endif
                sb::exec((sb::execsrch("zenity") ? "zenity --title=Systemback --error --text=\"" : "kdialog --title=Systemback --error=\"") % emsg % '\"', nullptr, sb::Bckgrnd);
            }

            qApp->exit(rv);
            return;
        }
    }

    if(qApp->arguments().count() == 3 && qApp->arguments().at(2) == "gtk+") qputenv("QT_STYLE_OVERRIDE", "gtk+");
    qApp->exit(sb::exec(*cmd));
}
