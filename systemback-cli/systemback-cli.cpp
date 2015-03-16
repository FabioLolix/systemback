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

#include "systemback-cli.hpp"
#include <QCoreApplication>
#include <QTextStream>
#include <QDateTime>
#include <QDir>
#include <ncursesw/ncurses.h>
#include <unistd.h>

#ifdef timeout
#undef timeout
#endif

#ifdef instr
#undef instr
#endif

systemback::systemback()
{
    ptimer = nullptr;
    QStr yns(tr("(Y/N)"));
    yn[0] = yns.at(1);
    yn[1] = yns.at(3);
}

systemback::~systemback()
{
    if(ptimer) delete ptimer;
}

void systemback::main()
{
    auto help([] {
            return tr("Usage: systemback-cli [option]\n\n"
                " Options:\n\n"
                "  -n, --newbackup          create a new restore point\n\n"
                "  -s, --storagedir <path>  get or set restore points storage directory path\n\n"
                "  -u, --upgrade            upgrade current system\n"
                "                           remove unnecessary files and packages\n\n"
                "  -v, --version            output Systemback version number\n\n"
                "  -h, --help               show this help");
        });

    uchar rv([&] {
            if(sb::like(qApp->arguments().value(1), {"_-h_", "_--help_"}))
                sb::print("\n " % help() % "\n\n");
            else if(sb::like(qApp->arguments().value(1), {"_-v_", "_--version_"}))
                sb::print("\n " % sb::appver() % "\n\n");
            else if(sb::like(qApp->arguments().value(1), {"_-u_", "_--upgrade_"}))
            {
                sb::unlock(sb::Dpkglock);
                sb::supgrade({tr("An error occurred while upgrading the system!"), tr("Restart upgrade ...")});
            }
            else
                return getuid() + getgid() > 0 ? 2
                    : ! sb::lock(sb::Sblock) ? 3
                    : ! sb::lock(sb::Dpkglock) ? 4
                    : [&] {
                            auto startui([this](bool crtrpt = false) -> uchar {
                                    { int pgid(getpgrp());
                                    if(pgid == -1 || ! sb::like(pgid, {tcgetpgrp(STDIN_FILENO), tcgetpgrp(STDOUT_FILENO)}, true)) return 255; }
                                    initscr();

                                    uchar rv(! has_colors() ? 11
                                        : LINES < 24 || COLS < 80 ? 12
                                        : [crtrpt, this]() -> uchar {
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
                                                if(! crtrpt) return clistart();
                                                sb::pupgrade();
                                                return newrestorepoint() ? 0 : sb::dfree(sb::sdir[1]) < 104857600 ? 8 : 9;
                                            }());

                                    endwin();
                                    return rv;
                                });

                            return qApp->arguments().count() == 1 ? startui()
                                : sb::like(qApp->arguments().value(1), {"_-n_", "_--newrestorepoint_"}) ? [&] { return ! sb::isdir(sb::sdir[1]) || ! sb::access(sb::sdir[1], sb::Write) ? 10 : startui(true); }()
                                : sb::like(qApp->arguments().value(1), {"_-s_", "_--storagedir_"}) ? storagedir() : 1;
                        }();

            return 0;
        }());

    if(! sb::like(rv, {0, 255})) sb::error("\n " % [=]() -> QStr {
            switch(rv) {
            case 1:
                return help();
            case 2:
                return tr("Root privileges are required for running Systemback!");
            case 3:
                return tr("Another Systemback process is currently running, please wait until it\n finishes.");
            case 4:
                return tr("Unable to get exclusive lock!") % "\n\n " % tr("First, close all package manager.");
            case 5:
                return tr("The specified storage directory path has not been set!");
            case 6:
                return tr("Restoration is aborted!");
            case 7:
                return tr("Restoration is completed, but an error occurred while reinstalling GRUB!");
            case 8:
                return tr("Restore point creation is aborted!") % "\n\n " % tr("Not enough free disk space to complete the process.");
            case 9:
                return tr("Restore point creation is aborted!") % "\n\n " % tr("There has been critical changes in the file system during this operation.");
            case 10:
                return tr("Restore points storage directory is not available or not writable!");
            case 11:
                return tr("This stupid terminal does not support color!");
            case 12:
                return tr("This terminal is too small!") % " (< 80x24)";
            default:
                return tr("Restore point deletion is aborted!") % "\n\n " % tr("An error occurred while during the process.");
            }
        }() % "\n\n");

    qApp->exit(rv);
}

uchar systemback::clistart()
{
    mvprintw(0, COLS / 2 - 6 - tr("basic restore UI").length() / 2, chr(("Systemback " % tr("basic restore UI"))));
    attron(COLOR_PAIR(1));
    printw(chr(("\n\n " % tr("Available restore point(s):") % "\n\n")));
    sb::pupgrade();
    attron(COLOR_PAIR(3));
    if(! sb::pnames[0].isEmpty()) printw(chr(("  1 ─ " % sb::left(sb::pnames[0], COLS - 7) % '\n')));
    if(! sb::pnames[1].isEmpty()) printw(chr(("  2 ─ " % sb::left(sb::pnames[1], COLS - 7) % '\n')));
    if(sb::pnumber == 3) attron(COLOR_PAIR(5));
    if(! sb::pnames[2].isEmpty()) printw(chr(("  3 ─ " % sb::left(sb::pnames[2], COLS - 7) % '\n')));
    if(sb::pnumber == 4) attron(COLOR_PAIR(5));
    if(! sb::pnames[3].isEmpty()) printw(chr(("  4 ─ " % sb::left(sb::pnames[3], COLS - 7) % '\n')));
    if(sb::pnumber == 5) attron(COLOR_PAIR(5));
    if(! sb::pnames[4].isEmpty()) printw(chr(("  5 ─ " % sb::left(sb::pnames[4], COLS - 7) % '\n')));
    if(sb::pnumber == 6) attron(COLOR_PAIR(5));
    if(! sb::pnames[5].isEmpty()) printw(chr(("  6 ─ " % sb::left(sb::pnames[5], COLS - 7) % '\n')));
    if(sb::pnumber == 7) attron(COLOR_PAIR(5));
    if(! sb::pnames[6].isEmpty()) printw(chr(("  7 ─ " % sb::left(sb::pnames[6], COLS - 7) % '\n')));
    if(sb::pnumber == 8) attron(COLOR_PAIR(5));
    if(! sb::pnames[7].isEmpty()) printw(chr(("  8 ─ " % sb::left(sb::pnames[7], COLS - 7) % '\n')));
    if(sb::pnumber == 9) attron(COLOR_PAIR(5));
    if(! sb::pnames[8].isEmpty()) printw(chr(("  9 ─ " % sb::left(sb::pnames[8], COLS - 7) % '\n')));
    if(sb::pnumber == 10) attron(COLOR_PAIR(5));
    if(! sb::pnames[9].isEmpty()) printw(chr(("  A ─ " % sb::left(sb::pnames[9], COLS - 7) % '\n')));
    attron(COLOR_PAIR(3));
    if(! sb::pnames[10].isEmpty()) printw(chr(("  B ─ " % sb::left(sb::pnames[10], COLS - 7) % '\n')));
    if(! sb::pnames[11].isEmpty()) printw(chr(("  C ─ " % sb::left(sb::pnames[11], COLS - 7) % '\n')));
    if(! sb::pnames[12].isEmpty()) printw(chr(("  D ─ " % sb::left(sb::pnames[12], COLS - 7) % '\n')));
    if(! sb::pnames[13].isEmpty()) printw(chr(("  E ─ " % sb::left(sb::pnames[13], COLS - 7) % '\n')));
    if(! sb::pnames[14].isEmpty()) printw(chr(("  F ─ " % sb::left(sb::pnames[14], COLS - 7) % '\n')));
    printw(chr(("\n G ─ " % tr("Create new") % "\n Q ─ " % tr("Quit") % '\n')));
    attron(COLOR_PAIR(2));
    mvprintw(LINES - 1, COLS - 13, "Kendek, GPLv3");
    refresh();
    if(! pname.isEmpty()) pname.clear();
    if(sb::Progress != -1) sb::Progress = -1;

    do {
        int gtch(getch());

        switch(gtch) {
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
        {
            QStr cstr(gtch);
            cpoint = "S0" % cstr;
            uchar num(cstr.toUShort() - 1);
            if(! sb::pnames[num].isEmpty()) pname = sb::pnames[num];
            break;
        }
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
    } while(pname.isEmpty());

    clear();
    mvprintw(0, COLS / 2 - 6 - tr("basic restore UI").length() / 2, chr(("Systemback " % tr("basic restore UI"))));
    attron(COLOR_PAIR(1));
    printw(chr(("\n\n " % tr("Selected restore point:"))));
    attron(COLOR_PAIR(4));
    printw(chr(("\n\n  " % sb::left(pname, COLS - 3))));
    attron(COLOR_PAIR(3));
    printw(chr(("\n\n 1 ─ " % tr("Delete") % "\n 2 ─ " % tr("System restore") % " ▸\n B ─ ◂ " % tr("Back"))));
    attron(COLOR_PAIR(2));
    mvprintw(LINES - 1, COLS - 13, "Kendek, GPLv3");
    refresh();

    forever
        switch(getch()) {
        case '1':
        {
            prun = tr("Deleting restore point");
            progress(Start);

            if(! QFile::rename(sb::sdir[1] % '/' % cpoint % '_' % pname, sb::sdir[1] % "/.DELETED_" % pname) || ! sb::remove(sb::sdir[1] % "/.DELETED_" % pname))
            {
                progress(Stop);
                return 13;
            }

            emptycache();
            progress(Stop);
            clear();
            return clistart();
        }
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
        QStr ndir;

        {
            QStr cpath, idir(qApp->arguments().value(2));

            if(qApp->arguments().count() > 3)
                for(uchar a(3) ; a < qApp->arguments().count() ; ++a) idir.append(' ' % qApp->arguments().value(a));

            QSL excl{"*/Systemback_", "*/Systemback/*", "*/_", "_/bin_", "_/bin/*", "_/boot_", "_/boot/*", "_/cdrom_", "_/cdrom/*", "_/dev_", "_/dev/*", "_/etc_", "_/etc/*", "_/lib_", "_/lib/*", "_/lib32_", "_/lib32/*", "_/lib64_", "_/lib64/*", "_/opt_", "_/opt/*", "_/proc_", "_/proc/*", "_/root_", "_/root/*", "_/run_", "_/run/*", "_/sbin_", "_/sbin/*", "_/selinux_", "_/selinux/*", "_/srv_", "_/sys/*", "_/tmp_", "_/tmp/*", "_/usr_", "_/usr/*", "_/var_", "_/var/*"};
            if(sb::like((ndir = QDir::cleanPath(idir)), excl) || sb::like((cpath = QDir(idir).canonicalPath()), excl) || sb::like(sb::fload("/etc/passwd"), {"*:" % idir % ":*","*:" % ndir % ":*", "*:" % cpath % ":*"}) || ! sb::islnxfs(cpath)) return 5;
        }

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

            sb::sdir[0] = ndir, sb::sdir[1] = sb::sdir[0] % "/Systemback";
            sb::cfgwrite();
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

void systemback::emptycache()
{
    prun = sb::ecache == sb::True ? tr("Emptying cache") : tr("Flushing filesystem buffers");
    sb::fssync();
    if(sb::ecache == sb::True) sb::crtfile("/proc/sys/vm/drop_caches", "3");
}

bool systemback::newrestorepoint()
{
    goto start;
error:
    progress(Stop);
    return false;
start:
    progress(Start);

    for(cQStr &item : QDir(sb::sdir[1]).entryList(QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot))
        if(sb::like(item, {"_.DELETED_*", "_.S00_*"}))
        {
            if(prun != tr("Deleting incomplete restore point")) prun = tr("Deleting incomplete restore point");
            if(! sb::remove(sb::sdir[1] % '/' % item)) goto error;
        }

    for(uchar a(9) ; a > 1 ; --a)
        if(! sb::pnames[a].isEmpty() && (a == 9 || a > 2 ? sb::pnumber < a + 2 : sb::pnumber == 3))
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
    emptycache();
    progress(Stop);
    return true;
}

uchar systemback::restore()
{
    mvprintw(0, COLS / 2 - 6 - tr("basic restore UI").length() / 2, chr(("Systemback " % tr("basic restore UI"))));
    attron(COLOR_PAIR(1));
    printw(chr(("\n\n " % tr("Restore with the following restore point:"))));
    attron(COLOR_PAIR(4));
    printw(chr(("\n\n  " % pname)));
    attron(COLOR_PAIR(1));
    printw(chr(("\n\n " % tr("Restore with the following restore method:"))));
    attron(COLOR_PAIR(3));
    printw(chr(("\n\n  1 ─ " % tr("Full restore") % "\n  2 ─ " % tr("System files restore"))));
    attron(COLOR_PAIR(1));
    printw(chr(("\n\n  " % tr("Users configuration files restore"))));
    attron(COLOR_PAIR(3));
    printw(chr(("\n\n   3 ─ " % tr("Complete configuration files restore") % "\n   4 ─ " % tr("Keep newly installed configuration files") % "\n\n C ─ " % tr("Cancel"))));
    attron(COLOR_PAIR(2));
    mvprintw(LINES - 1, COLS - 13, "Kendek, GPLv3");
    refresh();
    uchar rmode(0);

    do {
        int gtch(getch());

        switch(gtch) {
        case 'c':
        case 'C':
            clear();
            return clistart();
        case '1':
        case '2':
        case '3':
        case '4':
            rmode = QStr(gtch).toUShort();
        }
    } while(rmode == 0);

    clear();
    mvprintw(0, COLS / 2 - 6 - tr("basic restore UI").length() / 2, chr(("Systemback " % tr("basic restore UI"))));
    attron(COLOR_PAIR(1));
    printw(chr(("\n\n " % tr("Restore with the following restore point:"))));
    attron(COLOR_PAIR(4));
    printw(chr(("\n\n  " % pname)));
    attron(COLOR_PAIR(1));
    printw(chr(("\n\n " % tr("Restore with the following restore method:"))));
    attron(COLOR_PAIR(4));

    printw(chr(("\n\n  " % [rmode] {
            switch(rmode) {
            case 1:
                return tr("Full restore");
            case 2:
                return tr("System files restore");
            case 3:
                return tr("Complete configuration files restore");
            default:
                return tr("Configuration files restore");
            }
        }())));

    attron(COLOR_PAIR(3));
    uchar fsave(0), greinst(0);

    if(rmode < 3)
    {
        if(sb::isfile("/etc/fstab"))
        {
            printw(chr(("\n\n " % tr("You want to keep the current fstab file?") % ' ' % tr("(Y/N)"))));
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
            mvprintw(0, COLS / 2 - 6 - tr("basic restore UI").length() / 2, chr(("Systemback " % tr("basic restore UI"))));
            attron(COLOR_PAIR(1));
            printw(chr(("\n\n " % tr("Restore with the following restore point:"))));
            attron(COLOR_PAIR(4));
            printw(chr(("\n\n  " % pname)));
            attron(COLOR_PAIR(1));
            printw(chr(("\n\n " % tr("Restore with the following restore method:"))));
            attron(COLOR_PAIR(4));
            printw(chr(("\n\n  " % (rmode == 1 ? tr("Full restore") : tr("System files restore")) % "\n\n " % tr("You want to keep the current fstab file?") % ' ' % tr("(Y/N)") % ' ' % yn[fsave == 1 ? 0 : 1])));
            attron(COLOR_PAIR(3));

            if(sb::execsrch("update-grub2", sb::sdir[1] % '/' % cpoint % '_' % pname))
            {
                printw(chr(("\n\n " % tr("Reinstall the GRUB 2 bootloader?") % ' ' % tr("(Y/N)"))));
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
                mvprintw(0, COLS / 2 - 6 - tr("basic restore UI").length() / 2, chr(("Systemback " % tr("basic restore UI"))));
                attron(COLOR_PAIR(1));
                printw(chr(("\n\n " % tr("Restore with the following restore point:"))));
                attron(COLOR_PAIR(4));
                printw(chr(("\n\n  " % pname)));
                attron(COLOR_PAIR(1));
                printw(chr(("\n\n " % tr("Restore with the following restore method:"))));
                attron(COLOR_PAIR(4));
                printw(chr(("\n\n  " % (rmode == 1 ? tr("Full restore") : tr("System files restore")) % "\n\n " % tr("You want to keep the current fstab file?") % ' ' % tr("(Y/N)") % ' ' % yn[fsave == 1 ? 0 : 1] % "\n\n " % tr("Reinstall the GRUB 2 bootloader?") % ' ' % tr("(Y/N)") % ' ' % yn[greinst == 1 ? 0 : 1])));
            }
        }
        else if(sb::execsrch("update-grub2", sb::sdir[1] % '/' % cpoint % '_' % pname))
        {
            printw(chr(("\n\n " % tr("Reinstall the GRUB 2 bootloader?") % ' ' % tr("(Y/N)"))));
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
            mvprintw(0, COLS / 2 - 6 - tr("basic restore UI").length() / 2, chr(("Systemback " % tr("basic restore UI"))));
            attron(COLOR_PAIR(1));
            printw(chr(("\n\n " % tr("Restore with the following restore point:"))));
            attron(COLOR_PAIR(4));
            printw(chr(("\n\n  " % pname)));
            attron(COLOR_PAIR(1));
            printw(chr(("\n\n " % tr("Restore with the following restore method:"))));
            attron(COLOR_PAIR(4));
            printw(chr(("\n\n  " % (rmode == 1 ? tr("Full restore") : tr("System files restore")) % "\n\n " % tr("Reinstall the GRUB 2 bootloader?") % ' ' % tr("(Y/N)") % ' ' % yn[greinst == 1 ? 0 : 1])));
        }
    }

    attron(COLOR_PAIR(3));
    printw(chr(("\n\n " % tr("Start the restore?") % ' ' % tr("(Y/N)"))));
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

    uchar mthd([rmode, this] {
            switch(rmode) {
            case 1:
                prun = tr("Restoring the full system");
                return 1;
            case 2:
                prun = tr("Restoring the system files");
                return 2;
            case 3:
                prun = tr("Restoring users configuration files");
                return 3;
            default:
                prun = tr("Restoring users configuration files");
                return 4;
            }
        }());

    progress(Start);
    bool sfstab(fsave == 1);
    sb::srestore(mthd, nullptr, sb::sdir[1] % '/' % cpoint % '_' % pname, nullptr, sfstab);

    if(greinst == 1)
        if(sb::exec("sh -c \"update-grub ; grub-install --force " % sb::gdetect() % '\"', nullptr, sb::Silent) > 0)
        {
            progress(Stop);
            return 7;
        }

    progress(Stop);
    clear();
    mvprintw(0, COLS / 2 - 6 - tr("basic restore UI").length() / 2, chr(("Systemback " % tr("basic restore UI"))));
    attron(COLOR_PAIR(1));

    printw(chr(("\n\n " % [rmode] {
            switch(rmode) {
            case 1:
                return tr("Full system restoration is completed.");
            case 2:
                return tr("System files restoration are completed.");
            case 3:
                return tr("Users configuration files full restoration are completed.");
            default:
                return tr("Users configuration files restoration are completed.");
            }
        }())));

    attron(COLOR_PAIR(3));
    printw(chr(("\n\n " % (rmode < 3 ? tr("Press 'ENTER' key to reboot computer, or 'Q' to quit.") : tr("Press 'ENTER' key to quit.")))));
    attron(COLOR_PAIR(2));
    mvprintw(LINES - 1, COLS - 13, "Kendek, GPLv3");
    refresh();

    forever
        switch(getch()) {
        case '\n':
            if(rmode < 3) sb::exec(sb::execsrch("reboot") ? "reboot" : "systemctl reboot", nullptr, sb::Bckgrnd);
            return 0;
        case 'q':
        case 'Q':
            if(rmode < 3) return 0;
        }
}

void systemback::progress(uchar status)
{
    switch(status) {
    case Start:
        connect((ptimer = new QTimer), SIGNAL(timeout()), this, SLOT(progress()));
        QTimer::singleShot(0, this, SLOT(progress()));
        ptimer->start(2000);
        break;
    case Inprog:
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
                    else if(sb::like(99, {cperc, cbperc}, true))
                        pbar = " (100%)";
                }
                else if(cbperc < 100)
                    pbar = " (100%)";
            }
            else if(! pbar.isEmpty())
                pbar.clear();

            if(! ptimer) return;
            clear();
            attron(COLOR_PAIR(2));
            mvprintw(0, COLS / 2 - 6 - tr("basic restore UI").length() / 2, chr(("Systemback " % tr("basic restore UI"))));
            attron(COLOR_PAIR(1));
            mvprintw(LINES / 2 - 1, COLS / 2 - (prun.length() + pbar.length() + 4) / 2, chr((prun % pbar % ' ' % (a == 0 ? "   " : a == 1 ? ".  " : a == 2 ? ".. " : "..."))));
            attron(COLOR_PAIR(2));
            mvprintw(LINES - 1, COLS - 13, "Kendek, GPLv3");
            refresh();
            if(a < 3) sb::delay(500);
        }

        break;
    case Stop:
        delete ptimer;
        ptimer = nullptr;
        prun.clear();
        if(! pbar.isEmpty()) pbar.clear();
        if(sb::Progress != -1) sb::Progress = -1;
    }
}
