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

#include "systemback-cli.hpp"
#include <QCoreApplication>
#include <QStringBuilder>
#include <QTextStream>
#include <QDateTime>
#include <QDir>
#include <ncursesw/ncurses.h>
#include <unistd.h>

#ifdef instr
#undef instr
#endif

#ifdef timeout
#undef timeout
#endif

systemback::systemback(QObject *parent) : QObject(parent)
{
    yn[0] = tr("(Y/N)").at(1);
    yn[1] = tr("(Y/N)").at(3);
    ptimer = new QTimer;
    ptimer->setInterval(2000);
    connect(ptimer, SIGNAL(timeout()), this, SLOT(progress()));
}

void systemback::main()
{
    QStr help("\n " % tr("Usage: systemback-cli [option]\n\n"
                         " Options:\n\n"
                         "  -n, --newbackup          create a new restore point\n\n"
                         "  -s, --storagedir <path>  get or set restore points storage directory path\n\n"
                         "  -u, --upgrade            upgrade current system\n"
                         "                           remove unnecessary files and packages\n\n"
                         "  -v, --version            output Systemback version number\n\n"
                         "  -h, --help               show this help") % "\n\n");
    uchar rv(0);
    goto start;
error:
    switch(rv) {
    case 1:
        sb::error(help);
        break;
    case 2:
        sb::error("\n " % tr("Root privileges are required for running Systemback!") % "\n\n");
        break;
    case 3:
        sb::error("\n " % tr("Another Systemback process is currently running, please wait until it\n finishes.") % "\n\n");
        break;
    case 4:
        sb::error("\n " % tr("Unable to get exclusive lock!") % "\n\n " % tr("First, close all package manager.") % "\n\n");
        break;
    case 5:
        sb::error("\n " % tr("The specified storage directory path has not been set!") % "\n\n");
        break;
    case 6:
        sb::error("\n " % tr("Restoration is aborted!") % "\n\n");
        break;
    case 7:
        sb::error("\n " % tr("Restoration is completed, but an error occurred while reinstalling GRUB!") % "\n\n");
        break;
    case 8:
        sb::error("\n " % tr("Restore point creation is aborted!") % "\n\n " % tr("Not enough free disk space to complete the process.") % "\n\n");
        break;
    case 9:
        sb::error("\n " % tr("Restore point creation is aborted!") % "\n\n " % tr("There has been critical changes in the file system during this operation.") % "\n\n");
        break;
    case 10:
        sb::error("\n " % tr("Restore points storage directory is not available or not writable!") % "\n\n");
        break;
    case 11:
        sb::error("\n " % tr("This stupid terminal does not support color!") % "\n\n");
        break;
    case 12:
        sb::error("\n " % tr("This terminal is too small!") % " (< 80x24)\n\n");
        break;
    case 13:
        sb::error("\n " % tr("Restore point deletion is aborted!") % "\n\n " % tr("An error occurred while during the process.") % "\n\n");
    }

    qApp->exit(rv);
    return;
start:
    if(sb::like(qApp->arguments().value(1), {"_-h_", "_--help_"}))
        sb::print(help);
    else if(sb::like(qApp->arguments().value(1), {"_-v_", "_--version_"}))
    {
        QFile file(":version");
        file.open(QIODevice::ReadOnly);
        sb::print("\n " % file.readLine().trimmed() % "_Qt5_" % sb::getarch() % "\n\n");
    }
    else if(getuid() + getgid() > 0)
    {
        rv = 2;
        goto error;
    }
    else
    {
        sb::cfgread();

        if(! sb::lock(sb::Sblock))
        {
            rv = 3;
            goto error;
        }

        if(! sb::lock(sb::Dpkglock))
        {
            rv = 4;
            goto error;
        }

        if(qApp->arguments().count() == 1)
        {
            if((rv = uinit()) > 0) goto error;
            rv = clistart();
            endwin();
        }
        else if(sb::like(qApp->arguments().value(1), {"_-n_", "_--newrestorepoint_"}))
        {
            if(! sb::isdir(sb::sdir[1]) || ! sb::access(sb::sdir[1], sb::Write))
            {
                rv = 10;
                goto error;
            }

            if((rv = uinit()) > 0) goto error;
            sb::pupgrade();
            if(! newrestorepoint()) rv = sb::dfree(sb::sdir[1]) < 104857600 ? 8 : 9;
            endwin();
        }
        else if(sb::like(qApp->arguments().value(1), {"_-s_", "_--storagedir_"}))
            rv = storagedir();
        else if(sb::like(qApp->arguments().value(1), {"_-u_", "_--upgrade_"}))
        {
            sb::unlock(sb::Dpkglock);
            sb::supgrade({tr("An error occurred while upgrading the system!"), tr("Restart upgrade ...")});
        }
        else
        {
            rv = 1;
            goto error;
        }
    }

    if(rv == 0)
        qApp->quit();
    else
        goto error;
}

uchar systemback::uinit()
{
    initscr();
    uchar rv(has_colors() ? LINES < 24 || COLS < 80 ? 12 : 0 : 11);

    if(rv > 0)
    {
        endwin();
        return rv;
    }

    noecho();
    raw();
    curs_set(0);
    attron(A_BOLD);
    start_color();
    assume_default_colors(COLOR_BLUE, COLOR_BLACK);
    init_pair(1, COLOR_WHITE, COLOR_BLACK);
    init_pair(2, COLOR_BLUE, COLOR_BLACK);
    init_pair(3, COLOR_GREEN, COLOR_BLACK);
    init_pair(4, COLOR_YELLOW, COLOR_BLACK);
    init_pair(5, COLOR_RED, COLOR_BLACK);
    return 0;
}

uchar systemback::clistart()
{
    mvprintw(0, COLS / 2 - 6 - tr("basic restore UI").length() / 2, QStr("Systemback " % tr("basic restore UI")).toStdString().c_str());
    attron(COLOR_PAIR(1));
    printw(QStr("\n\n " % tr("Available restore point(s):") % "\n\n").toStdString().c_str());
    sb::pupgrade();
    attron(COLOR_PAIR(3));
    if(! sb::pnames[0].isEmpty()) printw(QStr("  1 ─ " % sb::left(sb::pnames[0], COLS - 7) % '\n').toStdString().c_str());
    if(! sb::pnames[1].isEmpty()) printw(QStr("  2 ─ " % sb::left(sb::pnames[1], COLS - 7) % '\n').toStdString().c_str());
    if(sb::pnumber == 3) attron(COLOR_PAIR(5));
    if(! sb::pnames[2].isEmpty()) printw(QStr("  3 ─ " % sb::left(sb::pnames[2], COLS - 7) % '\n').toStdString().c_str());
    if(sb::pnumber == 4) attron(COLOR_PAIR(5));
    if(! sb::pnames[3].isEmpty()) printw(QStr("  4 ─ " % sb::left(sb::pnames[3], COLS - 7) % '\n').toStdString().c_str());
    if(sb::pnumber == 5) attron(COLOR_PAIR(5));
    if(! sb::pnames[4].isEmpty()) printw(QStr("  5 ─ " % sb::left(sb::pnames[4], COLS - 7) % '\n').toStdString().c_str());
    if(sb::pnumber == 6) attron(COLOR_PAIR(5));
    if(! sb::pnames[5].isEmpty()) printw(QStr("  6 ─ " % sb::left(sb::pnames[5], COLS - 7) % '\n').toStdString().c_str());
    if(sb::pnumber == 7) attron(COLOR_PAIR(5));
    if(! sb::pnames[6].isEmpty()) printw(QStr("  7 ─ " % sb::left(sb::pnames[6], COLS - 7) % '\n').toStdString().c_str());
    if(sb::pnumber == 8) attron(COLOR_PAIR(5));
    if(! sb::pnames[7].isEmpty()) printw(QStr("  8 ─ " % sb::left(sb::pnames[7], COLS - 7) % '\n').toStdString().c_str());
    if(sb::pnumber == 9) attron(COLOR_PAIR(5));
    if(! sb::pnames[8].isEmpty()) printw(QStr("  9 ─ " % sb::left(sb::pnames[8], COLS - 7) % '\n').toStdString().c_str());
    if(sb::pnumber == 10) attron(COLOR_PAIR(5));
    if(! sb::pnames[9].isEmpty()) printw(QStr("  A ─ " % sb::left(sb::pnames[9], COLS - 7) % '\n').toStdString().c_str());
    attron(COLOR_PAIR(3));
    if(! sb::pnames[10].isEmpty()) printw(QStr("  B ─ " % sb::left(sb::pnames[10], COLS - 7) % '\n').toStdString().c_str());
    if(! sb::pnames[11].isEmpty()) printw(QStr("  C ─ " % sb::left(sb::pnames[11], COLS - 7) % '\n').toStdString().c_str());
    if(! sb::pnames[12].isEmpty()) printw(QStr("  D ─ " % sb::left(sb::pnames[12], COLS - 7) % '\n').toStdString().c_str());
    if(! sb::pnames[13].isEmpty()) printw(QStr("  E ─ " % sb::left(sb::pnames[13], COLS - 7) % '\n').toStdString().c_str());
    if(! sb::pnames[14].isEmpty()) printw(QStr("  F ─ " % sb::left(sb::pnames[14], COLS - 7) % '\n').toStdString().c_str());
    printw(QStr("\n G ─ " % tr("Create new") % "\n Q ─ " % tr("Quit") % '\n').toStdString().c_str());
    attron(COLOR_PAIR(2));
    mvprintw(LINES - 1, COLS - 13, "Kendek, GPLv3");
    refresh();
    if(! pname.isEmpty()) pname.clear();
    if(sb::Progress != -1) sb::Progress = -1;

    do
        switch(getch()) {
        case '1':
            cpoint = "S01";
            if(! sb::pnames[0].isEmpty()) pname = sb::pnames[0];
            break;
        case '2':
            cpoint = "S02";
            if(! sb::pnames[1].isEmpty()) pname = sb::pnames[1];
            break;
        case '3':
            cpoint = "S03";
            if(! sb::pnames[2].isEmpty()) pname = sb::pnames[2];
            break;
        case '4':
            cpoint = "S04";
            if(! sb::pnames[3].isEmpty()) pname = sb::pnames[3];
            break;
        case '5':
            cpoint = "S05";
            if(! sb::pnames[4].isEmpty()) pname = sb::pnames[4];
            break;
        case '6':
            cpoint = "S06";
            if(! sb::pnames[5].isEmpty()) pname = sb::pnames[5];
            break;
        case '7':
            cpoint = "S07";
            if(! sb::pnames[6].isEmpty()) pname = sb::pnames[6];
            break;
        case '8':
            cpoint = "S08";
            if(! sb::pnames[7].isEmpty()) pname = sb::pnames[7];
            break;
        case '9':
            cpoint = "S09";
            if(! sb::pnames[8].isEmpty()) pname = sb::pnames[8];
            break;
        case 'A':
            cpoint = "S10";
            if(! sb::pnames[9].isEmpty()) pname = sb::pnames[9];
            break;
        case 'B':
            cpoint = "H01";
            if(! sb::pnames[10].isEmpty()) pname = sb::pnames[10];
            break;
        case 'C':
            cpoint = "H02";
            if(! sb::pnames[11].isEmpty()) pname = sb::pnames[11];
            break;
        case 'D':
            cpoint = "H03";
            if(! sb::pnames[12].isEmpty()) pname = sb::pnames[12];
            break;
        case 'E':
            cpoint = "H04";
            if(! sb::pnames[13].isEmpty()) pname = sb::pnames[13];
            break;
        case 'F':
            cpoint = "H05";
            if(! sb::pnames[14].isEmpty()) pname = sb::pnames[14];
            break;
        case 'g':
        case 'G':
            if(! newrestorepoint()) return sb::dfree(sb::sdir[1]) < 104857600 ? 8 : 9;
            clear();
            return clistart();
        case 'q':
        case 'Q':
            return 0;
        }
    while(pname.isEmpty());

    clear();
    mvprintw(0, COLS / 2 - 6 - tr("basic restore UI").length() / 2, QStr("Systemback " % tr("basic restore UI")).toStdString().c_str());
    attron(COLOR_PAIR(1));
    printw(QStr("\n\n " % tr("Selected restore point:")).toStdString().c_str());
    attron(COLOR_PAIR(4));
    printw(QStr("\n\n  " % sb::left(pname, COLS - 3)).toStdString().c_str());
    attron(COLOR_PAIR(3));
    printw(QStr("\n\n 1 ─ " % tr("Delete") % "\n 2 ─ " % tr("System restore") % " ▸\n B ─ ◂ " % tr("Back")).toStdString().c_str());
    attron(COLOR_PAIR(2));
    mvprintw(LINES - 1, COLS - 13, "Kendek, GPLv3");
    refresh();

    for(;;)
        switch(getch()) {
        case '1':
            if(! pointdelete()) return 13;
            clear();
            return clistart();
        case '2':
            clear();
            return restore();
        case 'b':
        case 'B':
            clear();
            return clistart();
        }
}

uchar systemback::storagedir()
{
    if(qApp->arguments().count() == 2)
        sb::print("\n " % sb::sdir[0] % "\n\n");
    else
    {
        QStr ndir(qApp->arguments().value(2));

        if(qApp->arguments().count() > 3)
            for(uchar a(3) ; a < qApp->arguments().count() ; ++a) ndir.append(' ' % qApp->arguments().value(a));

        if(sb::like(ndir, {"*/systemback_", "*/_", "_/bin_", "_/bin/*", "_/boot_", "_/boot/*", "_/cdrom_", "_/cdrom/*", "_/dev_", "_/dev/*", "_/etc_", "_/etc/*", "_/lib_", "_/lib/*", "_/lib32_", "_/lib32/*", "_/lib64_", "_/lib64/*", "_/opt_", "_/opt/*", "_/proc_", "_/proc/*", "_/root_", "_/root/*", "_/run_", "_/run/*", "_/sbin_", "_/sbin/*", "_/selinux_", "_/selinux/*", "_/srv_", "_/sys/*", "_/tmp_", "_/tmp/*", "_/usr_", "_/usr/*", "_/var_", "_/var/*"}) || sb::fload("/etc/passwd").contains(':' % ndir % ':') || ! sb::islnxfs(ndir)) return 5;

        if(sb::sdir[0] != ndir)
        {
            if(sb::isdir(sb::sdir[1]))
            {
                QSL dlst(QDir(sb::sdir[1]).entryList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot));

                if(dlst.count() == 0)
                    QDir().rmdir(sb::sdir[1]);
                else if(dlst.count() == 1 && sb::isfile(sb::sdir[1] % "/.sbschedule"))
                    sb::remove(sb::sdir[1]);
            }

            sb::sdir[0] = ndir;
            sb::cfgwrite();
            sb::sdir[1] = sb::sdir[0] % "/Systemback";
        }

        if(! sb::isdir(sb::sdir[1]) && ! QDir().mkdir(sb::sdir[1]))
        {
            QFile::rename(sb::sdir[1], sb::sdir[1] % '_' % sb::rndstr());
            QDir().mkdir(sb::sdir[1]);
        }

        if(! sb::isfile(sb::sdir[1] % "/.sbschedule")) sb::crtfile(sb::sdir[1] % "/.sbschedule");
        sb::print("\n " % tr("The specified storage directory path is set.") % "\n\n");
    }

    return 0;
}

bool systemback::newrestorepoint()
{
    goto start;
error:
    ptimer->stop();
    return false;
start:
    QTimer::singleShot(0, this, SLOT(progress()));
    ptimer->start();

    for(cQStr &item : QDir(sb::sdir[1]).entryList(QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot))
        if(sb::like(item, {"_.DELETED_*", "_.S00_*"}))
        {
            if(prun != tr("Deleting incomplete restore point")) prun = tr("Deleting incomplete restore point");
            if(! sb::remove(sb::sdir[1] % '/' % item)) goto error;
        }

    for(uchar a(9) ; a > 1 ; --a)
        if(! sb::pnames[a].isEmpty() && (a < 9 ? (a > 2 ? sb::pnumber < a + 2 : sb::pnumber == 3) : true))
        {
            if(prun != tr("Deleting old restore point(s)")) prun = tr("Deleting old restore point(s)");
            if(! QFile::rename(sb::sdir[1] % (a < 9 ? QStr("/S0" % QStr::number(a + 1)) : "/S10") % '_' % sb::pnames[a], sb::sdir[1] % "/.DELETED_" % sb::pnames[a]) || ! sb::remove(sb::sdir[1] % "/.DELETED_" % sb::pnames[a])) goto error;
        }

    prun = tr("Creating restore point");
    QStr dtime(QDateTime().currentDateTime().toString("yyyy-MM-dd,hh.mm.ss"));
    if(! sb::crtrpoint(sb::sdir[1], ".S00_" % dtime)) goto error;

    for(uchar a(0) ; a < 9 && sb::isdir(sb::sdir[1] % "/S0" % QStr::number(a + 1) % '_' % sb::pnames[a]) ; ++a)
        if(! QFile::rename(sb::sdir[1] % "/S0" % QStr::number(a + 1) % '_' % sb::pnames[a], sb::sdir[1] % (a < 8 ? "/S0" : "/S") % QStr::number(a + 2) % '_' % sb::pnames[a])) goto error;

    if(! QFile::rename(sb::sdir[1] % "/.S00_" % dtime, sb::sdir[1] % "/S01_" % dtime)) goto error;
    sb::crtfile(sb::sdir[1] % "/.sbschedule");
    prun = tr("Emptying cache");
    sb::fssync();
    sb::crtfile("/proc/sys/vm/drop_caches", "3");
    ptimer->stop();
    return true;
}

bool systemback::pointdelete()
{
    goto start;
error:
    ptimer->stop();
    return false;
start:
    prun = tr("Deleting restore point");
    QTimer::singleShot(0, this, SLOT(progress()));
    ptimer->start();
    if(! QFile::rename(sb::sdir[1] % '/' % cpoint % '_' % pname, sb::sdir[1] % "/.DELETED_" % pname) || ! sb::remove(sb::sdir[1] % "/.DELETED_" % pname)) goto error;
    prun = tr("Emptying cache");
    sb::fssync();
    sb::crtfile("/proc/sys/vm/drop_caches", "3");
    ptimer->stop();
    return true;
}

uchar systemback::restore()
{
    mvprintw(0, COLS / 2 - 6 - tr("basic restore UI").length() / 2, QStr("Systemback " % tr("basic restore UI")).toStdString().c_str());
    attron(COLOR_PAIR(1));
    printw(QStr("\n\n " % tr("Restore with the following restore point:")).toStdString().c_str());
    attron(COLOR_PAIR(4));
    printw(QStr("\n\n  " % pname).toStdString().c_str());
    attron(COLOR_PAIR(1));
    printw(QStr("\n\n " % tr("Restore with the following restore method:")).toStdString().c_str());
    attron(COLOR_PAIR(3));
    printw(QStr("\n\n  1 ─ " % tr("Full restore") % "\n  2 ─ " % tr("System files restore")).toStdString().c_str());
    attron(COLOR_PAIR(1));
    printw(QStr("\n\n  " % tr("Users configuration files restore")).toStdString().c_str());
    attron(COLOR_PAIR(3));
    printw(QStr("\n\n   3 ─ " % tr("Complete configuration files restore") % "\n   4 ─ " % tr("Keep newly installed configuration files") % "\n\n C ─ " % tr("Cancel")).toStdString().c_str());
    attron(COLOR_PAIR(2));
    mvprintw(LINES - 1, COLS - 13, "Kendek, GPLv3");
    refresh();
    uchar rmode(0);

    do
        switch(getch()) {
        case 'c':
        case 'C':
            clear();
            return clistart();
        case '1':
            rmode = 1;
            break;
        case '2':
            rmode = 2;
            break;
        case '3':
            rmode = 3;
            break;
        case '4':
            rmode = 4;
        }
    while(rmode == 0);

    clear();
    mvprintw(0, COLS / 2 - 6 - tr("basic restore UI").length() / 2, QStr("Systemback " % tr("basic restore UI")).toStdString().c_str());
    attron(COLOR_PAIR(1));
    printw(QStr("\n\n " % tr("Restore with the following restore point:")).toStdString().c_str());
    attron(COLOR_PAIR(4));
    printw(QStr("\n\n  " % pname).toStdString().c_str());
    attron(COLOR_PAIR(1));
    printw(QStr("\n\n " % tr("Restore with the following restore method:")).toStdString().c_str());
    attron(COLOR_PAIR(4));

    switch(rmode) {
    case 1:
        printw(QStr("\n\n  " % tr("Full restore")).toStdString().c_str());
        break;
    case 2:
        printw(QStr("\n\n  " % tr("System files restore")).toStdString().c_str());
        break;
    case 3:
        printw(QStr("\n\n  " % tr("Complete configuration files restore")).toStdString().c_str());
        break;
    case 4:
        printw(QStr("\n\n  " % tr("Configuration files restore")).toStdString().c_str());
    }

    attron(COLOR_PAIR(3));
    uchar fsave(0), greinst(0);

    if(rmode < 3)
    {
        if(sb::isfile("/etc/fstab"))
        {
            printw(QStr("\n\n " % tr("You want to keep the current fstab file?") % ' ' % tr("(Y/N)")).toStdString().c_str());
            attron(COLOR_PAIR(2));
            mvprintw(LINES - 1, COLS - 13, "Kendek, GPLv3");
            refresh();

            do {
                QChar gtch(getch());

                if(sb::like(gtch.toUpper(), {"_Y_", '_' % yn[0] % '_'}))
                    fsave = 1;
                else if(sb::like(gtch.toUpper(), {"_N_", '_' % yn[1] % '_'}))
                    fsave = 2;
            } while(fsave == 0);

            clear();
            mvprintw(0, COLS / 2 - 6 - tr("basic restore UI").length() / 2, QStr("Systemback " % tr("basic restore UI")).toStdString().c_str());
            attron(COLOR_PAIR(1));
            printw(QStr("\n\n " % tr("Restore with the following restore point:")).toStdString().c_str());
            attron(COLOR_PAIR(4));
            printw(QStr("\n\n  " % pname).toStdString().c_str());
            attron(COLOR_PAIR(1));
            printw(QStr("\n\n " % tr("Restore with the following restore method:")).toStdString().c_str());
            attron(COLOR_PAIR(4));
            printw(QStr("\n\n  " % (rmode == 1 ? tr("Full restore") : tr("System files restore"))).toStdString().c_str());
            printw(QStr("\n\n " % tr("You want to keep the current fstab file?") % ' ' % tr("(Y/N)")).toStdString().c_str());
            printw(QStr(' ' % yn[fsave == 1 ? 0 : 1]).toStdString().c_str());
            attron(COLOR_PAIR(3));

            if(sb::execsrch("update-grub2", sb::sdir[1] % '/' % cpoint % '_' % pname))
            {
                printw(QStr("\n\n " % tr("Reinstall the GRUB 2 bootloader?") % ' ' % tr("(Y/N)")).toStdString().c_str());
                attron(COLOR_PAIR(2));
                mvprintw(LINES - 1, COLS - 13, "Kendek, GPLv3");
                refresh();

                do {
                    QChar gtch(getch());

                    if(sb::like(gtch.toUpper(), {"_Y_", '_' % yn[0] % '_'}))
                        greinst = 1;
                    else if(sb::like(gtch.toUpper(), {"_N_", '_' % yn[1] % '_'}))
                        greinst = 2;
                } while(greinst == 0);

                clear();
                mvprintw(0, COLS / 2 - 6 - tr("basic restore UI").length() / 2, QStr("Systemback " % tr("basic restore UI")).toStdString().c_str());
                attron(COLOR_PAIR(1));
                printw(QStr("\n\n " % tr("Restore with the following restore point:")).toStdString().c_str());
                attron(COLOR_PAIR(4));
                printw(QStr("\n\n  " % pname).toStdString().c_str());
                attron(COLOR_PAIR(1));
                printw(QStr("\n\n " % tr("Restore with the following restore method:")).toStdString().c_str());
                attron(COLOR_PAIR(4));
                printw(QStr("\n\n  " % (rmode == 1 ? tr("Full restore") : tr("System files restore"))).toStdString().c_str());
                printw(QStr("\n\n " % tr("You want to keep the current fstab file?") % ' ' % tr("(Y/N)")).toStdString().c_str());
                printw(QStr(' ' % yn[fsave == 1 ? 0 : 1]).toStdString().c_str());
                printw(QStr("\n\n " % tr("Reinstall the GRUB 2 bootloader?") % ' ' % tr("(Y/N)")).toStdString().c_str());
                printw(QStr(' ' % yn[greinst == 1 ? 0 : 1]).toStdString().c_str());
            }
        }
        else if(sb::execsrch("update-grub2", sb::sdir[1] % '/' % cpoint % '_' % pname))
        {
            printw(QStr("\n\n " % tr("Reinstall the GRUB 2 bootloader?") % ' ' % tr("(Y/N)")).toStdString().c_str());
            attron(COLOR_PAIR(2));
            mvprintw(LINES - 1, COLS - 13, "Kendek, GPLv3");
            refresh();

            do {
                QChar gtch(getch());

                if(sb::like(gtch.toUpper(), {"_Y_", '_' % yn[0] % '_'}))
                    greinst = 1;
                else if(sb::like(gtch.toUpper(), {"_N_", '_' % yn[1] % '_'}))
                    greinst = 2;
            } while(greinst == 0);

            clear();
            mvprintw(0, COLS / 2 - 6 - tr("basic restore UI").length() / 2, QStr("Systemback " % tr("basic restore UI")).toStdString().c_str());
            attron(COLOR_PAIR(1));
            printw(QStr("\n\n " % tr("Restore with the following restore point:")).toStdString().c_str());
            attron(COLOR_PAIR(4));
            printw(QStr("\n\n  " % pname).toStdString().c_str());
            attron(COLOR_PAIR(1));
            printw(QStr("\n\n " % tr("Restore with the following restore method:")).toStdString().c_str());
            attron(COLOR_PAIR(4));
            printw(QStr("\n\n  " % (rmode == 1 ? tr("Full restore") : tr("System files restore"))).toStdString().c_str());
            printw(QStr("\n\n " % tr("Reinstall the GRUB 2 bootloader?") % ' ' % tr("(Y/N)")).toStdString().c_str());
            printw(QStr(' ' % yn[greinst == 1 ? 0 : 1]).toStdString().c_str());
        }
    }

    attron(COLOR_PAIR(3));
    printw(QStr("\n\n " % tr("Start the restore?") % ' ' % tr("(Y/N)")).toStdString().c_str());
    attron(COLOR_PAIR(2));
    mvprintw(LINES - 1, COLS - 13, "Kendek, GPLv3");
    refresh();
    bool rstart(false);

    do {
        QChar gtch(getch());

        if(sb::like(gtch.toUpper(), {"_Y_", '_' % yn[0] % '_'}))
            rstart = true;
        else if(sb::like(gtch.toUpper(), {"_N_", '_' % yn[1] % '_'}))
            return 6;
    } while(! rstart);

    uchar mthd;

    switch(rmode) {
    case 1:
        mthd = 1;
        prun = tr("Restoring the full system");
        break;
    case 2:
        mthd = 2;
        prun = tr("Restoring the system files");
        break;
    case 3:
        mthd = 3;
        prun = tr("Restoring users configuration files");
        break;
    case 4:
        mthd = 5;
        prun = tr("Restoring users configuration files");
    }

    QTimer::singleShot(0, this, SLOT(progress()));
    ptimer->start();
    bool sfstab(fsave == 1);
    sb::srestore(mthd, nullptr, sb::sdir[1] % '/' % cpoint % '_' % pname, nullptr, sfstab);

    if(greinst == 1)
    {
        sb::exec("update-grub", nullptr, true);
        QStr mntdev, mnts(sb::fload("/proc/self/mounts"));
        QTS in(&mnts, QIODevice::ReadOnly);

        while(! in.atEnd())
        {
            QStr cline(in.readLine());

            if(cline.contains(" /boot "))
            {
                mntdev = sb::left(cline, sb::instr(cline, " ") - 1);
                break;
            }
            else if(cline.contains(" / "))
                mntdev = sb::left(cline, sb::instr(cline, " ") - 1);
        }

        if(sb::exec("grub-install --force " % sb::left(mntdev, 8), nullptr, true) > 0)
        {
            ptimer->stop();
            return 7;
        }
    }

    ptimer->stop();
    clear();
    mvprintw(0, COLS / 2 - 6 - tr("basic restore UI").length() / 2, QStr("Systemback " % tr("basic restore UI")).toStdString().c_str());
    attron(COLOR_PAIR(1));

    switch(rmode) {
    case 1:
        printw(QStr("\n\n " % tr("Full system restoration is completed.")).toStdString().c_str());
        break;
    case 2:
        printw(QStr("\n\n " % tr("System files restoration are completed.")).toStdString().c_str());
        break;
    case 3:
        printw(QStr("\n\n " % tr("Users configuration files full restoration are completed.")).toStdString().c_str());
        break;
    case 4:
        printw(QStr("\n\n " % tr("Users configuration files restoration are completed.")).toStdString().c_str());
    }

    attron(COLOR_PAIR(3));
    printw(QStr("\n\n " % (rmode < 3 ? tr("Press 'ENTER' key to reboot computer, or 'Q' to quit.") : tr("Press 'ENTER' key to quit."))).toStdString().c_str());
    attron(COLOR_PAIR(2));
    mvprintw(LINES - 1, COLS - 13, "Kendek, GPLv3");
    refresh();

    for(;;)
        switch(getch()) {
        case '\n':
            if(rmode < 3) sb::exec(sb::execsrch("reboot") ? "reboot" : "systemctl reboot", nullptr, false, true);
            return 0;
        case 'q':
        case 'Q':
            if(rmode < 3) return 0;
        }
}

void systemback::progress()
{
    for(uchar a(0) ; a < 4 ; ++a)
    {
        if(sb::like(prun, {'_' % tr("Creating restore point") % '_', '_' % tr("Restoring the full system") % '_', '_' % tr("Restoring the system files") % '_', '_' % tr("Restoring users configuration files") % '_'}))
        {
            schar cbperc(sb::mid(pbar, 3, sb::instr(pbar, "%") - 1).toUShort()), cperc(sb::Progress);

            if(cperc == -1)
            {
                if(pbar == " (?%)")
                    pbar = " ( %)";
                else if(pbar.isEmpty() || pbar == " ( %)")
                    pbar = " (?%)";
            }
            else if(cperc < 100)
            {
                if(cbperc < cperc || (cbperc == 0 && pbar != "(0%)"))
                    pbar = " (" % QStr::number(cperc) % "%)";
                else if(cperc == 99 && cbperc == 99)
                    pbar = " (100%)";
            }
            else if(cbperc < 100)
                pbar = " (100%)";
        }
        else if(! pbar.isEmpty())
            pbar.clear();

        if(! ptimer->isActive()) return;
        clear();
        attron(COLOR_PAIR(2));
        mvprintw(0, COLS / 2 - 6 - tr("basic restore UI").length() / 2, QStr("Systemback " % tr("basic restore UI")).toStdString().c_str());
        attron(COLOR_PAIR(1));
        mvprintw(LINES / 2 - 1, COLS / 2 - (prun.length() + pbar.length() + 4) / 2, QStr(prun % pbar % ' ' % (a == 0 ? "   " : a == 1 ? ".  " : a == 2 ? ".. " : "...")).toStdString().c_str());
        attron(COLOR_PAIR(2));
        mvprintw(LINES - 1, COLS - 13, "Kendek, GPLv3");
        refresh();
        if(a < 3) sb::delay(500);
    }
}
