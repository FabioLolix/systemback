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

#include "sblib.hpp"
#include <QCoreApplication>
#include <QProcess>
#include <QTime>
#include <QDir>
#include <parted/parted.h>
#include <sys/resource.h>
#include <sys/sendfile.h>
#include <blkid/blkid.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <linux/fs.h>
#include <dirent.h>
#include <utime.h>
#include <fcntl.h>

#ifdef C_MNT_LIB
#include "libmount.hpp"
#else
#include <libmount/libmount.h>
#endif

#ifdef bool
#undef bool
#endif

sb sb::SBThrd;
QSL *sb::ThrdSlst;
QStr sb::ThrdStr[3], sb::ThrdDbg, sb::sdir[3], sb::schdlr[2], sb::pnames[15], sb::lang, sb::style, sb::wsclng;
ullong sb::ThrdLng[]{0, 0};
int sb::sblock[3];
uchar sb::ThrdType, sb::ThrdChr, sb::pnumber(0), sb::ismpnt(sb::Empty), sb::schdle[]{sb::Empty, sb::Empty, sb::Empty, sb::Empty, sb::Empty, sb::Empty}, sb::waot(sb::Empty), sb::incrmtl(sb::Empty), sb::xzcmpr(sb::Empty), sb::autoiso(sb::Empty), sb::ecache(sb::Empty);
schar sb::Progress(-1);
bool sb::ThrdBool, sb::ExecKill(true), sb::ThrdKill(true), sb::ThrdRslt;

sb::sb()
{
    qputenv("PATH", "/usr/lib/systemback:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin");
    setlocale(LC_ALL, "C.UTF-8");
    umask(0);
}

QTrn *sb::ldtltr()
{
    QTrn *tltr(new QTrn);
    cfgread();

    if(lang == "auto")
    {
        if(QLocale::system().name() != "en_EN") tltr->load(QLocale::system(), "systemback", "_", "/usr/share/systemback/lang");
    }
    else if(lang != "en_EN")
        tltr->load("systemback_" % lang, "/usr/share/systemback/lang");

    if(tltr->isEmpty())
        delete tltr;
    else
        return tltr;

    return nullptr;
}

void sb::print(cQStr &txt)
{
    QTS(stdout) << "\033[1m" % txt % "\033[0m";
}

void sb::error(cQStr &txt)
{
    QTS(stderr) << "\033[1;31m" % txt % "\033[0m";
}

QStr sb::appver()
{
    QFile file(":version");
    file.open(QIODevice::ReadOnly);
    QStr vrsn(qVersion());

    return file.readLine().trimmed() % "_Qt" % (vrsn == QT_VERSION_STR ? vrsn : vrsn % '(' % QT_VERSION_STR % ')') % '_' %
#ifdef __clang__
        "Clang" % QStr::number(__clang_major__) % '.' % QStr::number(__clang_minor__) % '.' % QStr::number(__clang_patchlevel__)
#elif defined(__INTEL_COMPILER) || ! defined(__GNUC__)
        "compiler?"
#elif defined(__GNUC__)
        "GCC" % QStr::number(__GNUC__) % '.' % QStr::number(__GNUC_MINOR__) % '.' % QStr::number(__GNUC_PATCHLEVEL__)
#endif
        % '_' %
#ifdef __amd64__
        "amd64";
#elif defined(__i386__)
        "i386";
#else
        "arch?";
#endif
}

bool sb::like(cQStr &txt, cQSL &lst, uchar mode)
{
    switch(mode) {
    case Norm:
        for(cQStr &stxt : lst)
            if(stxt.startsWith('*'))
            {
                if(stxt.endsWith('*'))
                {
                    if(txt.contains(stxt.mid(1, stxt.length() - 2))) return true;
                }
                else if(txt.endsWith(stxt.mid(1, stxt.length() - 2)))
                    return true;
            }
            else if(stxt.endsWith('*'))
            {
                if(txt.startsWith(stxt.mid(1, stxt.length() - 2))) return true;
            }
            else if(txt == stxt.mid(1, stxt.length() - 2))
                return true;

        return false;
    case All:
        for(cQStr &stxt : lst)
            if(stxt.startsWith('*'))
            {
                if(stxt.endsWith('*'))
                {
                    if(! txt.contains(stxt.mid(1, stxt.length() - 2))) return false;
                }
                else if(! txt.endsWith(stxt.mid(1, stxt.length() - 2)))
                    return false;
            }
            else if(stxt.endsWith('*'))
            {
                if(! txt.startsWith(stxt.mid(1, stxt.length() - 2))) return false;
            }
            else if(txt != stxt.mid(1, stxt.length() - 2))
                return false;

        return true;
    case Mixed:
    {
        QSL alst, nlst;

        for(cQStr &stxt : lst)
            switch(stxt.at(0).toLatin1()) {
            case '+':
                alst.append(right(stxt, -1));
                break;
            case '-':
                nlst.append(right(stxt, -1));
                break;
            default:
                return false;
            }

        return like(txt, alst, All) && like(txt, nlst);
    }
    default:
        return false;
    }
}

QStr sb::rndstr(uchar vlen)
{
    QStr val, chrs("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz./");
    val.reserve(vlen);
    uchar clen(vlen == 16 ? 64 : 62), num(255);
    qsrand(QTime::currentTime().msecsSinceStartOfDay());

    do {
        uchar prev(num);
        while((num = qrand() % clen) == prev);
        val.append(chrs.at(num));
    } while(val.length() < vlen);

    return val;
}

bool sb::access(cQStr &path, uchar mode)
{
    switch(mode) {
    case Read:
        return QFileInfo(path).isReadable();
    case Write:
        return QFileInfo(path).isWritable();
    case Exec:
        return QFileInfo(path).isExecutable();
    default:
        return false;
    }
}

QStr sb::fload(cQStr &path, bool ascnt)
{
    QBA ba;
    { QFile file(path);
    if(! file.open(QIODevice::ReadOnly)) return nullptr;
    ba = file.readAll(); }

    if(ascnt && ! ba.isEmpty())
    {
        QStr str;
        QTS in(&ba, QIODevice::ReadOnly);
        while(! in.atEnd()) str.prepend(in.readLine() % '\n');
        return str;
    }

    return ba;
}

QBA sb::fload(cQStr &path)
{
    QFile file(path);
    if(! file.open(QIODevice::ReadOnly)) return nullptr;
    return file.readAll();
}

bool sb::islnxfs(cQStr &path)
{
    QTemporaryFile file(path % "/.sbdirtestfile_" % rndstr());
    return file.open() && file.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner | QFile::ReadGroup | QFile::ExeGroup | QFile::ReadOther) && file.permissions() == 30548;
}

bool sb::crtfile(cQStr &path, cQStr &txt)
{
    uchar otp(stype(path));
    if(! like(otp, {Notexist, Isfile}) || ! isdir(left(path, rinstr(path, "/") - 1))) return false;
    QFile file(path);
    if(! file.open(QFile::WriteOnly | QFile::Truncate) || file.write(txt.toUtf8()) == -1) return false;
    file.flush();
    return otp == Isfile || file.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ReadGroup | QFile::ReadOther);
}

template<typename T1, typename T2> inline bool sb::crthlnk(const T1 &srclnk, const T2 &newlnk)
{
    return link(bstr(srclnk), bstr(newlnk)) == 0;
}

bool sb::lock(uchar type)
{
    return (sblock[type] = open([type] {
            switch(type) {
            case Sblock:
                return isdir("/run") ? "/run/systemback.lock" : "/var/run/systemback.lock";
            case Dpkglock:
                return "/var/lib/dpkg/lock";
            default:
                return isdir("/run") ? "/run/sbscheduler.lock" : "/var/run/sbscheduler.lock";
            }
        }(), O_RDWR | O_CREAT, 0644)) > -1 && lockf(sblock[type], F_TLOCK, 0) == 0;
}

void sb::unlock(uchar type)
{
    close(sblock[type]);
}

void sb::delay(ushort msec)
{
    QTime time;
    time.start();

    do {
        msleep(10);
        qApp->processEvents();
    } while(ushort(time.elapsed()) < msec);
}

void sb::thrdelay()
{
    while(SBThrd.isRunning())
    {
        msleep(10);
        qApp->processEvents();
    }
}

bool sb::cfgwrite(cQStr &file)
{
    return crtfile(file, "# Restore points settings\n#  storage_directory=<path>\n#  storage_dir_is_mount_point=[true/false]\n#  max_temporary_restore_points=[3-10]\n#  use_incremental_backup_method=[true/false]\n\n"
        "storage_directory=" % sdir[0] %
        "\nstorage_dir_is_mount_point=" % (ismpnt == True ? "true" : "false") %
        "\nmax_temporary_restore_points=" % QStr::number(pnumber) %
        "\nuse_incremental_backup_method=" % (incrmtl == True ? "true" : "false") %
        "\n\n\n# Live system settings\n#  working_directory=<path>\n#  use_xz_compressor=[true/false]\n#  auto_iso_images=[true/false]\n\n"
        "working_directory=" % sdir[2] %
        "\nuse_xz_compressor=" % (xzcmpr == True ? "true" : "false") %
        "\nauto_iso_images=" % (autoiso == True ? "true" : "false") %
        "\n\n\n# Scheduler settigns\n#  enabled=[true/false]\n#  schedule=[0-7]:[0-23]:[0-59]:[10-99]\n#  silent=[true/false]\n#  window_position=[topleft/topright/center/bottomleft/bottomright]\n#  disable_starting_for_users=[false/everyone/:<username,list>]\n\n"
        "enabled=" % (file.startsWith("/.") ? "false" : schdle[0] == True ? "true" : "false") %
        "\nschedule=" % QStr::number(schdle[1]) % ':' % QStr::number(schdle[2]) % ':' % QStr::number(schdle[3]) % ':' % QStr::number(schdle[4]) %
        "\nsilent=" % (schdle[5] == True ? "true" : "false") %
        "\nwindow_position=" % schdlr[0] %
        "\ndisable_starting_for_users=" % schdlr[1] %
        "\n\n\n# User interface settings\n#  language=[auto/<language_COUNTRY>]\n\n"
        "language=" % lang %
        "\n\n\n# Graphical user interface settings\n#  style=[auto/<name>]\n#  window_scaling_factor=[auto/1/1.5/2]\n#  always_on_top=[true/false]\n\n"
        "style=" % style %
        "\nwindow_scaling_factor=" % wsclng %
        "\nalways_on_top=" % (waot == True ? "true" : "false") %
        "\n\n\n# Host system settings\n#  disable_cache_emptying=[true/false]\n\n" %
        "disable_cache_emptying=" % (ecache == True ? "false" : "true") % '\n');
}

void sb::cfgread()
{
    if(isfile("/etc/systemback.conf"))
    {
        QFile file("/etc/systemback.conf");

        if(file.open(QIODevice::ReadOnly))
            while(! file.atEnd())
            {
                QStr cline(file.readLine().trimmed()), cval(right(cline, - instr(cline, "=")));

                if(! cval.isEmpty() && ! cval.startsWith('#'))
                {
                    if(cline.startsWith("storage_directory="))
                        sdir[0] = cval;
                    else if(cline.startsWith("storage_dir_is_mount_point="))
                    {
                        if(cval == "true")
                            ismpnt = True;
                        else if(cval == "false")
                            ismpnt = False;
                    }
                    else if(cline.startsWith("max_temporary_restore_points="))
                        pnumber = cval.toUShort();
                    else if(cline.startsWith("use_incremental_backup_method="))
                    {
                        if(cval == "true")
                            incrmtl = True;
                        else if(cval == "false")
                            incrmtl = False;
                    }
                    else if(cline.startsWith("working_directory="))
                        sdir[2] = cval;
                    else if(cline.startsWith("use_xz_compressor="))
                    {
                        if(cval == "true")
                            xzcmpr = True;
                        else if(cval == "false")
                            xzcmpr = False;
                    }
                    else if(cline.startsWith("auto_iso_images="))
                    {
                        if(cval == "true")
                            autoiso = True;
                        else if(cval == "false")
                            autoiso = False;
                    }
                    else if(cline.startsWith("enabled="))
                    {
                        if(cval == "true")
                            schdle[0] = True;
                        else if(cval == "false")
                            schdle[0] = False;
                    }
                    else if(cline.startsWith("schedule="))
                    {
                        QSL vals(cval.split(':'));

                        if(vals.count() == 4)
                            for(uchar a(0) ; a < 4 ; ++a)
                            {
                                bool ok;
                                uchar num(vals.at(a).toUShort(&ok));
                                if(ok) schdle[a + 1] = num;
                            }
                    }
                    else if(cline.startsWith("silent="))
                    {
                        if(cval == "true")
                            schdle[5] = True;
                        else if(cval == "false")
                            schdle[5] = False;
                    }
                    else if(cline.startsWith("window_position="))
                        schdlr[0] = cval;
                    else if(cline.startsWith("disable_starting_for_users="))
                        schdlr[1] = cval;
                    else if(cline.startsWith("language="))
                        lang = cval;
                    else if(cline.startsWith("style="))
                        style = cval;
                    else if(cline.startsWith("window_scaling_factor="))
                        wsclng = cval;
                    else if(cline.startsWith("always_on_top="))
                    {
                        if(cval == "true")
                            waot = True;
                        else if(cval == "false")
                            waot = False;
                    }
                    else if(cline.startsWith("disable_cache_emptying="))
                    {
                        if(cval == "true")
                            ecache = False;
                        else if(cval == "false")
                            ecache = True;
                    }
                }
            }
    }

    bool cfgupdt(false);

    if(sdir[0].isEmpty())
    {
        sdir[0] = "/home", cfgupdt = true;
        if(ismpnt != Empty) ismpnt = Empty;
        if(! isdir("/home/Systemback")) crtdir("/home/Systemback");
        if(! isfile("/home/Systemback/.sbschedule")) crtfile("/home/Systemback/.sbschedule");
    }
    else
    {
        if(! isdir(sdir[0] % "/Systemback") && isdir(sdir[0]) && (ismpnt != True || ! issmfs(sdir[0], sdir[0].count('/') == 1 ? "/" : left(sdir[0], rinstr(sdir[0], "/") - 1))) && crtdir(sdir[0] % "/Systemback")) crtfile(sdir[0] % "/Systemback/.sbschedule");
        QStr cpath(QDir::cleanPath(sdir[0]));
        if(sdir[0] != cpath) sdir[0] = cpath, cfgupdt = true;
    }

    if(sdir[2].isEmpty())
    {
        sdir[2] = "/home";
        if(! cfgupdt) cfgupdt = true;
    }
    else
    {
        QStr cpath(QDir::cleanPath(sdir[2]));

        if(sdir[2] != cpath)
        {
            sdir[2] = cpath;
            if(! cfgupdt) cfgupdt = true;
        }
    }

    if(ismpnt == Empty)
    {
        QStr pdir(sdir[0].count('/') == 1 ? "/" : left(sdir[0], rinstr(sdir[0], "/") - 1));
        ismpnt = sdir[0] != pdir && isdir(pdir) && ! issmfs(sdir[0], pdir);
        if(! cfgupdt) cfgupdt = true;
    }

    if(incrmtl == Empty)
    {
        incrmtl = True;
        if(! cfgupdt) cfgupdt = true;
    }

    if(xzcmpr == Empty)
    {
        xzcmpr = False;
        if(! cfgupdt) cfgupdt = true;
    }

    if(autoiso == Empty)
    {
        autoiso = False;
        if(! cfgupdt) cfgupdt = true;
    }

    if(schdle[0] == Empty)
    {
        schdle[0] = False;
        if(! cfgupdt) cfgupdt = true;
    }

    if(schdle[1] > 7 || schdle[2] > 23 || schdle[3] > 59 || schdle[4] < 10 || schdle[4] > 99)
    {
        schdle[1] = 1, schdle[2] = schdle[3] = 0, schdle[4] = 10;
        if(! cfgupdt) cfgupdt = true;
    }
    else if(schdle[3] < 30 && like(0, {schdle[1], schdle[2]}, true))
    {
        schdle[3] = 30;
        if(! cfgupdt) cfgupdt = true;
    }

    if(schdle[5] == Empty)
    {
        schdle[5] = False;
        if(! cfgupdt) cfgupdt = true;
    }

    if(! like(schdlr[0], {"_topleft_", "_topright_", "_center_", "_bottomleft_", "_bottomright_"}))
    {
        schdlr[0] = "topright";
        if(! cfgupdt) cfgupdt = true;
    }

    if(! like(schdlr[1], {"_false_", "_everyone_", "_:*"}))
    {
        schdlr[1] = "false";
        if(! cfgupdt) cfgupdt = true;
    }

    if(pnumber < 3 || pnumber > 10)
    {
        pnumber = 5;
        if(! cfgupdt) cfgupdt = true;
    }

    if(lang.isEmpty() || (lang != "auto" && (lang.length() != 5 || lang.at(2) != '_' || ! lang.at(0).isLower() || ! lang.at(1).isLower() || ! lang.at(3).isUpper() || ! lang.at(4).isUpper())))
    {
        lang = "auto";
        if(! cfgupdt) cfgupdt = true;
    }

    if(style.isEmpty() || style.contains(' '))
    {
        style = "auto";
        if(! cfgupdt) cfgupdt = true;
    }

    if(! like(wsclng, {"_auto_", "_1_", "_1.5_", "_2_"}))
    {
        wsclng = "auto";
        if(! cfgupdt) cfgupdt = true;
    }

    if(waot == Empty)
    {
        waot = False;
        if(! cfgupdt) cfgupdt = true;
    }

    if(ecache == Empty)
    {
        ecache = True;
        if(! cfgupdt) cfgupdt = true;
    }

    sdir[1] = sdir[0] % "/Systemback";
    if(cfgupdt) cfgwrite();
    if(! isfile("/etc/systemback.excludes")) crtfile("/etc/systemback.excludes");
}

bool sb::execsrch(cQStr &fname, cQStr &ppath)
{
    for(cQStr &path : qgetenv("PATH").split(':'))
    {
        QStr fpath(ppath % path % '/' % fname);
        if(isfile(fpath)) return access(fpath, Exec);
    }

    return false;
}

uchar sb::exec(cQStr &cmd, cQStr &envv, uchar flag)
{
    if(ExecKill) ExecKill = false;
    bool silent, bckgrnd;
    uchar rprcnt;

    if(flag == Noflag)
        silent = bckgrnd = false, rprcnt = 0;
    else
    {
        silent = flag != (flag & ~Silent), bckgrnd = flag != (flag & ~Bckgrnd);
        if((rprcnt = bckgrnd || flag == (flag & ~Prgrss) ? 0 : cmd.startsWith("mksquashfs") ? 1 : cmd.startsWith("genisoimage") ? 2 : cmd.startsWith("tar -cf") ? 3 : cmd.startsWith("tar -xf") ? 4 : 0) > 0) Progress = 0;
    }

    QProcess proc;

    if(! silent && rprcnt == 0)
    {
        proc.setProcessChannelMode(QProcess::ForwardedChannels);
        proc.setInputChannelMode(QProcess::ForwardedInputChannel);
    }

    if(! envv.isEmpty())
    {
        QProcessEnvironment env(QProcessEnvironment::systemEnvironment());
        env.insert(left(envv, instr(envv, "=") - 1), right(envv, envv.length() - instr(envv, "=")));
        proc.setProcessEnvironment(env);
    }

    if(bckgrnd) return proc.startDetached(cmd) ? 0 : 255;
    proc.start(cmd, QProcess::ReadOnly);

    while(proc.state() == QProcess::Starting)
    {
        msleep(10);
        qApp->processEvents();
    }

    if(proc.error() == QProcess::FailedToStart) return 255;
    if(rprcnt == 1) setpriority(0, proc.pid(), 10);
    ullong inum(0);
    uchar cperc;

    while(proc.state() == QProcess::Running)
    {
        msleep(10);
        qApp->processEvents();
        if(ExecKill) proc.kill();

        switch(rprcnt) {
        case 1:
            if(Progress < (cperc = ((inum += proc.readAllStandardOutput().count('\n')) * 100 + 50) / ThrdLng[0])) Progress = cperc;
            QTS(stderr) << proc.readAllStandardError();

            if(dfree(sdir[2]) < 104857600)
            {
                proc.kill();
                return 255;
            }

            break;
        case 2:
        {
            QStr pout(proc.readAllStandardError());
            if(Progress < (cperc = mid(pout, rinstr(pout, "%") - 5, 2).toUShort())) Progress = cperc;
            break;
        }
        case 3:
            if(ThrdLng[0] == 0)
            {
                QBA itms;
                itms.reserve(10000);
                QUCL itmst;
                itmst.reserve(500);
                ushort lcnt(0);
                rodir(itms, itmst, sdir[2] % "/.sblivesystemcreate");

                if(! itmst.isEmpty())
                {
                    QTS in(&itms, QIODevice::ReadOnly);

                    while(! in.atEnd())
                    {
                        QStr item(in.readLine());
                        if(itmst.at(lcnt++) == Isfile) ThrdLng[0] += fsize(sdir[2] % "/.sblivesystemcreate/" % item);
                    }
                }
            }
            else if(isfile(ThrdStr[0]) && Progress < (cperc = (fsize(ThrdStr[0]) * 100 + 50) / ThrdLng[0]))
                Progress = cperc;

            break;
        case 4:
            QBA itms;
            itms.reserve(10000);
            QUCL itmst;
            itmst.reserve(500);
            ushort lcnt(0);
            rodir(itms, itmst, ThrdStr[0]);

            if(! itmst.isEmpty())
            {
                QTS in(&itms, QIODevice::ReadOnly);
                ullong size(0);

                while(! in.atEnd())
                {
                    QStr item(in.readLine());
                    if(itmst.at(lcnt++) == Isfile) size += fsize(ThrdStr[0] % '/' % item);
                }

                if(Progress < (cperc = (size * 100 + 50) / ThrdLng[0])) Progress = cperc;
            }
        }
    }

    return proc.exitStatus() == QProcess::CrashExit ? 255 : proc.exitCode();
}

uchar sb::exec(cQSL &cmds)
{
    for(cQStr &cmd : cmds)
    {
        uchar rv(exec(cmd));
        if(rv > 0) return rv;
    }

    return 0;
}

bool sb::mcheck(cQStr &item)
{
    QStr mnts(fload("/proc/self/mounts"));
    cQStr &itm(item.contains(' ') ? QStr(item).replace(" ", "\\040") : item);

    if(! itm.startsWith("/dev/"))
        return mnts.contains(' ' % (itm.endsWith('/') && itm.length() > 1 ? left(itm, -1) : itm % ' '));
    else if(QStr('\n' % mnts).contains('\n' % itm % (itm.length() > (item.contains("mmc") ? 12 : 8) ? " " : nullptr)))
        return true;
    else
    {
        blkid_probe pr(blkid_new_probe_from_filename(bstr(itm)));
        blkid_do_probe(pr);
        cchar *uuid(nullptr);
        blkid_probe_lookup_value(pr, "UUID", &uuid, nullptr);
        blkid_free_probe(pr);
        return uuid && QStr('\n' % mnts).contains("\n/dev/disk/by-uuid/" % QStr(uuid) % ' ');
    }
}

QStr sb::gdetect(cQStr rdir)
{
    QStr mnts(fload("/proc/self/mounts", true));
    QTS in(&mnts, QIODevice::ReadOnly);
    QSL incl[]{{"* " % rdir % " *", "* " % rdir % (rdir.endsWith('/') ? nullptr : "/") % "boot *"}, {"_/dev/sd*", "_/dev/hd*", "_/dev/vd*"}};

    while(! in.atEnd())
    {
        QStr cline(in.readLine());

        if(like(cline, incl[0]))
        {
            if(like(cline, incl[1]))
                return left(cline, 8);
            else if(cline.startsWith("/dev/mmcblk"))
                return left(cline, 12);
            else if(cline.startsWith("/dev/disk/by-uuid"))
            {
                QStr uid(right(left(cline, instr(cline, " ") - 1), -18));

                if(islink("/dev/disk/by-uuid/" % uid))
                {
                    QStr dev(QFile("/dev/disk/by-uuid/" % uid).readLink());
                    return left(dev, dev.contains("mmc") ? 12 : 8);
                }
            }

            break;
        }
    }

    return nullptr;
}

void sb::pupgrade()
{
    bool rerun;

    do {
        rerun = false;

        for(uchar a(0) ; a < 15 ; ++a)
            if(! pnames[a].isEmpty()) pnames[a].clear();

        if(isdir(sdir[1]) && access(sdir[1], Write))
        {
            for(cQStr &item : QDir(sdir[1]).entryList(QDir::Dirs | QDir::NoDotAndDotDot))
                if(! islink(sdir[1] % '/' % item) && ! item.contains(' '))
                {
                    QStr pre(left(item, 4));

                    if(pre.at(1).isDigit() && pre.at(2).isDigit() && pre.at(3) == '_')
                    {
                        if(pre.at(0) == 'S')
                        {
                            if(pre.at(1) == '0' || mid(pre, 2, 2) == "10") pnames[mid(pre, 2, 2).toUShort() - 1] = right(item, -4);
                        }
                        else if(pre.at(0) == 'H' && pre.at(1) == '0' && like(pre.at(2), {"_1_", "_2_", "_3_", "_4_", "_5_"}))
                            pnames[9 + mid(pre, 3, 1).toUShort()] = right(item, -4);
                    }
                }

            for(uchar a(14) ; a > 0 ; --a)
                if(a != 10 && ! pnames[a].isEmpty() && pnames[a - 1].isEmpty())
                {
                    QFile::rename(sdir[1] % (a > 10 ? QStr("/H0" % QStr::number(a - 9)) : (a < 9 ? "/S0" : "/S") % QStr::number(a + 1)) % '_' % pnames[a], sdir[1] % (a > 10 ? ("/H0" % QStr::number(a - 10)) : "/S0" % QStr::number(a)) % '_' % pnames[a]);
                    if(! rerun) rerun = true;
                }
        }
    } while(rerun);
}

void sb::supgrade(cQSL &estr)
{
    exec("apt-get update");

    forever
    {
        if(exec({"apt-get install -fym --force-yes", "dpkg --configure -a", "apt-get dist-upgrade --no-install-recommends -ym --force-yes", "apt-get autoremove --purge -y"}) == 0)
        {
            QStr rklist;

            {
                QSL dlst(QDir("/boot").entryList(QDir::Files, QDir::Reversed));

                for(cQStr &item : dlst)
                    if(item.startsWith("vmlinuz-"))
                    {
                        QStr vmlinuz(right(item, -8)), kernel(left(vmlinuz, instr(vmlinuz, "-") - 1)), kver(mid(vmlinuz, kernel.length() + 2, instr(vmlinuz, "-", kernel.length() + 2) - kernel.length() - 2));

                        if(isnum(kver) && vmlinuz.startsWith(kernel % '-' % kver % '-') && ! rklist.contains(kernel % '-' % kver % "-*"))
                        {
                            for(ushort a(1) ; a < 101 ; ++a)
                            {
                                QStr subk(kernel % '-' % QStr::number(kver.toUShort() - a));

                                for(cQStr &ritem : dlst)
                                    if(ritem.startsWith("vmlinuz-" % subk % '-') && ! rklist.contains(' ' % subk % "-*")) rklist.append(' ' % subk % "-*");
                            }
                        }
                    }
            }

            uchar cproc(rklist.isEmpty() ? 0 : exec("apt-get autoremove --purge " % rklist));

            if(like(cproc, {0, 1}))
            {
                {
                    QProcess proc;
                    proc.start("dpkg -l", QProcess::ReadOnly);

                    while(proc.state() == QProcess::Starting || proc.state() == QProcess::Running)
                    {
                        msleep(10);
                        qApp->processEvents();
                    }

                    QBA sout(proc.readAllStandardOutput());
                    QStr iplist;
                    QTS in(&sout, QIODevice::ReadOnly);

                    while(! in.atEnd())
                    {
                        QStr cline(in.readLine());
                        if(cline.startsWith("rc")) iplist.append(' ' % mid(cline, 5, instr(cline, " ", 5) - 5));
                    }

                    if(! iplist.isEmpty()) exec("dpkg --purge " % iplist);
                }

                exec("apt-get clean");

                {
                    QSL dlst(QDir("/var/cache/apt").entryList(QDir::Files));

                    for(uchar a(0) ; a < dlst.count() ; ++a)
                    {
                        cQStr &item(dlst.at(a));
                        if(item.contains(".bin.")) rmfile("/var/cache/apt/" % item);
                    }
                }

                for(cQStr &item : QDir("/lib/modules").entryList(QDir::Dirs | QDir::NoDotAndDotDot))
                    if(! exist("/boot/vmlinuz-" % item)) QDir("/lib/modules/" % item).removeRecursively();

                break;
            }
        }
        else
            exec("dpkg --configure -a");

        exec({"tput reset", "tput civis"});

        for(uchar a(3) ; a > 0 ; --a)
        {
            error("\n " % estr.at(0) % '\n');
            print("\n " % estr.at(1) % ' ' % QStr::number(a));
            sleep(1);
            exec("tput cup 0 0");
        }

        exec("tput reset");
    }
}

bool sb::copy(cQStr &srcfile, cQStr &newfile)
{
    if(! isfile(srcfile)) return false;
    ThrdType = Copy,
    ThrdStr[0] = srcfile,
    ThrdStr[1] = newfile;
    SBThrd.start();
    thrdelay();
    return ThrdRslt;
}

QStr sb::ruuid(cQStr &part)
{
    ThrdType = Ruuid,
    ThrdStr[0] = part;
    SBThrd.start();
    thrdelay();
    return ThrdStr[1];
}

bool sb::srestore(uchar mthd, cQStr &usr, cQStr &srcdir, cQStr &trgt, bool sfstab)
{
    ThrdType = Srestore,
    ThrdChr = mthd,
    ThrdStr[0] = usr,
    ThrdStr[1] = srcdir,
    ThrdStr[2] = trgt,
    ThrdBool = sfstab;
    SBThrd.start();
    thrdelay();
    return ThrdRslt;
}

bool sb::mkpart(cQStr &dev, ullong start, ullong len, uchar type)
{
    if(dev.length() > (dev.contains("mmc") ? 12 : 8) || stype(dev) != Isblock) return false;
    ThrdType = Mkpart,
    ThrdStr[0] = dev,
    ThrdLng[0] = start,
    ThrdLng[1] = len,
    ThrdChr = type;
    SBThrd.start();
    thrdelay();
    return ThrdRslt;
}

bool sb::mount(cQStr &dev, cQStr &mpoint, cQStr &moptns)
{
#ifdef C_MNT_LIB
    if(moptns == "loop") return exec("mount -o loop " % dev % ' ' % mpoint) == 0;
#endif
    ThrdType = Mount,
    ThrdStr[0] = dev,
    ThrdStr[1] = mpoint,
    ThrdStr[2] = moptns;
    SBThrd.start();
    thrdelay();
    return ThrdRslt;
}

bool sb::scopy(uchar mthd, cQStr &usr, cQStr &srcdir)
{
    ThrdType = Scopy,
    ThrdChr = mthd,
    ThrdStr[0] = usr,
    ThrdStr[1] = srcdir;
    SBThrd.start();
    thrdelay();
    return ThrdRslt;
}

bool sb::crtrpoint(cQStr &pname)
{
    ThrdType = Crtrpoint,
    ThrdStr[0] = sdir[1] % "/.S00_" % pname;
    SBThrd.start();
    thrdelay();
    return ThrdRslt;
}

bool sb::setpflag(cQStr &part, cQStr &flag)
{
    { bool ismmc(part.contains("mmc"));
    if(part.length() < (ismmc ? 14 : 9) || stype(part) != Isblock || stype(left(part, (ismmc ? 12 : 8))) != Isblock) return false; }
    ThrdType = Setpflag,
    ThrdStr[0] = part,
    ThrdStr[1] = flag;
    SBThrd.start();
    thrdelay();
    return ThrdRslt;
}

bool sb::lvprpr(bool iudata)
{
    ThrdType = Lvprpr,
    ThrdBool = iudata;
    SBThrd.start();
    thrdelay();
    return ThrdRslt;
}

bool sb::mkptable(cQStr &dev, cQStr &type)
{
    if(dev.length() > (dev.contains("mmc") ? 12 : 8) || stype(dev) != Isblock) return false;
    ThrdType = Mkptable,
    ThrdStr[0] = dev,
    ThrdStr[1] = type;
    SBThrd.start();
    thrdelay();
    return ThrdRslt;
}

bool sb::remove(cQStr &path)
{
    ThrdType = Remove,
    ThrdStr[0] = path;
    SBThrd.start();
    thrdelay();
    return ThrdRslt;
}

bool sb::umount(cQStr &dev)
{
    ThrdType = Umount,
    ThrdStr[0] = dev;
    SBThrd.start();
    thrdelay();
    return ThrdRslt;
}

void sb::readlvdevs(QSL &strlst)
{
    ThrdType = Readlvdevs,
    ThrdSlst = &strlst;
    SBThrd.start();
    thrdelay();
}

void sb::readprttns(QSL &strlst)
{
    ThrdType = Readprttns,
    ThrdSlst = &strlst;
    SBThrd.start();
    thrdelay();
}

void sb::fssync()
{
    ThrdType = Sync;
    SBThrd.start();
    thrdelay();
}

void sb::delpart(cQStr &part)
{
    if(stype(part) == Isblock)
    {
        ThrdType = Delpart,
        ThrdStr[0] = part;
        SBThrd.start();
        thrdelay();
    }
}

inline QStr sb::rlink(cQStr &path, ushort blen)
{
    char rpath[blen];
    short rlen(readlink(bstr(path), rpath, blen));
    return rlen > 0 ? QStr(rpath).left(rlen) : nullptr;
}

uchar sb::fcomp(cQStr &file1, cQStr &file2)
{
    struct stat fstat[2];

    return like(-1, {stat(bstr(file1), &fstat[0]), stat(bstr(file2), &fstat[1])}) ? 0
        : fstat[0].st_size == fstat[1].st_size && fstat[0].st_mtim.tv_sec == fstat[1].st_mtim.tv_sec ? fstat[0].st_mode == fstat[1].st_mode && fstat[0].st_uid == fstat[1].st_uid && fstat[0].st_gid == fstat[1].st_gid ? 2 : 1 : 0;
}

bool sb::cpertime(cQStr &srcitem, cQStr &newitem, bool skel)
{
    struct stat istat[2];
    if(stat(bstr(srcitem), &istat[0]) == -1) return false;
    bstr nitem(newitem);
    if(stat(nitem, &istat[1]) == -1) return false;

    if(skel)
    {
        struct stat ustat;
        if(stat(bstr(left(newitem, instr(newitem, "/", 21) - 1)), &ustat) == -1) return false;
        istat[0].st_uid = ustat.st_uid, istat[0].st_gid = ustat.st_gid;
    }

    if(istat[0].st_uid != istat[1].st_uid || istat[0].st_gid != istat[1].st_gid)
    {
        if(chown(nitem, istat[0].st_uid, istat[0].st_gid) == -1 || ((istat[0].st_mode != (istat[0].st_mode & ~(S_ISUID | S_ISGID)) || istat[0].st_mode != istat[1].st_mode) && chmod(nitem, istat[0].st_mode) == -1)) return false;
    }
    else if(istat[0].st_mode != istat[1].st_mode && chmod(nitem, istat[0].st_mode) == -1)
        return false;

    if(istat[0].st_atim.tv_sec != istat[1].st_atim.tv_sec || istat[0].st_mtim.tv_sec != istat[1].st_mtim.tv_sec)
    {
        utimbuf sitimes;
        sitimes.actime = istat[0].st_atim.tv_sec, sitimes.modtime = istat[0].st_mtim.tv_sec;
        if(utime(nitem, &sitimes) == -1) return false;
    }

    return true;
}

bool sb::cplink(cQStr &srclink, cQStr &newlink)
{
    struct stat sistat;
    if(lstat(bstr(srclink), &sistat) == -1 || ! S_ISLNK(sistat.st_mode)) return false;
    QStr path(rlink(srclink, sistat.st_size));
    bstr nlink(newlink);
    if(path.isEmpty() || symlink(bstr(path), nlink) == -1) return false;
    timeval sitimes[2];
    sitimes[0].tv_sec = sistat.st_atim.tv_sec, sitimes[1].tv_sec = sistat.st_mtim.tv_sec, sitimes[0].tv_usec = sitimes[1].tv_usec = 0;
    return lutimes(nlink, sitimes) == 0;
}

bool sb::cpfile(cQStr &srcfile, cQStr &newfile, bool skel)
{
    int src, dst;
    struct stat fstat;
    { bstr sfile(srcfile);
    if(stat(sfile, &fstat) == -1) return false;
    if((src = open(sfile, O_RDONLY | O_NOATIME)) == -1) return false; }
    bstr nfile(newfile);
    bool err;

    if(! (err = (dst = creat(nfile, fstat.st_mode)) == -1))
    {
        if(fstat.st_size > 0)
        {
            llong size(0);

            do {
                llong csize(size)
#ifdef __i386__
                    , rsize(fstat.st_size - size)
#endif
                    ;
                if((size += sendfile(dst, src, nullptr,
#ifdef __i386__
                    rsize < 2147483648 ? rsize : 2147483647
#else
                    fstat.st_size - size
#endif
                    )) <= csize) err = true;
            } while(! err && size < fstat.st_size);
        }

        close(dst);
    }

    close(src);
    if(err) return false;

    if(skel)
    {
        struct stat ustat;
        if(stat(bstr(left(newfile, instr(newfile, "/", 21) - 1)), &ustat) == -1) return false;
        fstat.st_uid = ustat.st_uid, fstat.st_gid = ustat.st_gid;
    }

    if(fstat.st_uid + fstat.st_gid > 0 && (chown(nfile, fstat.st_uid, fstat.st_gid) == -1 || (fstat.st_mode != (fstat.st_mode & ~(S_ISUID | S_ISGID)) && chmod(nfile, fstat.st_mode) == -1))) return false;
    utimbuf sftimes;
    sftimes.actime = fstat.st_atim.tv_sec, sftimes.modtime = fstat.st_mtim.tv_sec;
    return utime(nfile, &sftimes) == 0;
}

bool sb::cpdir(cQStr &srcdir, cQStr &newdir)
{
    struct stat dstat;
    if(stat(bstr(srcdir), &dstat) == -1 || ! S_ISDIR(dstat.st_mode)) return false;
    bstr ndir(newdir);
    if(mkdir(ndir, dstat.st_mode) == -1 || (dstat.st_uid + dstat.st_gid > 0 && (chown(ndir, dstat.st_uid, dstat.st_gid) == -1 || (dstat.st_mode != (dstat.st_mode & ~(S_ISUID | S_ISGID)) && chmod(ndir, dstat.st_mode) == -1)))) return false;
    utimbuf sdtimes;
    sdtimes.actime = dstat.st_atim.tv_sec, sdtimes.modtime = dstat.st_mtim.tv_sec;
    return utime(ndir, &sdtimes) == 0;
}

bool sb::exclcheck(cQSL &elist, cQStr &item)
{
    for(cQStr &excl : elist)
        if(excl.endsWith('/'))
        {
            if(item.startsWith(excl)) return true;
        }
        else if(excl.endsWith('*'))
        {
            if(item.startsWith(left(excl, -1))) return true;
        }
        else if(like(item, {'_' % excl % '_', '_' % excl % "/*"}))
            return true;

    return false;
}

bool sb::lcomp(cQStr &link1, cQStr &link2)
{
    struct stat istat[2];
    if(like(-1, {lstat(bstr(link1), &istat[0]), lstat(bstr(link2), &istat[1])}) || ! S_ISLNK(istat[0].st_mode) || ! S_ISLNK(istat[1].st_mode) || istat[0].st_mtim.tv_sec != istat[1].st_mtim.tv_sec) return false;
    QStr lnk(rlink(link1, istat[0].st_size));
    return ! lnk.isEmpty() && lnk == rlink(link2, istat[1].st_size);
}

bool sb::rodir(QBA &ba, QUCL &ucl, cQStr &path, bool hidden, uchar oplen)
{
    QStr prepath(ba.isEmpty() ? nullptr : QStr(right(path, - (oplen == 1 ? 1 : oplen + 1)) % '/'));
    DIR *dir(opendir(bstr(path)));
    dirent *ent;
    QSL dd{"_._", "_.._"};

    while(! ThrdKill && (ent = readdir(dir)))
    {
        QStr iname(ent->d_name);

        if(! like(iname, dd) && (! hidden || iname.startsWith('.')))
        {
            uchar type([&]() -> uchar {
                    switch(ent->d_type) {
                    case DT_LNK:
                        return Islink;
                    case DT_DIR:
                        return Isdir;
                    case DT_REG:
                        return Isfile;
                    case DT_UNKNOWN:
                        return stype(path % '/' % iname);
                    default:
                        return Excluded;
                    }
                }());

            switch(type) {
            case Islink:
            case Isfile:
                ucl.append(type);
                ba.append(prepath % iname % '\n');
                break;
            case Isdir:
                rodir(ba.append(prepath % iname % '\n'), ucl << Isdir, path % '/' % iname, false, (oplen == 0 ? path.length() : oplen));
            }
        }
    }

    closedir(dir);
    if(oplen == 0 && ! ThrdKill) ba.squeeze();
    return ! ThrdKill;
}

bool sb::rodir(QBA &ba, cQStr &path, uchar oplen)
{
    QStr prepath(ba.isEmpty() ? nullptr : QStr(right(path, - (oplen == 1 ? 1 : oplen + 1)) % '/'));
    DIR *dir(opendir(bstr(path)));
    dirent *ent;
    QSL dd{"_._", "_.._"};

    while(! ThrdKill && (ent = readdir(dir)))
    {
        QStr iname(ent->d_name);

        if(! like(iname, dd))
            switch([&]() -> uchar {
                    switch(ent->d_type) {
                    case DT_LNK:
                    case DT_REG:
                        return Included;
                    case DT_DIR:
                        return Isdir;
                    case DT_UNKNOWN:
                        return stype(path % '/' % iname);
                    default:
                        return Excluded;
                    }
                }()) {
            case Islink:
            case Isfile:
            case Included:
                ba.append(prepath % iname % '\n');
                break;
            case Isdir:
                rodir(ba.append(prepath % iname % '\n'), path % '/' % iname, (oplen == 0 ? path.length() : oplen));
            }
    }

    closedir(dir);
    if(oplen == 0 && ! ThrdKill) ba.squeeze();
    return ! ThrdKill;
}

bool sb::rodir(QUCL &ucl, cQStr &path, uchar oplen)
{
    DIR *dir(opendir(bstr(path)));
    dirent *ent;
    QSL dd{"_._", "_.._"};

    while(! ThrdKill && (ent = readdir(dir)))
    {
        QStr iname(ent->d_name);

        if(! like(iname, dd))
            switch([&]() -> uchar {
                    switch(ent->d_type) {
                    case DT_LNK:
                    case DT_REG:
                        return Included;
                    case DT_DIR:
                        return Isdir;
                    case DT_UNKNOWN:
                        return stype(path % '/' % iname);
                    default:
                        return Excluded;
                    }
                }()) {
            case Islink:
            case Isfile:
            case Included:
                ucl.append(0);
                break;
            case Isdir:
                rodir(ucl << 0, path % '/' % iname, (oplen == 0 ? path.length() : oplen));
            }
    }

    closedir(dir);
    return ! ThrdKill;
}

bool sb::odir(QBAL &balst, cQStr &path, bool hidden)
{
    balst.reserve(1000);
    DIR *dir(opendir(bstr(path)));
    dirent *ent;
    QSL dd{"_._", "_.._"};

    while(! ThrdKill && (ent = readdir(dir)))
    {
        QStr iname(ent->d_name);
        if(! like(iname, dd) && (! hidden || iname.startsWith('.'))) balst.append(QBA(ent->d_name));
    }

    closedir(dir);
    return ! ThrdKill;
}

bool sb::recrmdir(cQStr &path, bool slimit)
{
    DIR *dir(opendir(bstr(path)));
    dirent *ent;
    QSL dd{"_._", "_.._"};

    while(! ThrdKill && (ent = readdir(dir)))
    {
        QStr iname(ent->d_name);

        if(! like(iname, dd))
        {
            QStr fpath(path % '/' % iname);

            switch(ent->d_type) {
            case DT_UNKNOWN:
                switch(stype(fpath)) {
                case Isdir:
                    if(! recrmdir(fpath, slimit))
                    {
                        closedir(dir);
                        return false;
                    }

                    rmdir(bstr(fpath));
                    break;
                case Isfile:
                    if(slimit && QFile(fpath).size() > 8000000) break;
                default:
                    rmfile(fpath);
                }

                break;
            case DT_DIR:
                if(! recrmdir(fpath, slimit))
                {
                    closedir(dir);
                    return false;
                }

                rmdir(bstr(fpath));
                break;
            case DT_REG:
                if(slimit && QFile(fpath).size() > 8000000) break;
            default:
                rmfile(fpath);
            }
        }
    }

    closedir(dir);
    return ! ThrdKill && (rmdir(bstr(path)) == 0 || slimit);
}

void sb::run()
{
    if(ThrdKill) ThrdKill = false;
    if(! ThrdDbg.isEmpty()) ThrdDbg.clear();

    auto psalign([](ullong pstart, ushort ssize) -> ullong {
            if(pstart <= 1048576 / ssize) return 1048576 / ssize;
            ushort rem(pstart % (1048576 / ssize));
            return rem > 0 ? pstart + 1048576 / ssize - rem : pstart;
        });

    auto pealign([](ullong end, ushort ssize) -> ullong {
            ushort rem(end % (1048576 / ssize));
            return rem > 0 ? rem < (1048576 / ssize) - 1 ? end - rem - 1 : end : end - 1;
        });

    auto devsize([](cQStr &dev) -> ullong {
            ullong bsize;
            int odev;
            bool err;

            if(! (err = (odev = open(bstr(dev), O_RDONLY)) == -1))
            {
                if(ioctl(odev, BLKGETSIZE64, &bsize) == -1) err = true;
                close(odev);
            }

            return err ? 0 : bsize;
        });

    switch(ThrdType) {
    case Remove:
        ThrdRslt = [this] {
                switch(stype(ThrdStr[0])) {
                case Isdir:
                    return recrmdir(ThrdStr[0]);
                default:
                    return rmfile(ThrdStr[0]);
                }
            }();

        break;
    case Copy:
        ThrdRslt = cpfile(ThrdStr[0], ThrdStr[1]);
        break;
    case Sync:
        return sync();
    case Mount:
    {
        libmnt_context *mcxt(mnt_new_context());
        mnt_context_set_source(mcxt, bstr(ThrdStr[0]));
        mnt_context_set_target(mcxt, bstr(ThrdStr[1]));
        mnt_context_set_options(mcxt, ! ThrdStr[2].isEmpty() ? bstr(ThrdStr[2]).data : isdir(ThrdStr[0]) ? "bind" : "noatime");
        ThrdRslt = mnt_context_mount(mcxt) == 0;
        return mnt_free_context(mcxt);
    }
    case Umount:
    {
        libmnt_context *ucxt(mnt_new_context());
        mnt_context_set_target(ucxt, bstr(ThrdStr[0]));
        mnt_context_enable_force(ucxt, true);
        mnt_context_enable_lazy(ucxt, true);
#ifndef C_MNT_LIB
        mnt_context_enable_loopdel(ucxt, true);
#endif
        ThrdRslt = mnt_context_umount(ucxt) == 0;
        return mnt_free_context(ucxt);
    }
    case Readprttns:
    {
        ThrdSlst->reserve(25);
        QSL dlst{"_/dev/sd*", "_/dev/hd*", "_/dev/vd*", "_/dev/mmcblk*"};

        for(cQStr &spath : QDir("/dev").entryList(QDir::System))
        {
            QStr path("/dev/" % spath);

            if(like(path.length(), {8, 12}) && like(path, dlst) && devsize(path) > 536870911)
            {
                PedDevice *dev(ped_device_get(bstr(path)));
                PedDisk *dsk(ped_disk_new(dev));

                uchar type([&dsk] {
                        QStr name(dsk ? dsk->type->name : nullptr);
                        return name == "gpt" ? GPT : name == "msdos" ? MSDOS : Clear;
                    }());

                ThrdSlst->append(path % '\n' % QStr::number(dev->length * dev->sector_size) % '\n' % QStr::number(type));

                if(type == Clear)
                {
                    if(dsk)
                        goto next_1;
                    else
                        goto next_2;
                }

                {
                    PedPartition *prt(nullptr);
                    QLIL egeom;

                    while((prt = ped_disk_next_partition(dsk, prt)))
                        if(prt->geom.length >= 1048576 / dev->sector_size)
                        {
                            if(prt->num > 0)
                            {
                                QStr ppath(path % (path.length() == 12 ? "p" : nullptr) % QStr::number(prt->num));

                                if(stype(ppath) == Isblock)
                                {
                                    if(prt->type == PED_PARTITION_EXTENDED)
                                    {
                                        ThrdSlst->append(ppath % '\n' % QStr::number(prt->geom.length * dev->sector_size) % '\n' % QStr::number(Extended) % '\n' % QStr::number(prt->geom.start * dev->sector_size));
                                        egeom.append({prt->geom.start, prt->geom.end});
                                    }
                                    else
                                    {
                                        if(egeom.count() > 2)
                                        {
                                            ullong pstart(psalign(egeom.at(2), dev->sector_size));
                                            ThrdSlst->append(path % "?\n" % QStr::number((pealign(egeom.at(3), dev->sector_size) - pstart + 1) * dev->sector_size - (prt->type == PED_PARTITION_LOGICAL ? 2097152 : 1048576 - dev->sector_size)) % '\n' % QStr::number(Emptyspace) % '\n' % QStr::number(pstart * dev->sector_size + 1048576));
                                            for(uchar a(3) ; a > 1 ; --a) egeom.removeAt(a);
                                        }

                                        blkid_probe pr(blkid_new_probe_from_filename(bstr(ppath)));
                                        blkid_do_probe(pr);
                                        cchar *uuid(nullptr), *fstype("?"), *label(nullptr);

                                        if(blkid_probe_lookup_value(pr, "UUID", &uuid, nullptr) == 0)
                                        {
                                            blkid_probe_lookup_value(pr, "TYPE", &fstype, nullptr);
                                            blkid_probe_lookup_value(pr, "LABEL", &label, nullptr);
                                        }

                                        blkid_free_probe(pr);
                                        ThrdSlst->append(ppath % '\n' % QStr::number(prt->geom.length * dev->sector_size) % '\n' % QStr::number(prt->type == PED_PARTITION_LOGICAL ? Logical : Primary) % '\n' % QStr::number(prt->geom.start * dev->sector_size) % '\n' % fstype % '\n' % label % '\n' % uuid);
                                    }
                                }
                            }
                            else if(prt->type == PED_PARTITION_FREESPACE)
                            {
                                if(egeom.count() > 2)
                                {
                                    ullong pstart(psalign(egeom.at(2), dev->sector_size));
                                    ThrdSlst->append(path % "?\n" % QStr::number((pealign(egeom.at(3), dev->sector_size) - pstart + 1) * dev->sector_size - 1048576) % '\n' % QStr::number(Emptyspace) % '\n' % QStr::number(pstart * dev->sector_size + 1048576));
                                    egeom.clear();
                                }

                                llong fgeom[]{llong(prt->geom.start < 1048576 / dev->sector_size ? 1048576 / dev->sector_size : psalign(prt->geom.start, dev->sector_size)), llong(prt->next && prt->next->type == PED_PARTITION_METADATA ? type == MSDOS ? prt->next->geom.end : prt->next->geom.end - (34816 / dev->sector_size * 10 + 5) / 10 : pealign(prt->geom.end, dev->sector_size))};
                                if(fgeom[1] - fgeom[0] > 1048576 / dev->sector_size - 2) ThrdSlst->append(path % "?\n" % QStr::number((fgeom[1] - fgeom[0] + 1) * dev->sector_size) % '\n' % QStr::number(Freespace) % '\n' % QStr::number(fgeom[0] * dev->sector_size));
                            }
                            else if(! egeom.isEmpty())
                            {
                                if(prt->geom.end <= egeom.at(1))
                                    switch(egeom.count()) {
                                    case 2:
                                        if(prt->geom.length >= 3145728 / dev->sector_size) egeom.append({(prt->geom.start - egeom.at(0) < 1048576 / dev->sector_size ? egeom.at(0) : prt->geom.start), prt->geom.end + (prt->geom.end == egeom.at(1) ? 1 : 0)});
                                        break;
                                    default:
                                        egeom.replace(3, prt->geom.end + (prt->geom.end == egeom.at(1) ? 1 : 0));
                                    }
                                else
                                    egeom.clear();
                            }
                        }

                    if(egeom.count() > 2)
                    {
                        ullong pstart(psalign(egeom.at(2), dev->sector_size));
                        ThrdSlst->append(path % "?\n" % QStr::number((pealign(egeom.at(3), dev->sector_size) - pstart + 1) * dev->sector_size - 1048576) % '\n' % QStr::number(Emptyspace) % '\n' % QStr::number(pstart * dev->sector_size + 1048576));
                    }
                }

            next_1:
                ped_disk_destroy(dsk);
            next_2:
                ped_device_destroy(dev);
            }
        }

        break;
    }
    case Readlvdevs:
    {
        ThrdSlst->reserve(10);
        QBA fstab(fload("/etc/fstab"));
        QSL dlst[]{{"_usb-*", "_mmc-*"}, {"_/dev/sd*", "_/dev/mmcblk*"}};

        for(cQStr &item : QDir("/dev/disk/by-id").entryList(QDir::Files))
        {
            if(like(item, dlst[0]) && islink("/dev/disk/by-id/" % item))
            {
                QStr path(rlink("/dev/disk/by-id/" % item, 14));

                if(! path.isEmpty() && like((path = "/dev" % right(path, -5)).length(), {8, 12}) && like(path, dlst[1]))
                {
                    ullong size(devsize(path));

                    if(size > 536870911)
                    {
                        if(! fstab.isEmpty())
                        {
                            QSL fchk('_' % path % '*');

                            {
                                PedDevice *dev(ped_device_get(bstr(path)));
                                PedDisk *dsk(ped_disk_new(dev));

                                if(dsk)
                                {
                                    PedPartition *prt(nullptr);

                                    while((prt = ped_disk_next_partition(dsk, prt)))
                                        if(prt->num > 0 && prt->type != PED_PARTITION_EXTENDED)
                                        {
                                            QStr ppath(path % (path.length() == 12 ? "p" : nullptr) % QStr::number(prt->num));

                                            if(stype(ppath) == Isblock)
                                            {
                                                blkid_probe pr(blkid_new_probe_from_filename(bstr(ppath)));
                                                blkid_do_probe(pr);
                                                cchar *uuid(nullptr);
                                                if(blkid_probe_lookup_value(pr, "UUID", &uuid, nullptr) == 0 && uuid) fchk.append("_UUID=" % QStr(uuid) % '*');
                                                blkid_free_probe(pr);
                                            }
                                        }

                                    ped_disk_destroy(dsk);
                                }

                                ped_device_destroy(dev);
                            }

                            QTS in(&fstab, QIODevice::ReadOnly);

                            while(! in.atEnd())
                                if(like(in.readLine().trimmed(), fchk)) goto next_3;
                        }

                        ThrdSlst->append(path % '\n' % mid(item, 5, rinstr(item, "_") - 5).replace('_', ' ') % '\n' % QStr::number(size));
                    }
                }
            }

        next_3:;
        }

        return ThrdSlst->sort();
    }
    case Ruuid:
    {
        blkid_probe pr(blkid_new_probe_from_filename(bstr(ThrdStr[0])));
        blkid_do_probe(pr);
        cchar *uuid(nullptr);
        blkid_probe_lookup_value(pr, "UUID", &uuid, nullptr);
        ThrdStr[1] = uuid;
        return blkid_free_probe(pr);
    }
    case Setpflag:
    {
        PedDevice *dev(ped_device_get(bstr(left(ThrdStr[0], (ThrdStr[0].contains("mmc") ? 12 : 8)))));
        PedDisk *dsk(ped_disk_new(dev));
        PedPartition *prt(nullptr);
        if(ThrdRslt) ThrdRslt = false;

        while(! ThrdKill && (prt = ped_disk_next_partition(dsk, prt)))
            if(QStr(ped_partition_get_path(prt)) == ThrdStr[0])
            {
                if(like(1, {ped_partition_set_flag(prt, ped_partition_flag_get_by_name(bstr(ThrdStr[1])), 1), ped_disk_commit_to_dev(dsk)}, true)) ThrdRslt = true;
                ped_disk_commit_to_os(dsk);
            }

        ped_disk_destroy(dsk);
        return ped_device_destroy(dev);
    }
    case Mkptable:
    {
        PedDevice *dev(ped_device_get(bstr(ThrdStr[0])));
        PedDisk *dsk(ped_disk_new_fresh(dev, ped_disk_type_get(bstr(ThrdStr[1]))));
        ThrdRslt = ped_disk_commit_to_dev(dsk) == 1;
        ped_disk_commit_to_os(dsk);
        ped_disk_destroy(dsk);
        return ped_device_destroy(dev);
    }
    case Mkpart:
    {
        PedDevice *dev(ped_device_get(bstr(ThrdStr[0])));
        PedDisk *dsk(ped_disk_new(dev));
        PedPartition *prt(nullptr);
        if(ThrdRslt) ThrdRslt = false;

        if(ThrdLng[0] > 0 && ThrdLng[1] > 0)
        {
            PedPartition *crtprt(ped_partition_new(dsk, ThrdChr == Primary ? PED_PARTITION_NORMAL : ThrdChr == Extended ? PED_PARTITION_EXTENDED : PED_PARTITION_LOGICAL, ped_file_system_type_get("ext2"), psalign(ThrdLng[0] / dev->sector_size, dev->sector_size), ullong(dev->length - 1048576 / dev->sector_size) >= (ThrdLng[0] + ThrdLng[1]) / dev->sector_size - 1 ? pealign((ThrdLng[0] + ThrdLng[1]) / dev->sector_size - 1, dev->sector_size) : (ThrdLng[0] + ThrdLng[1]) / dev->sector_size - 1));
            if(like(1, {ped_disk_add_partition(dsk, crtprt, ped_constraint_exact(&crtprt->geom)), ped_disk_commit_to_dev(dsk)}, true)) ThrdRslt = true;
            ped_disk_commit_to_os(dsk);
        }
        else
            while(! ThrdKill && (prt = ped_disk_next_partition(dsk, prt)))
                if(prt->type == PED_PARTITION_FREESPACE && prt->geom.length >= 1048576 / dev->sector_size)
                {
                    PedPartition *crtprt(ped_partition_new(dsk, PED_PARTITION_NORMAL, ped_file_system_type_get("ext2"), ThrdLng[0] == 0 ? psalign(prt->geom.start, dev->sector_size) : psalign(ThrdLng[0] / dev->sector_size, dev->sector_size), ThrdLng[1] == 0 ? prt->next && prt->next->type == PED_PARTITION_METADATA ? prt->next->geom.end : pealign(prt->geom.end, dev->sector_size) : pealign((ThrdLng[0] + ThrdLng[1]) / dev->sector_size - 1, dev->sector_size)));
                    if(like(1, {ped_disk_add_partition(dsk, crtprt, ped_constraint_exact(&crtprt->geom)), ped_disk_commit_to_dev(dsk)}, true)) ThrdRslt = true;
                    ped_disk_commit_to_os(dsk);
                    break;
                }

        ped_disk_destroy(dsk);
        return ped_device_destroy(dev);
    }
    case Delpart:
    {
        bool ismmc(ThrdStr[0].contains("mmc"));
        PedDevice *dev(ped_device_get(bstr(left(ThrdStr[0], ismmc ? 12 : 8))));
        PedDisk *dsk(ped_disk_new(dev));

        if(ped_disk_delete_partition(dsk, ped_disk_get_partition(dsk, right(ThrdStr[0], - (ismmc ? 13 : 8)).toUShort())) == 1)
        {
            ped_disk_commit_to_dev(dsk);
            ped_disk_commit_to_os(dsk);
        }

        ped_disk_destroy(dsk);
        return ped_device_destroy(dev);
    }
    case Crtrpoint:
        ThrdRslt = thrdcrtrpoint(ThrdStr[0]);
        break;
    case Srestore:
        ThrdRslt = thrdsrestore(ThrdChr, ThrdStr[0], ThrdStr[1], ThrdStr[2], ThrdBool);
        break;
    case Scopy:
        ThrdRslt = thrdscopy(ThrdChr, ThrdStr[0], ThrdStr[1]);
        break;
    case Lvprpr:
        ThrdRslt = thrdlvprpr(ThrdBool);
    }
}

bool sb::thrdcrtrpoint(cQStr &trgt)
{
    uint lcnt;

    {
        QBA sysitms[12];
        QUCL sysitmst[12];

        {
            QStr dirs[]{"/bin", "/boot", "/etc", "/lib", "/lib32", "/lib64", "/opt", "/sbin", "/selinux", "/srv", "/usr", "/var"};
            uint bbs[]{10000, 20000, 100000, 500000, 10000, 10000, 100000, 10000, 10000, 10000, 10000000, 1000000}, ibs[]{500, 1000, 10000, 20000, 500, 500, 5000, 1000, 500, 500, 500000, 50000};

            for(uchar a(0) ; a < 12 ; ++a)
                if(isdir(dirs[a]))
                {
                    sysitms[a].reserve(bbs[a]);
                    sysitmst[a].reserve(ibs[a]);
                    if(! rodir(sysitms[a], sysitmst[a], dirs[a])) return false;
                }
        }

        uint anum(0), cnum(0);
        for(uchar a(0) ; a < 12 ; ++a) anum += sysitmst[a].count();
        QSL rplst;
        QBA *cditms;
        QUCL *cditmst;
        uchar cperc;

        {
            QSL usrs;
            QBAL homeitms;
            QUCLL homeitmst;

            {
                QFile file("/etc/passwd");
                if(! file.open(QIODevice::ReadOnly)) return false;

                while(! file.atEnd())
                {
                    QStr usr(file.readLine().trimmed());

                    if(usr.contains(":/home/") && isdir("/home/" % (usr = left(usr, instr(usr, ":") -1))))
                    {
                        usrs.append(usr);
                        homeitms.append(nullptr);
                        homeitmst.append(QUCL());
                    }
                }
            }

            if(! (usrs << (isdir("/root") ? "" : nullptr)).last().isNull())
            {
                homeitms.append(nullptr);
                homeitms.last().reserve(50000);
                homeitmst.append(QUCL());
                homeitmst.last().reserve(1000);
                if(! rodir(homeitms.last(), homeitmst.last(), "/root", true)) return false;
            }

            for(schar a(usrs.count() - 2) ; a > -1 ; --a)
            {
                homeitms[a].reserve(5000000);
                homeitmst[a].reserve(100000);
                if(! rodir(homeitms[a], homeitmst[a], "/home/" % usrs.at(a), true)) return false;
            }

            for(cQUCL &cucl : homeitmst) anum += cucl.count();
            Progress = 0;
            if(! crtdir(trgt) || (isdir("/home") && ! crtdir(trgt % "/home"))) return false;

            if(incrmtl == True)
            {
                rplst.reserve(15);
                QSL incl{"_S01_*", "_S02_*", "_S03_*", "_S04_*", "_S05_*", "_S06_*", "_S07_*", "_S08_*", "_S09_*", "_S10_*", "_H01_*", "_H02_*", "_H03_*", "_H04_*", "_H05_*"};

                for(cQStr &item : QDir(sdir[1]).entryList(QDir::Dirs | QDir::NoDotAndDotDot))
                {
                    if(like(item, incl)) rplst.append(item);
                    if(ThrdKill) return false;
                }
            }

            QSL elst{".sbuserdata", ".cache/gvfs", ".local/share/Trash/files/", ".local/share/Trash/info/", ".Xauthority", ".ICEauthority"};

            {
                QFile file("/etc/systemback.excludes");
                if(! file.open(QIODevice::ReadOnly)) return false;

                while(! file.atEnd())
                {
                    QBA cline(file.readLine().trimmed());
                    if(cline.startsWith('.')) elst.append(cline);
                    if(ThrdKill) return false;
                }
            }

            QSL excl{"_lost+found_", "_lost+found/*", "*/lost+found_", "*/lost+found/*", "_Systemback_", "_Systemback/*", "*/Systemback_", "*/Systemback/*", "*~_", "*~/*"};

            for(uchar a(0) ; a < usrs.count() ; ++a)
            {
                cQStr &usr(usrs.at(a));

                if(! usr.isNull())
                {
                    QStr srcd(usr.isEmpty() ? QStr("/root") : "/home/" % usr);
                    if(! crtdir(trgt % srcd)) return false;

                    if(! (cditmst = &homeitmst[a])->isEmpty())
                    {
                        lcnt = 0;
                        QTS in((cditms = &homeitms[a]), QIODevice::ReadOnly);

                        while(! in.atEnd())
                        {
                            if(Progress < (cperc = (++cnum * 100 + 50) / anum)) Progress = cperc;
                            QStr item(in.readLine());

                            if(! like(item, excl) && ! exclcheck(elst, item))
                            {
                                QStr srci(srcd % '/' % item);

                                if(exist(srci))
                                {
                                    QStr nrpi(trgt % srci);

                                    switch(cditmst->at(lcnt)) {
                                    case Islink:
                                        for(cQStr &pname : rplst)
                                        {
                                            QStr orpi(sdir[1] % '/' % pname % srci);

                                            if(stype(orpi) == Islink && lcomp(srci, orpi))
                                            {
                                                if(! crthlnk(orpi, nrpi)) goto err_1;
                                                goto nitem_1;
                                            }

                                            if(ThrdKill) return false;
                                        }

                                        if(! cplink(srci, nrpi)) goto err_1;
                                        break;
                                    case Isdir:
                                        if(! crtdir(nrpi)) return false;
                                        break;
                                    case Isfile:
                                        if(QFile(srci).size() <= 8000000)
                                        {
                                            for(cQStr &pname : rplst)
                                            {
                                                QStr orpi(sdir[1] % '/' % pname % srci);

                                                if(stype(orpi) == Isfile && fcomp(srci, orpi) == 2)
                                                {
                                                    if(! crthlnk(orpi, nrpi)) goto err_1;
                                                    goto nitem_1;
                                                }

                                                if(ThrdKill) return false;
                                            }

                                            if(! cpfile(srci, nrpi)) goto err_1;
                                        }
                                    }

                                    goto nitem_1;
                                err_1:
                                    ThrdDbg = '@' % srci;
                                    return false;
                                }
                            }

                        nitem_1:
                            if(ThrdKill) return false;
                            ++lcnt;
                        }

                        in.seek(0);
                        lcnt = 0;

                        while(! in.atEnd())
                        {
                            QStr item(in.readLine());

                            if(cditmst->at(lcnt++) == Isdir)
                            {
                                QStr srci(srcd % '/' % item), nrpi(trgt % srci);

                                if(exist(nrpi) && ! cpertime(srci, nrpi))
                                {
                                    ThrdDbg = '@' % srci;
                                    return false;
                                }
                            }

                            if(ThrdKill) return false;
                        }

                        cditms->clear();
                        cditmst->clear();
                    }

                    if(! cpertime(srcd, trgt % srcd))
                    {
                        ThrdDbg = '@' % srcd;
                        return false;
                    }
                }
            }
        }

        if(isdir(trgt % "/home") && ! cpertime("/home", trgt % "/home"))
        {
            ThrdDbg = "@/home";
            return false;
        }

        {
            QSL incl{"_initrd.img_", "_initrd.img.old_", "_vmlinuz_", "_vmlinuz.old_"};

            for(cQStr &item : QDir("/").entryList(QDir::Files))
            {
                if(like(item, incl))
                {
                    QStr srci('/' % item);

                    if(islink(srci))
                    {
                        QStr nrpi(trgt % srci);

                        for(cQStr &pname : rplst)
                        {
                            QStr orpi(sdir[1] % '/' % pname % srci);

                            if(stype(orpi) == Islink && lcomp(srci, orpi))
                            {
                                if(! crthlnk(orpi, nrpi)) goto err_2;
                                goto nitem_2;
                            }

                            if(ThrdKill) return false;
                        }

                        if(! cplink(srci, nrpi)) return false;
                        goto nitem_2;
                    err_2:
                        ThrdDbg = '@' % srci;
                        return false;
                    }
                }

            nitem_2:
                if(ThrdKill) return false;
            }
        }

        for(cQStr &cdir : {"/cdrom", "/dev", "/mnt", "/proc", "/run", "/sys", "/tmp"})
        {
            if(isdir(cdir) && ! cpdir(cdir, trgt % cdir)) return false;
            if(ThrdKill) return false;
        }

        QSL elst{"/etc/mtab", "/var/.sblvtmp", "/var/cache/fontconfig/", "/var/lib/dpkg/lock", "/var/lib/udisks2/", "/var/lib/ureadahead/", "/var/log/", "/var/run/", "/var/tmp/"}, dlst{"/bin", "/boot", "/etc", "/lib", "/lib32", "/lib64", "/opt", "/sbin", "/selinux", "/srv", "/usr", "/var"}, excl[]{{"_lost+found_", "_lost+found/*", "*/lost+found_", "*/lost+found/*", "_Systemback_", "_Systemback/*", "*/Systemback_", "*/Systemback/*", "*.dpkg-old_", "*~_", "*~/*"}, {"+_/var/cache/apt/*", "-*.bin_", "-*.bin.*"}, {"_/var/cache/apt/archives/*", "*.deb_"}};

        for(uchar a(0) ; a < dlst.count() ; ++a)
        {
            cQStr &cdir(dlst.at(a));

            if(isdir(cdir))
            {
                if(! crtdir(trgt % cdir)) return false;

                if(! (cditmst = &sysitmst[a])->isEmpty())
                {
                    lcnt = 0;
                    QTS in((cditms = &sysitms[a]), QIODevice::ReadOnly);

                    while(! in.atEnd())
                    {
                        if(Progress < (cperc = (++cnum * 100 + 50) / anum)) Progress = cperc;
                        QStr item(in.readLine());

                        if(! like(item, excl[0]))
                        {
                            QStr srci(cdir % '/' % item);

                            if(! like(srci, excl[1], Mixed) && ! like(srci, excl[2], All) && ! exclcheck(elst, srci) && exist(srci))
                            {
                                QStr nrpi(trgt % srci);

                                switch(cditmst->at(lcnt)) {
                                case Islink:
                                    for(cQStr &pname : rplst)
                                    {
                                        QStr orpi(sdir[1] % '/' % pname % srci);

                                        if(stype(orpi) == Islink && lcomp(srci, orpi))
                                        {
                                            if(! crthlnk(orpi, nrpi)) goto err_3;
                                            goto nitem_3;
                                        }

                                        if(ThrdKill) return false;
                                    }

                                    if(! cplink(srci, nrpi)) goto err_3;
                                    break;
                                case Isdir:
                                    if(! crtdir(nrpi)) return false;
                                    break;
                                case Isfile:
                                    for(cQStr &pname : rplst)
                                    {
                                        QStr orpi(sdir[1] % '/' % pname % srci);

                                        if(stype(orpi) == Isfile && fcomp(srci, orpi) == 2)
                                        {
                                            if(! crthlnk(orpi, nrpi)) goto err_3;
                                            goto nitem_3;
                                        }

                                        if(ThrdKill) return false;
                                    }

                                    if(! cpfile(srci, nrpi)) goto err_3;
                                }

                                goto nitem_3;
                            err_3:
                                ThrdDbg = '@' % srci;
                                return false;
                            }
                        }

                    nitem_3:
                        if(ThrdKill) return false;
                        ++lcnt;
                    }

                    in.seek(0);
                    lcnt = 0;

                    while(! in.atEnd())
                    {
                        QStr item(in.readLine());

                        if(cditmst->at(lcnt++) == Isdir)
                        {
                            QStr srci(cdir % '/' % item), nrpi(trgt % srci);

                            if(exist(nrpi) && ! cpertime(srci, nrpi))
                            {
                                ThrdDbg = '@' % srci;
                                return false;
                            }
                        }

                        if(ThrdKill) return false;
                    }

                    cditms->clear();
                    cditmst->clear();
                }

                if(! cpertime(cdir, trgt % cdir))
                {
                    ThrdDbg = '@' % cdir;
                    return false;
                }
            }
        }
    }

    if(isdir("/media"))
    {
        if(! crtdir(trgt % "/media")) return false;

        if(isfile("/etc/fstab"))
        {
            QSL dlst(QDir("/media").entryList(QDir::Dirs | QDir::NoDotAndDotDot));
            QFile file("/etc/fstab");
            if(! file.open(QIODevice::ReadOnly)) return false;

            for(uchar a(0) ; a < dlst.count() ; ++a)
            {
                cQStr &item(dlst.at(a));
                if(a > 0 && ! file.open(QIODevice::ReadOnly)) return false;
                QSL incl{"* /media/" % item % " *", "* /media/" % item % "/*"};

                while(! file.atEnd())
                {
                    QStr cline(file.readLine().trimmed().replace('\t', ' ')), fdir;

                    if(! cline.startsWith('#') && like(cline.contains("\\040") ? QStr(cline).replace("\\040", " ") : cline, incl))
                        for(cQStr cdname : mid(cline, instr(cline, "/media/") + 7, instr(cline, " ", instr(cline, "/media/")) - instr(cline, "/media/") - 7).split('/'))
                            if(! cdname.isEmpty())
                            {
                                QStr nrpi(trgt % "/media" % fdir.append('/' % (cdname.contains("\\040") ? QStr(cdname).replace("\\040", " ") : cdname)));

                                if(! isdir(nrpi) && ! cpdir("/media" % fdir, nrpi))
                                {
                                    ThrdDbg = "@/media" % fdir;
                                    return false;
                                }
                            }

                    if(ThrdKill) return false;
                }

                file.close();
            }
        }

        if(! cpertime("/media", trgt % "/media"))
        {
            ThrdDbg = "@/media";
            return false;
        }
    }

    if(isdir("/var/log"))
    {
        QBA logitms;
        logitms.reserve(20000);
        QUCL logitmst;
        logitmst.reserve(1000);
        if(! rodir(logitms, logitmst, "/var/log")) return false;

        if(! logitmst.isEmpty())
        {
            QSL excl{"*.gz_", "*.old_"};
            lcnt = 0;
            QTS in(&logitms, QIODevice::ReadOnly);

            while(! in.atEnd())
            {
                QStr item(in.readLine());

                switch(logitmst.at(lcnt++)) {
                case Isdir:
                    if(! crtdir(trgt % "/var/log/" % item)) goto err_4;
                    break;
                case Isfile:
                    if(! like(item, excl) && (! item.contains('.') || ! isnum(right(item, - rinstr(item, ".")))))
                    {
                        QStr srci("/var/log/" % item), nrpi(trgt % srci);
                        crtfile(nrpi);
                        if(! cpertime(srci, nrpi)) goto err_4;
                    }
                }

                if(ThrdKill) return false;
                continue;
            err_4:
                ThrdDbg = "@/var/log/" % item;
                return false;
            }

            in.seek(0);
            lcnt = 0;

            while(! in.atEnd())
            {
                QStr item(in.readLine());

                if(logitmst.at(lcnt++) == Isdir)
                {
                    QStr srci("/var/log/" % item), nrpi(trgt % srci);

                    if(exist(nrpi) && ! cpertime("/var/log/" % item, nrpi))
                    {
                        ThrdDbg = '@' % srci;
                        return false;
                    }
                }

                if(ThrdKill) return false;
            }
        }

        if(! cpertime("/var/log", trgt % "/var/log"))
        {
            ThrdDbg = "@/var/log";
            return false;
        }
        else if(! cpertime("/var", trgt % "/var"))
        {
            ThrdDbg = "@/var";
            return false;
        }
    }

    return true;
}

bool sb::thrdsrestore(uchar mthd, cQStr &usr, cQStr &srcdir, cQStr &trgt, bool sfstab)
{
    QSL usrs;
    QBAL homeitms;
    QUCLL homeitmst;
    uint anum(0);

    if(mthd != 2)
    {
        if(! like(mthd, {4, 6}))
        {
            if(isdir(srcdir % "/home"))
                for(cQStr &cusr : QDir(srcdir % "/home").entryList(QDir::Dirs | QDir::NoDotAndDotDot))
                {
                    usrs.append(cusr);
                    homeitms.append(nullptr);
                    homeitmst.append(QUCL());
                }

            usrs.append(isdir(srcdir % "/root") ? "" : nullptr);
        }
        else if(usr == "root")
            usrs.append(isdir(srcdir % "/root") ? "" : nullptr);
        else if(isdir(srcdir % "/home/" % usr))
        {
            usrs = QSL{usr, nullptr};
            homeitms.append(nullptr);
            homeitmst.append(QUCL());
        }
        else
            usrs.append(nullptr);

        if(! usrs.last().isNull())
        {
            homeitms.append(nullptr);
            homeitms.last().reserve(50000);
            homeitmst.append(QUCL());
            homeitmst.last().reserve(1000);
            if(! rodir(homeitms.last(), homeitmst.last(), srcdir % "/root", true)) return false;
        }

        for(schar a(usrs.count() - 2) ; a > -1 ; --a)
        {
            homeitms[a].reserve(5000000);
            homeitmst[a].reserve(100000);
            if(! rodir(homeitms[a], homeitmst[a], srcdir % "/home/" % usrs.at(a), true)) return false;
        }

        for(cQUCL &cucl : homeitmst) anum += cucl.count();
    }

    QBA *cditms;
    QUCL *cditmst;
    uint cnum(0), lcnt;
    uchar cperc;
    auto fspchk([&trgt] { return dfree(trgt.isEmpty() ? "/" : trgt) > 10485760; });

    if(mthd < 3)
    {
        {
            QBA sysitms[12];
            QUCL sysitmst[12];

            {
                QStr dirs[]{srcdir % "/bin", srcdir % "/boot", srcdir % "/etc", srcdir % "/lib", srcdir % "/lib32", srcdir % "/lib64", srcdir % "/opt", srcdir % "/sbin", srcdir % "/selinux", srcdir % "/srv", srcdir % "/usr", srcdir % "/var"};
                uint bbs[]{10000, 20000, 100000, 500000, 10000, 10000, 100000, 10000, 10000, 10000, 10000000, 1000000}, ibs[]{500, 1000, 10000, 20000, 500, 500, 5000, 1000, 500, 500, 500000, 50000};

                for(uchar a(0) ; a < 12 ; ++a)
                    if(isdir(dirs[a]))
                    {
                        sysitms[a].reserve(bbs[a]);
                        sysitmst[a].reserve(ibs[a]);
                        if(! rodir(sysitms[a], sysitmst[a], dirs[a])) return false;
                    }
            }

            for(uchar a(0) ; a < 12 ; ++a) anum += sysitmst[a].count();
            Progress = 0;

            {
                QSL incl{"_initrd.img_", "_initrd.img.old_", "_vmlinuz_", "_vmlinuz.old_"};

                for(cQStr &item : QDir(trgt.isEmpty() ? "/" : trgt).entryList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Files | QDir::System))
                {
                    if(like(item, incl))
                    {
                        QStr trgi(trgt % '/' % item);

                        switch(stype(trgi)) {
                        case Islink:
                        {
                            QStr srci(srcdir % '/' % item);
                            if(exist(srci) && lcomp(srci, trgi)) break;
                        }
                        case Isfile:
                            rmfile(trgi);
                            break;
                        case Isdir:
                            recrmdir(trgi);
                        }
                    }

                    if(ThrdKill) return false;
                }

                for(cQStr &item : QDir(srcdir).entryList(QDir::Files | QDir::System))
                {
                    if(like(item, incl))
                    {
                        QStr trgi(trgt % '/' % item);
                        if(! exist(trgi)) cplink(srcdir % '/' % item, trgi);
                    }

                    if(ThrdKill) return false;
                }
            }

            for(cQStr &cdir : {"/cdrom", "/dev", "/home", "/mnt", "/root", "/proc", "/run", "/sys", "/tmp"})
            {
                QStr srci(srcdir % cdir), trgi(trgt % cdir);

                if(! isdir(srci))
                {
                    if(exist(trgi)) stype(trgi) == Isdir ? recrmdir(trgi) : rmfile(trgi);
                }
                else if(isdir(trgi))
                    cpertime(srci, trgi);
                else
                {
                    if(exist(trgi)) rmfile(trgi);
                    if(! cpdir(srci, trgi) && ! fspchk()) return false;
                }

                if(ThrdKill) return false;
            }

            QSL elst;
            if(trgt.isEmpty()) elst = QSL{"/etc/mtab", "/var/cache/fontconfig/", "/var/lib/dpkg/lock", "/var/lib/udisks2/", "/var/run/", "/var/tmp/"};
            if(trgt.isEmpty() || (isfile("/mnt/etc/xdg/autostart/sbschedule.desktop") && isfile("/mnt/etc/xdg/autostart/sbschedule-kde.desktop") && isfile("/mnt/usr/bin/systemback") && isfile("/mnt/usr/lib/systemback/libsystemback.so.1.0.0") && isfile("/mnt/usr/lib/systemback/sbscheduler") && isfile("/mnt/usr/lib/systemback/sbsustart") && isfile("/mnt/usr/lib/systemback/sbsysupgrade")&& isdir("/mnt/usr/share/systemback/lang") && isfile("/mnt/usr/share/systemback/efi.tar.gz") && isfile("/mnt/usr/share/systemback/splash.png") && isfile("/mnt/var/lib/dpkg/info/systemback.list") && isfile("/mnt/var/lib/dpkg/info/systemback.md5sums"))) elst.append({"/etc/systemback*", "/etc/xdg/autostart/sbschedule*", "/usr/bin/systemback*", "/usr/lib/systemback/", "/usr/share/systemback/", "/var/lib/dpkg/info/systemback*"});
            if(sfstab) elst.append("/etc/fstab");
            QSL dlst{"/bin", "/boot", "/etc", "/lib", "/lib32", "/lib64", "/opt", "/sbin", "/selinux", "/srv", "/usr", "/var"}, excl[]{{"_lost+found_", "_Systemback_"}, {"_lost+found_", "_lost+found/*", "*/lost+found_", "*/lost+found/*", "_Systemback_", "_Systemback/*", "*/Systemback_", "*/Systemback/*"}};

            for(uchar a(0) ; a < dlst.count() ; ++a)
            {
                cQStr &cdir(dlst.at(a));
                QStr srcd(srcdir % cdir), trgd(trgt % cdir);

                if(isdir(srcd))
                {
                    if(isdir(trgd))
                    {
                        QBAL sdlst;
                        if(! odir(sdlst, trgd)) return false;

                        for(cQStr &item : sdlst)
                        {
                            if(! like(item, excl[0]) && ! exclcheck(elst, cdir % '/' % item) && ! exist(srcd % '/' % item))
                            {
                                QStr trgi(trgd % '/' % item);
                                stype(trgi) == Isdir ? recrmdir(trgi) : rmfile(trgi);
                            }

                            if(ThrdKill) return false;
                        }
                    }
                    else
                    {
                        if(exist(trgd)) rmfile(trgd);
                        if(! crtdir(trgd) && ! fspchk()) return false;
                    }

                    if(! (cditmst = &sysitmst[a])->isEmpty())
                    {
                        lcnt = 0;
                        QTS in((cditms = &sysitms[a]), QIODevice::ReadOnly);

                        while(! in.atEnd())
                        {
                            if(Progress < (cperc = (++cnum * 100 + 50) / anum)) Progress = cperc;
                            QStr item(in.readLine());

                            if(! like(item, excl[1]) && ! exclcheck(elst, cdir % '/' % item))
                            {
                                QStr srci(srcd % '/' % item), trgi(trgd % '/' % item);

                                switch(cditmst->at(lcnt)) {
                                case Islink:
                                    switch(stype(trgi)) {
                                    case Islink:
                                        if(lcomp(srci, trgi)) goto nitem_1;
                                    case Isfile:
                                        rmfile(trgi);
                                        break;
                                    case Isdir:
                                        recrmdir(trgi);
                                    }

                                    if(! cplink(srci, trgi) && ! fspchk()) return false;
                                    break;
                                case Isdir:
                                    switch(stype(trgi)) {
                                    case Isdir:
                                    {
                                        QBAL sdlst;
                                        if(! odir(sdlst, trgi)) return false;

                                        for(cQStr &sitem : sdlst)
                                        {
                                            if(! like(sitem, excl[0]) && ! exclcheck(elst, cdir % '/' % item % '/' % sitem) && ! exist(srci % '/' % sitem))
                                            {
                                                QStr strgi(trgi % '/' % sitem);
                                                stype(strgi) == Isdir ? recrmdir(strgi) : rmfile(strgi);
                                            }

                                            if(ThrdKill) return false;
                                        }

                                        goto nitem_1;
                                    }
                                    case Islink:
                                    case Isfile:
                                        rmfile(trgi);
                                    }

                                    if(! crtdir(trgi) && ! fspchk()) return false;
                                    break;
                                case Isfile:
                                    switch(stype(trgi)) {
                                    case Isfile:
                                        switch(fcomp(srci, trgi)) {
                                        case 1:
                                            cpertime(srci, trgi);
                                        case 2:
                                            goto nitem_1;
                                        }
                                    case Islink:
                                        rmfile(trgi);
                                        break;
                                    case Isdir:
                                        recrmdir(trgi);
                                    }

                                    if(! cpfile(srci, trgi) && ! fspchk()) return false;
                                }
                            }

                        nitem_1:
                            if(ThrdKill) return false;
                            ++lcnt;
                        }

                        in.seek(0);
                        lcnt = 0;

                        while(! in.atEnd())
                        {
                            QStr item(in.readLine());

                            if(cditmst->at(lcnt++) == Isdir)
                            {
                                QStr trgi(trgd % '/' % item);
                                if(exist(trgi)) cpertime(srcd % '/' % item, trgi);
                            }

                            if(ThrdKill) return false;
                        }

                        cditms->clear();
                        cditmst->clear();
                    }

                    cpertime(srcd, trgd);
                }
                else if(exist(trgd))
                    stype(trgd) == Isdir ? recrmdir(trgd) : rmfile(trgd);
            }
        }

        {
            QStr srcd(srcdir % "/media"), trgd(trgt % "/media");

            if(isdir(srcd))
            {
                if(isdir(trgd))
                    for(cQStr &item : QDir(trgd).entryList(QDir::Dirs | QDir::NoDotAndDotDot))
                    {
                        if(! exist(srcd % '/' % item))
                        {
                            QStr trgi(trgd % '/' % item);
                            if(! mcheck(trgi % '/')) recrmdir(trgi);
                        }

                        if(ThrdKill) return false;
                    }
                else
                {
                    if(exist(trgd)) rmfile(trgd);
                    if(! crtdir(trgd) && ! fspchk()) return false;
                }

                QBA mediaitms;
                mediaitms.reserve(10000);
                if(! rodir(mediaitms, srcd)) return false;

                if(! mediaitms.isEmpty())
                {
                    QTS in(&mediaitms, QIODevice::ReadOnly);

                    while(! in.atEnd())
                    {
                        QStr item(in.readLine()), trgi(trgd % '/' % item);

                        if(exist(trgi))
                        {
                            if(stype(trgi) != Isdir)
                            {
                                rmfile(trgi);
                                if(! crtdir(trgi) && ! fspchk()) return false;
                            }

                            cpertime(srcd % '/' % item, trgi);
                        }
                        else if(! cpdir(srcd % '/' % item, trgi) && ! fspchk())
                            return false;

                        if(ThrdKill) return false;
                    }
                }

                cpertime(srcd, trgd);
            }
            else if(exist(trgd))
                stype(trgd) == Isdir ? recrmdir(trgd) : rmfile(trgd);
        }

        if(srcdir == "/.systembacklivepoint" && isdir("/.systembacklivepoint/.systemback"))
        {
            QBA sbitms;
            sbitms.reserve(10000);
            QUCL sbitmst;
            sbitmst.reserve(500);
            if(! rodir(sbitms, sbitmst, "/.systembacklivepoint/.systemback")) return false;
            lcnt = 0;
            QTS in(&sbitms, QIODevice::ReadOnly);

            while(! in.atEnd())
            {
                QStr item(in.readLine());

                switch(sbitmst.at(lcnt++)) {
                case Islink:
                    if((! sfstab || item != "etc/fstab") && ! cplink("/.systembacklivepoint/.systemback/" % item, trgt % '/' % item) && ! fspchk()) return false;
                    break;
                case Isfile:
                    if((! sfstab || item != "etc/fstab") && ! cpfile("/.systembacklivepoint/.systemback/" % item, trgt % '/' % item) && ! fspchk()) return false;
                    break;
                }

                if(ThrdKill) return false;
            }

            in.seek(0);
            lcnt = 0;

            while(! in.atEnd())
            {
                QStr item(in.readLine());
                if(sbitmst.at(lcnt++) == Isdir) cpertime("/.systembacklivepoint/.systemback/" % item, trgt % '/' % item);
                if(ThrdKill) return false;
            }
        }
    }
    else
        Progress = 0;

    if(mthd != 2)
    {
        QSL elst{".cache/gvfs", ".gvfs", ".local/share/Trash/files/", ".local/share/Trash/info/", ".Xauthority", ".ICEauthority"};

        {
            QFile file(srcdir % "/etc/systemback.excludes");
            if(! file.open(QIODevice::ReadOnly)) return false;

            while(! file.atEnd())
            {
                QBA cline(file.readLine().trimmed());
                if(cline.startsWith('.')) elst.append(cline);
                if(ThrdKill) return false;
            }
        }

        bool skppd;
        QSL excl[]{{"_lost+found_", "_lost+found/*", "*/lost+found_", "*/lost+found/*", "_Systemback_", "_Systemback/*", "*/Systemback_", "*/Systemback/*", "*~_", "*~/*"}, {"_lost+found_", "_Systemback_", "*~_"}};

        for(schar a(usrs.count() - 1) ; a > -1 ; --a)
        {
            cQStr &cusr(usrs.at(a));

            if(! cusr.isNull())
            {
                QStr srcd(srcdir), trgd(trgt);

                if(cusr.isEmpty())
                {
                    srcd.append("/root");
                    trgd.append("/root");
                }
                else
                {
                    QStr hdir("/home/" % cusr);
                    srcd.append(hdir);
                    trgd.append(hdir);
                }

                {
                    if(! isdir(trgd))
                    {
                        if(exist(trgd)) rmfile(trgd);
                        if(! crtdir(trgd) && ! fspchk()) return false;
                    }
                    else if(! like(mthd, {3, 4}))
                    {
                        QBAL sdlst;
                        if(! odir(sdlst, trgd, true)) return false;

                        for(cQStr &item : sdlst)
                        {
                            if(! item.endsWith('~') && ! exclcheck(elst, item) && ! exist(srcd % '/' % item))
                            {
                                QStr trgi(trgd % '/' % item);

                                switch(stype(trgi)) {
                                case Isdir:
                                    recrmdir(trgi, true);
                                    break;
                                case Isfile:
                                    if(QFile(trgi).size() > 8000000) break;
                                case Islink:
                                    rmfile(trgi);
                                }
                            }

                            if(ThrdKill) return false;
                        }
                    }
                }

                if(! (cditmst = &homeitmst[a])->isEmpty() && anum > 0)
                {
                    lcnt = 0;
                    QTS in((cditms = &homeitms[a]), QIODevice::ReadOnly);

                    while(! in.atEnd())
                    {
                        if(Progress < (cperc = (++cnum * 100 + 50) / anum)) Progress = cperc;
                        QStr item(in.readLine());

                        if(! like(item, excl[0]) && ! exclcheck(elst, item))
                        {
                            QStr srci(srcd % '/' % item), trgi(trgd % '/' % item);

                            switch(cditmst->at(lcnt)) {
                            case Islink:
                                switch(stype(trgi)) {
                                case Islink:
                                    if(lcomp(srci, trgi)) goto nitem_2;
                                case Isfile:
                                    rmfile(trgi);
                                    break;
                                case Isdir:
                                    recrmdir(trgi);
                                }

                                if(! cplink(srci, trgi) && ! fspchk()) return false;
                                break;
                            case Isdir:
                                switch(stype(trgi)) {
                                case Isdir:
                                {
                                    if(! like(mthd, {3, 4}))
                                    {
                                        QBAL sdlst;
                                        if(! odir(sdlst, trgi)) return false;

                                        for(cQStr &sitem : sdlst)
                                        {
                                            if(! like(sitem, excl[1]) && ! exclcheck(elst, item % '/' % sitem) && ! exist(srci % '/' % sitem))
                                            {
                                                QStr strgi(trgi % '/' % sitem);

                                                switch(stype(strgi)) {
                                                case Isdir:
                                                    recrmdir(strgi, true);
                                                    break;
                                                case Isfile:
                                                    if(QFile(strgi).size() > 8000000) break;
                                                case Islink:
                                                    rmfile(strgi);
                                                }
                                            }

                                            if(ThrdKill) return false;
                                        }
                                    }

                                    goto nitem_2;
                                }
                                case Islink:
                                case Isfile:
                                    rmfile(trgi);
                                }

                                if(! crtdir(trgi) && ! fspchk()) return false;
                                break;
                            case Isfile:
                                skppd = QFile(srci).size() > 8000000;

                                switch(stype(trgi)) {
                                case Isfile:
                                    switch(fcomp(trgi, srci)) {
                                    case 1:
                                        cpertime(srci, trgi);
                                    case 2:
                                        goto nitem_2;
                                    }

                                    if(skppd) goto nitem_2;
                                case Islink:
                                    rmfile(trgi);
                                    break;
                                case Isdir:
                                    recrmdir(trgi);
                                }

                                if(! skppd && ! cpfile(srci, trgi) && ! fspchk()) return false;
                            }
                        }

                    nitem_2:
                        if(ThrdKill) return false;
                        ++lcnt;
                    }

                    in.seek(0);
                    lcnt = 0;

                    while(! in.atEnd())
                    {
                        QStr item(in.readLine());

                        if(cditmst->at(lcnt++) == Isdir)
                        {
                            QStr trgi(trgd % '/' % item);
                            if(exist(trgi)) cpertime(srcd % '/' % item, trgi);
                        }

                        if(ThrdKill) return false;
                    }

                    cditms->clear();
                    cditmst->clear();
                }

                cpertime(srcd, trgd);
            }
        }
    }

    return true;
}

bool sb::thrdscopy(uchar mthd, cQStr &usr, cQStr &srcdir)
{
    uint lcnt;

    {
        QBA sysitms[12];
        QUCL sysitmst[12];

        {
            QStr dirs[]{srcdir % "/bin", srcdir % "/boot", srcdir % "/etc", srcdir % "/lib", srcdir % "/lib32", srcdir % "/lib64", srcdir % "/opt", srcdir % "/sbin", srcdir % "/selinux", srcdir % "/srv", srcdir % "/usr", srcdir % "/var"};
            uint bbs[]{10000, 20000, 100000, 500000, 10000, 10000, 100000, 10000, 10000, 10000, 10000000, 1000000}, ibs[]{500, 1000, 10000, 20000, 500, 500, 5000, 1000, 500, 500, 500000, 50000};

            for(uchar a(0) ; a < 12 ; ++a)
                if(isdir(dirs[a]))
                {
                    sysitms[a].reserve(bbs[a]);
                    sysitmst[a].reserve(ibs[a]);
                    if(! rodir(sysitms[a], sysitmst[a], dirs[a])) return false;
                }
        }

        uint anum(0), cnum(0);
        for(uchar a(0) ; a < 12 ; ++a) anum += sysitmst[a].count();
        QStr macid;
        QBA *cditms;
        QUCL *cditmst;
        uchar cperc;

        {
            QSL usrs;
            QBAL homeitms;
            QUCLL homeitmst;

            if(mthd > 2)
            {
                if(isdir(srcdir % "/home/" % usr)) usrs.append(usr);
                homeitms.append(nullptr);
                homeitmst.append(QUCL());
            }
            else if(mthd > 0 && isdir(srcdir % "/home"))
            {
                if(srcdir.isEmpty())
                {
                    QFile file("/etc/passwd");
                    if(! file.open(QIODevice::ReadOnly)) return false;

                    while(! file.atEnd())
                    {
                        QStr cusr(file.readLine().trimmed());

                        if(cusr.contains(":/home/") && isdir("/home/" % (cusr = left(cusr, instr(cusr, ":") -1))))
                        {
                            usrs.append(cusr);
                            homeitms.append(nullptr);
                            homeitmst.append(QUCL());
                        }
                    }
                }
                else
                    for(cQStr &cusr : QDir(srcdir % "/home").entryList(QDir::Dirs | QDir::NoDotAndDotDot))
                    {
                        usrs.append(cusr);
                        homeitms.append(nullptr);
                        homeitmst.append(QUCL());
                    }

                if(ThrdKill) return false;
            }

            usrs.append(isdir(srcdir % "/root") ? "" : nullptr);

            if(mthd == 5)
            {
                homeitms[0].reserve(10000);
                homeitmst[0].reserve(500);
                if(! rodir(homeitms[0], homeitmst[0], srcdir % "/etc/skel")) return false;
            }
            else
            {
                if(! usrs.last().isNull())
                {
                    homeitms.append(nullptr);
                    homeitms.last().reserve(50000);
                    homeitmst.append(QUCL());
                    homeitmst.last().reserve(1000);
                    if(! rodir(homeitms.last(), homeitmst.last(), srcdir % "/root", like(mthd, {2, 3}))) return false;
                }

                for(schar a(usrs.count() - 2) ; a > -1 ; --a)
                {
                    homeitms[a].reserve(5000000);
                    homeitmst[a].reserve(100000);
                    if(! rodir(homeitms[a], homeitmst[a], srcdir % "/home/" % usrs.at(a), like(mthd, {2, 3}))) return false;
                }
            }

            for(cQUCL &cucl : homeitmst) anum += cucl.count();
            Progress = 0;

            if(isdir(srcdir % "/home") && ! isdir("/.sbsystemcopy/home") && ! crtdir("/.sbsystemcopy/home"))
            {
                QFile::rename("/.sbsystemcopy/home", "/.sbsystemcopy/home_" % rndstr());
                if(! crtdir("/.sbsystemcopy/home")) return false;
            }

            if(mthd > 2)
            {
                QStr mfile(isfile(srcdir % "/var/lib/dbus/machine-id") ? "/var/lib/dbus/machine-id" : isfile(srcdir % "/etc/machine-id") ? "/etc/machine-id" : nullptr);

                if(! mfile.isEmpty())
                {
                    QFile file(srcdir % mfile);
                    if(! file.open(QIODevice::ReadOnly)) return false;
                    QStr cline(file.readLine().trimmed());
                    if(cline.length() == 32) macid = cline;
                 }
            }

            QSL elst{".cache/gvfs", ".gvfs", ".local/share/Trash/files/", ".local/share/Trash/info/", ".Xauthority", ".ICEauthority"};

            {
                QFile file(srcdir % "/etc/systemback.excludes");
                if(! file.open(QIODevice::ReadOnly)) return false;

                while(! file.atEnd())
                {
                    QBA cline(file.readLine().trimmed());
                    if(cline.startsWith('.')) elst.append(cline);
                    if(ThrdKill) return false;
                }
            }

            bool skppd;
            QSL excl{"_lost+found_", "_lost+found/*", "*/lost+found_", "*/lost+found/*", "_Systemback_", "_Systemback/*", "*/Systemback_", "*/Systemback/*", "*~_", "*~/*"};

            for(uchar a(0) ; a < usrs.count() ; ++a)
            {
                cQStr &cusr(usrs.at(a));

                if(! cusr.isNull())
                {
                    QStr srcd[2], trgd;
                    cusr.isEmpty() ? (srcd[0] = srcdir % "/root", trgd = "/.sbsystemcopy/root") : (srcd[0] = srcdir % "/home/" % cusr, trgd = "/.sbsystemcopy/home/" % cusr);
                    srcd[1] = mthd == 5 ? srcdir % "/etc/skel" : srcd[0];

                    if(! isdir(trgd))
                    {
                        if(exist(trgd)) rmfile(trgd);

                        if(! cpdir(srcd[0], trgd))
                        {
                            ThrdDbg = '@' % right(trgd, -14);
                            return false;
                        }
                    }

                    if(! (cditmst = &homeitmst[mthd == 5 ? 0 : a])->isEmpty())
                    {
                        lcnt = 0;
                        QTS in((cditms = &homeitms[mthd == 5 ? 0 : a]), QIODevice::ReadOnly);

                        while(! in.atEnd())
                        {
                            if(Progress < (cperc = (++cnum * 100 + 50) / anum)) Progress = cperc;
                            QStr item(in.readLine());

                            if((mthd == 5 || (! like(item, excl) && ! exclcheck(elst, item) && (macid.isEmpty() || ! item.contains(macid)))) && (! srcdir.isEmpty() || exist(srcd[1] % '/' % item)))
                            {
                                QStr trgi(trgd % '/' % item);

                                switch(cditmst->at(lcnt)) {
                                case Islink:
                                {
                                    QStr srci(srcd[1] % '/' % item);

                                    switch(stype(trgi)) {
                                    case Islink:
                                        if(! lcomp(srci, trgi)) goto nitem_1;
                                    case Isfile:
                                        rmfile(trgi);
                                        break;
                                    case Isdir:
                                        recrmdir(trgi);
                                    }

                                    if(! cplink(srci, trgi)) goto err_1;
                                    break;
                                }
                                case Isdir:
                                    switch(stype(trgi)) {
                                    case Isdir:
                                        goto nitem_1;
                                    default:
                                        rmfile(trgi);
                                    }

                                    if(! crtdir(trgi)) return false;
                                    break;
                                case Isfile:
                                    QStr srci(srcd[1] % '/' % item);
                                    skppd = like(mthd, {2, 3}) && QFile(srci).size() > 8000000;

                                    switch(stype(trgi)) {
                                    case Isfile:
                                        if(mthd == 5)
                                            switch(fcomp(trgi, srci)) {
                                            case 1:
                                                if(! cpertime(srci, trgi, ! cusr.isEmpty())) goto err_1;
                                            case 2:
                                                goto nitem_1;
                                            }
                                        else
                                        {
                                            switch(fcomp(trgi, srci)) {
                                            case 1:
                                                if(! cpertime(srci, trgi)) goto err_1;
                                            case 2:
                                                goto nitem_1;
                                            }

                                            if(skppd) goto nitem_1;
                                        }
                                    case Islink:
                                        rmfile(trgi);
                                        break;
                                    case Isdir:
                                        recrmdir(trgi);
                                    }

                                    if(mthd == 5)
                                    {
                                        if(! cpfile(srci, trgi, ! cusr.isEmpty())) goto err_1;
                                    }
                                    else if(! skppd && ! cpfile(srci, trgi))
                                        goto err_1;
                                }

                                goto nitem_1;
                            err_1:
                                ThrdDbg = '@' % right(trgd, -14) % '/' % item;
                                return false;
                            }

                        nitem_1:
                            if(ThrdKill) return false;
                            ++lcnt;
                        }

                        in.seek(0);
                        lcnt = 0;

                        while(! in.atEnd())
                        {
                            QStr item(in.readLine());

                            if(cditmst->at(lcnt++) == Isdir)
                            {
                                QStr trgi(trgd % '/' % item);

                                if(exist(trgi) && ! cpertime(srcd[1] % '/' % item, trgi, mthd == 5 && ! cusr.isEmpty()))
                                {
                                    ThrdDbg = '@' % right(trgd, -14) % '/' % item;
                                    return false;
                                }
                            }

                            if(ThrdKill) return false;
                        }

                        if(mthd < 5)
                        {
                            cditms->clear();
                            cditmst->clear();
                        }
                    }

                    if(! cusr.isEmpty())
                    {
                        if(isfile(srcd[0] % "/.config/user-dirs.dirs"))
                        {
                            QFile file(srcd[0] % "/.config/user-dirs.dirs");
                            if(! file.open(QIODevice::ReadOnly)) return false;

                            while(! file.atEnd())
                            {
                                QStr cline(file.readLine().trimmed()), dir;

                                if(! cline.startsWith('#') && cline.contains("$HOME") && (dir = left(right(cline, - instr(cline, "/")), -1)).length() > 0)
                                {
                                    QStr trgi(trgd % '/' % dir);

                                    if(! isdir(trgi))
                                    {
                                        QStr srci(srcd[0] % '/' % dir);

                                        if(isdir(srci))
                                        {
                                            if(! cpdir(srci, trgi)) goto err_2;
                                        }
                                        else if(srcdir.startsWith(sdir[1]) && (! crtdir(trgi) || ! cpertime(trgd, trgi)))
                                            goto err_2;

                                        goto nitem_2;
                                    err_2:
                                        ThrdDbg = '@' % right(trgd, -14) % '/' % dir;
                                        return false;
                                    }
                                }

                            nitem_2:
                                if(ThrdKill) return false;
                            }
                        }

                        if(! cpertime(srcd[0], trgd))
                        {
                            ThrdDbg = '@' % right(trgd, -14);
                            return false;
                        }
                    }
                }
            }
        }

        {
            QSL incl{"_initrd.img_", "_initrd.img.old_", "_vmlinuz_", "_vmlinuz.old_"};

            for(cQStr &item : QDir("/.sbsystemcopy").entryList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Files | QDir::System))
            {
                if(like(item, incl))
                {
                    QStr trgi("/.sbsystemcopy/" % item);

                    switch(stype(trgi)) {
                    case Islink:
                    {
                        QStr srci(srcdir % '/' % item);
                        if(islink(srci) && lcomp(srci, trgi)) break;
                    }
                    case Isfile:
                        if(! rmfile(trgi)) return false;
                        break;
                    case Isdir:
                        if(! recrmdir(trgi)) return false;
                    }
                }

                if(ThrdKill) return false;
            }

            for(cQStr &item : QDir(srcdir.isEmpty() ? "/" : srcdir).entryList(QDir::Files | QDir::System))
            {
                if(like(item, incl))
                {
                    QStr srci(srcdir % '/' % item);

                    if(islink(srci))
                    {
                        QStr trgi("/.sbsystemcopy/" % item);
                        if(! exist(trgi) && ! cplink(srci, trgi)) return false;
                    }
                }

                if(ThrdKill) return false;
            }
        }

        for(cQStr &cdir : {"/cdrom", "/dev", "/home", "/mnt", "/root", "/proc", "/run", "/sys", "/tmp"})
        {
            QStr trgd("/.sbsystemcopy" % cdir);

            if(! isdir(srcdir % cdir))
            {
                if(exist(trgd)) stype(trgd) == Isdir ? recrmdir(trgd) : rmfile(trgd);
            }
            else if(! isdir(trgd))
            {
                if(exist(trgd)) rmfile(trgd);

                if(! cpdir(srcdir % cdir, trgd))
                {
                    ThrdDbg = '@' % cdir;
                    return false;
                }
            }
            else if(! cpertime(srcdir % cdir, trgd))
            {
                ThrdDbg = '@' % cdir;
                return false;
            }

            if(ThrdKill) return false;
        }

        QSL elst{"/boot/efi", "/etc/crypttab", "/etc/mtab", "/var/cache/fontconfig/", "/var/lib/dpkg/lock", "/var/lib/udisks2/", "/var/log", "/var/run/", "/var/tmp/"};
        if(mthd > 2) elst.append({"/etc/machine-id", "/etc/systemback.conf", "/etc/systemback.excludes", "/var/lib/dbus/machine-id"});
        if(srcdir == "/.systembacklivepoint" && fload("/proc/cmdline").contains("noxconf")) elst.append("/etc/X11/xorg.conf");

        for(cQStr &cdir : {"/etc/rc0.d", "/etc/rc1.d", "/etc/rc2.d", "/etc/rc3.d", "/etc/rc4.d", "/etc/rc5.d", "/etc/rc6.d", "/etc/rcS.d"})
        {
            QStr srcd(srcdir % cdir);

            if(isdir(srcd))
                for(cQStr &item : QDir(srcd).entryList(QDir::Files))
                    if(item.contains("cryptdisks")) elst.append(cdir % '/' % item);
        }

        QSL dlst{"/bin", "/boot", "/etc", "/lib", "/lib32", "/lib64", "/opt", "/sbin", "/selinux", "/srv", "/usr", "/var"}, excl[]{{"_lost+found_", "_Systemback_"}, {"_lost+found_", "_lost+found/*", "*/lost+found_", "*/lost+found/*", "_Systemback_", "_Systemback/*", "*/Systemback_", "*/Systemback/*"}, {"_/etc/udev/rules.d*", "*-persistent-*"}};

        for(uchar a(0) ; a < dlst.count() ; ++a)
        {
            cQStr &cdir(dlst.at(a));
            QStr srcd(srcdir % cdir), trgd("/.sbsystemcopy" % cdir);

            if(isdir(srcd))
            {
                if(isdir(trgd))
                {
                    QBAL sdlst;
                    if(! odir(sdlst, trgd)) return false;

                    for(cQStr &item : sdlst)
                    {
                        if(! like(item, excl[0]) && ! exist(srcd % '/' % item))
                        {
                            QStr trgi(trgd % '/' % item);
                            stype(trgi) == Isdir ? recrmdir(trgi) : rmfile(trgi);
                        }

                        if(ThrdKill) return false;
                    }
                }
                else
                {
                    if(exist(trgd)) rmfile(trgd);
                    if(! crtdir(trgd)) return false;
                }

                if(! (cditmst = &sysitmst[a])->isEmpty())
                {
                    lcnt = 0;
                    QTS in((cditms = &sysitms[a]), QIODevice::ReadOnly);

                    while(! in.atEnd())
                    {
                        if(Progress < (cperc = (++cnum * 100 + 50) / anum)) Progress = cperc;
                        QStr item(in.readLine());

                        if(! like(item, excl[1]))
                        {
                            QStr pdi(cdir % '/' % item);

                            if(! exclcheck(elst, pdi) && (macid.isEmpty() || ! item.contains(macid)) && (mthd < 3 || ! like(pdi, excl[2], All)) && (! srcdir.isEmpty() || exist(pdi)))
                            {
                                QStr srci(srcd % '/' % item), trgi(trgd % '/' % item);

                                switch(cditmst->at(lcnt)) {
                                case Islink:
                                    switch(stype(trgi)) {
                                    case Islink:
                                        if(lcomp(srci, trgi)) goto nitem_3;
                                    case Isfile:
                                        rmfile(trgi);
                                        break;
                                    case Isdir:
                                        recrmdir(trgi);
                                    }

                                    if(! cplink(srci, trgi)) goto err_3;
                                    break;
                                case Isdir:
                                    switch(stype(trgi)) {
                                    case Isdir:
                                    {
                                        QBAL sdlst;
                                        if(! odir(sdlst, trgi)) return false;

                                        for(cQStr &sitem : sdlst)
                                        {
                                            if(! like(sitem, excl[0]) && (! exist(srci % '/' % sitem) || pdi % '/' % sitem == "/etc/X11/xorg.conf"))
                                            {
                                                QStr strgi(trgi % '/' % sitem);
                                                stype(strgi) == Isdir ? recrmdir(strgi) : rmfile(strgi);
                                            }

                                            if(ThrdKill) return false;
                                        }

                                        goto nitem_3;
                                    }
                                    case Islink:
                                    case Isfile:
                                        rmfile(trgi);
                                    }

                                    if(! crtdir(trgi)) return false;
                                    break;
                                case Isfile:
                                    switch(stype(trgi)) {
                                    case Isfile:
                                        switch(fcomp(trgi, srci)) {
                                        case 1:
                                            if(! cpertime(srci, trgi)) goto err_3;
                                        case 2:
                                            goto nitem_3;
                                        }
                                    case Islink:
                                        rmfile(trgi);
                                        break;
                                    case Isdir:
                                        recrmdir(trgi);
                                    }

                                    if(! cpfile(srci, trgi)) goto err_3;
                                }

                                goto nitem_3;
                            err_3:
                                ThrdDbg = '@' % pdi;
                                return false;
                            }
                        }

                    nitem_3:
                        if(ThrdKill) return false;
                        ++lcnt;
                    }

                    in.seek(0);
                    lcnt = 0;

                    while(! in.atEnd())
                    {
                        QStr item(in.readLine());

                        if(cditmst->at(lcnt++) == Isdir)
                        {
                            QStr trgi(trgd % '/' % item);

                            if(exist(trgi) && ! cpertime(srcd % '/' % item, trgi))
                            {
                                ThrdDbg = '@' % cdir % '/' % item;
                                return false;
                            }
                        }

                        if(ThrdKill) return false;
                    }

                    cditms->clear();
                    cditmst->clear();
                }

                if(! cpertime(srcd, trgd))
                {
                    ThrdDbg = '@' % cdir;
                    return false;
                }
            }
            else if(exist(trgd))
                stype(trgd) == Isdir ? recrmdir(trgd) : rmfile(trgd);
        }
    }

    {
        QStr srcd(srcdir % "/media"), trgd("/.sbsystemcopy/media");

        if(isdir(srcd))
        {
            if(isdir(trgd))
                for(cQStr &item : QDir(trgd).entryList(QDir::Dirs | QDir::NoDotAndDotDot))
                {
                    QStr trgi(trgd % '/' % item);
                    if(! exist(srcdir % '/' % item) && ! mcheck(trgi % '/')) recrmdir(trgi);
                    if(ThrdKill) return false;
                }
            else
            {
                if(exist(trgd)) rmfile(trgd);
                if(! crtdir(trgd)) return false;
            }

            if(! srcdir.isEmpty())
            {
                QBA mediaitms;
                mediaitms.reserve(10000);
                if(! rodir(mediaitms, srcd)) return false;

                if(! mediaitms.isEmpty())
                {
                    QTS in(&mediaitms, QIODevice::ReadOnly);

                    while(! in.atEnd())
                    {
                        QStr item(in.readLine()), trgi(trgd % '/' % item);

                        if(exist(trgi))
                        {
                            if(stype(trgi) != Isdir)
                            {
                                rmfile(trgi);
                                if(! crtdir(trgi)) return false;
                            }

                            if(! cpertime(srcdir % "/media/" % item, trgi)) goto err_4;
                        }
                        else if(! cpdir(srcdir % "/media/" % item, trgi))
                            goto err_4;

                        if(ThrdKill) return false;
                        continue;
                    err_4:
                        ThrdDbg = "@/media/" % item;
                        return false;
                    }
                }
            }
            else if(isfile("/etc/fstab"))
            {
                QSL dlst(QDir(srcd).entryList(QDir::Dirs | QDir::NoDotAndDotDot));
                QFile file("/etc/fstab");
                if(! file.open(QIODevice::ReadOnly)) return false;

                for(uchar a(0) ; a < dlst.count() ; ++a)
                {
                    cQStr &item(dlst.at(a));
                    if(a > 0 && ! file.open(QIODevice::ReadOnly)) return false;
                    QSL incl{"* /media/" % item % " *", "* /media/" % item % "/*"};

                    while(! file.atEnd())
                    {
                        QStr cline(file.readLine().trimmed().replace('\t', ' ')), fdir;

                        if(! cline.startsWith('#') && like(cline.contains("\\040") ? QStr(cline).replace("\\040", " ") : cline, incl))
                            for(cQStr cdname : mid(cline, instr(cline, "/media/") + 7, instr(cline, " ", instr(cline, "/media/")) - instr(cline, "/media/") - 7).split('/'))
                                if(! cdname.isEmpty())
                                {
                                    QStr trgi(trgd % fdir.append('/' % (cdname.contains("\\040") ? QStr(cdname).replace("\\040", " ") : cdname)));

                                    if(! isdir(trgi) && ! cpdir("/media" % fdir, trgi))
                                    {
                                        ThrdDbg = "@/media" % fdir;
                                        return false;
                                    }
                                }

                        if(ThrdKill) return false;
                    }

                    file.close();
                }
            }

            if(! cpertime(srcd, trgd))
            {
                ThrdDbg = "@/media";
                return false;
            }
        }
        else if(exist(trgd))
            stype(trgd) == Isdir ? recrmdir(trgd) : rmfile(trgd);
    }

    if(exist("/.sbsystemcopy/var/log")) stype("/.sbsystemcopy/var/log") == Isdir ? recrmdir("/.sbsystemcopy/var/log") : rmfile("/.sbsystemcopy/var/log");

    if(isdir(srcdir % "/var/log"))
    {
        if(! crtdir("/.sbsystemcopy/var/log")) return false;
        QBA logitms;
        logitms.reserve(20000);
        QUCL logitmst;
        logitmst.reserve(1000);
        if(! rodir(logitms, logitmst, srcdir % "/var/log")) return false;

        if(! logitmst.isEmpty())
        {
            QSL excl{"*.gz_", "*.old_"};
            lcnt = 0;
            QTS in(&logitms, QIODevice::ReadOnly);

            while(! in.atEnd())
            {
                QStr item(in.readLine());

                switch(logitmst.at(lcnt++)) {
                case Isdir:
                    if(! crtdir("/.sbsystemcopy/var/log/" % item)) goto err_5;
                    break;
                case Isfile:
                    if(! like(item, excl) && (! item.contains('.') || ! isnum(right(item, - rinstr(item, ".")))))
                    {
                        QStr trgi("/.sbsystemcopy/var/log/" % item);
                        crtfile(trgi);
                        if(! cpertime(srcdir % "/var/log/" % item, trgi)) goto err_5;
                    }
                }

                if(ThrdKill) return false;
                continue;
            err_5:
                ThrdDbg = "@/var/log/" % item;
                return false;
            }

            in.seek(0);
            lcnt = 0;

            while(! in.atEnd())
            {
                QStr item(in.readLine());

                if(logitmst.at(lcnt++) == Isdir && ! cpertime(srcdir % "/var/log/" % item, "/.sbsystemcopy/var/log/" % item))
                {
                    ThrdDbg = "@/var/log/" % item;
                    return false;
                }

                if(ThrdKill) return false;
            }
        }

        if(! cpertime(srcdir % "/var/log", "/.sbsystemcopy/var/log"))
        {
            ThrdDbg = "@/var/log";
            return false;
        }
        else if(! cpertime(srcdir % "/var", "/.sbsystemcopy/var"))
        {
            ThrdDbg = "@/var";
            return false;
        }
    }

    if(srcdir == "/.systembacklivepoint" && isdir("/.systembacklivepoint/.systemback"))
    {
        QBA sbitms;
        sbitms.reserve(10000);
        QUCL sbitmst;
        sbitmst.reserve(500);
        if(! rodir(sbitms, sbitmst, "/.systembacklivepoint/.systemback")) return false;
        lcnt = 0;
        QTS in(&sbitms, QIODevice::ReadOnly);

        while(! in.atEnd())
        {
            QStr item(in.readLine());

            switch(sbitmst.at(lcnt++)) {
            case Islink:
                if(item != "etc/fstab" && ! cplink("/.systembacklivepoint/.systemback/" % item, "/.sbsystemcopy/" % item)) goto err_6;
                break;
            case Isfile:
                if(item != "etc/fstab" && ! cpfile("/.systembacklivepoint/.systemback/" % item, "/.sbsystemcopy/" % item)) goto err_6;
            }

            if(ThrdKill) return false;
            continue;
        err_6:
            ThrdDbg = "@/.systemback/" % item;
            return false;
        }

        in.seek(0);
        lcnt = 0;

        while(! in.atEnd())
        {
            QStr item(in.readLine());

            if(sbitmst.at(lcnt++) == Isdir && ! cpertime("/.systembacklivepoint/.systemback/" % item, "/.sbsystemcopy/" % item))
            {
                ThrdDbg = "@/.systemback/" % item;
                return false;
            }

            if(ThrdKill) return false;
        }
    }

    return true;
}

bool sb::thrdlvprpr(bool iudata)
{
    if(ThrdLng[0] > 0) ThrdLng[0] = 0;

    {
        QUCL sitmst;
        sitmst.reserve(1000000);

        for(cQStr &item : {"/bin", "/boot", "/etc", "/lib", "/lib32", "/lib64", "/opt", "/sbin", "/selinux", "/usr", "/initrd.img", "/initrd.img.old", "/vmlinuz", "/vmlinuz.old"})
            if(isdir(item))
            {
                if(! rodir(sitmst, item)) return false;
                ++ThrdLng[0];
            }
            else if(exist(item))
                ++ThrdLng[0];

        ThrdLng[0] += sitmst.count();
    }

    if(! crtdir(sdir[2] % "/.sblivesystemcreate/.systemback") || ! crtdir(sdir[2] % "/.sblivesystemcreate/.systemback/etc")) return false;

    if(isdir("/etc/udev"))
    {
        if(! crtdir(sdir[2] % "/.sblivesystemcreate/.systemback/etc/udev")) return false;

        if(isdir("/etc/udev/rules.d") && (! crtdir(sdir[2] % "/.sblivesystemcreate/.systemback/etc/udev/rules.d") ||
            (isfile("/etc/udev/rules.d/70-persistent-cd.rules") && ! cpfile("/etc/udev/rules.d/70-persistent-cd.rules", sdir[2] % "/.sblivesystemcreate/.systemback/etc/udev/rules.d/70-persistent-cd.rules")) ||
            (isfile("/etc/udev/rules.d/70-persistent-net.rules") && ! cpfile("/etc/udev/rules.d/70-persistent-net.rules", sdir[2] % "/.sblivesystemcreate/.systemback/etc/udev/rules.d/70-persistent-net.rules")) ||
            (! cpertime("/etc/udev/rules.d", sdir[2] % "/.sblivesystemcreate/.systemback/etc/udev/rules.d"))))
        {
            ThrdDbg = "@/etc/udev/rules.d/ ?";
            return false;
        }

        if(! cpertime("/etc/udev", sdir[2] % "/.sblivesystemcreate/.systemback/etc/udev"))
        {
            ThrdDbg = "@/etc/udev";
            return false;
        }
    }

    if((isfile("/etc/fstab") && ! cpfile("/etc/fstab", sdir[2] % "/.sblivesystemcreate/.systemback/etc/fstab")) || ! cpertime("/etc", sdir[2] % "/.sblivesystemcreate/.systemback/etc")) return false;
    if(exist("/.sblvtmp")) stype("/.sblvtmp") == Isdir ? recrmdir("/.sblvtmp") : rmfile("/.sblvtmp");
    if(! crtdir("/.sblvtmp")) return false;

    for(cQStr &cdir : {"/cdrom", "/dev", "/mnt", "/proc", "/run", "/srv", "/sys", "/tmp"})
        if(isdir(cdir))
        {
            if(! cpdir(cdir, "/.sblvtmp" % cdir))
            {
                ThrdDbg = '@' % cdir;
                return false;
            }

            ++ThrdLng[0];
        }

    if(ThrdKill) return false;

    if(! isdir("/media"))
    {
        if(! crtdir("/media")) return false;
    }
    else if(exist("/media/.sblvtmp"))
       stype("/media/.sblvtmp") == Isdir ? recrmdir("/media/.sblvtmp") : rmfile("/media/.sblvtmp");

    if(! crtdir("/media/.sblvtmp") || ! crtdir("/media/.sblvtmp/media")) return false;
    ++ThrdLng[0];
    if(ThrdKill) return false;

    if(isfile("/etc/fstab"))
    {
        QFile file("/etc/fstab");
        if(! file.open(QIODevice::ReadOnly)) return false;
        QSL dlst(QDir("/media").entryList(QDir::Dirs | QDir::NoDotAndDotDot));

        for(uchar a(0) ; a < dlst.count() ; ++a)
        {
            cQStr &item(dlst.at(a));
            if(a > 0 && ! file.open(QIODevice::ReadOnly)) return false;
            QSL incl{"* /media/" % item % " *", "* /media/" % item % "/*"};

            while(! file.atEnd())
            {
                QStr cline(file.readLine().trimmed().replace('\t', ' ')), fdir;

                if(! cline.startsWith('#') && like(cline.contains("\\040") ? QStr(cline).replace("\\040", " ") : cline, incl))
                    for(cQStr cdname : mid(cline, instr(cline, "/media/") + 7, instr(cline, " ", instr(cline, "/media/")) - instr(cline, "/media/") - 7).split('/'))
                        if(! cdname.isEmpty())
                        {
                            QStr sbli("/media/.sblvtmp/media" % fdir.append('/' % (cdname.contains("\\040") ? QStr(cdname).replace("\\040", " ") : cdname)));

                            if(! isdir(sbli))
                            {
                                if(! cpdir("/media" % fdir, sbli))
                                {
                                    ThrdDbg = "@/media" % fdir;
                                    return false;
                                }

                                ++ThrdLng[0];
                            }
                        }

                if(ThrdKill) return false;
            }

            file.close();
        }
    }

    if(exist("/var/.sblvtmp")) stype("/var/.sblvtmp") == Isdir ? recrmdir("/var/.sblvtmp") : rmfile("/var/.sblvtmp");
    if(ThrdKill) return false;
    uint lcnt;

    {
        QBA varitms;
        varitms.reserve(1000000);
        QUCL varitmst;
        varitmst.reserve(50000);
        if(! rodir(varitms, varitmst, "/var")) return false;
        if(! crtdir("/var/.sblvtmp") || ! crtdir("/var/.sblvtmp/var")) return false;
        ++ThrdLng[0];
        QSL elst{"lib/dpkg/lock", "lib/udisks/mtab", "lib/ureadahead/", "log/", "run/", "tmp/"};

        if(! varitmst.isEmpty())
        {
            QSL excl[]{{"+_cache/apt/*", "-*.bin_", "-*.bin.*"}, {"_cache/apt/archives/*", "*.deb_"}, {"_lost+found_", "_lost+found/*", "*/lost+found_", "*/lost+found/*", "_Systemback_", "_Systemback/*", "*/Systemback_", "*/Systemback/*", "*.dpkg-old_", "*~_", "*~/*"}, {"*.gz_", "*.old_"}};
            lcnt = 0;
            QTS in(&varitms, QIODevice::ReadOnly);

            while(! in.atEnd())
            {
                QStr item(in.readLine()), srci("/var/" % item);

                if(exist(srci))
                {
                    if(! like(item, excl[0], Mixed) && ! like(item, excl[1], All) && ! like(item, excl[2]) && ! exclcheck(elst, item))
                        switch(varitmst.at(lcnt)) {
                        case Islink:
                            if(! cplink(srci, "/var/.sblvtmp/var/" % item)) goto err_1;
                            ++ThrdLng[0];
                            break;
                        case Isdir:
                            if(! crtdir("/var/.sblvtmp/var/" % item)) return false;
                            ++ThrdLng[0];
                            break;
                        case Isfile:
                            if(issmfs("/var/.sblvtmp", srci) && ! crthlnk(srci, "/var/.sblvtmp/var/" % item)) goto err_1;
                            ++ThrdLng[0];
                        }
                    else if(item.startsWith("log"))
                        switch(varitmst.at(lcnt)) {
                        case Isdir:
                            if(! crtdir("/var/.sblvtmp/var/" % item)) return false;
                            ++ThrdLng[0];
                            break;
                        case Isfile:
                            if(! like(item, excl[3]) && (! item.contains('.') || ! isnum(right(item, - rinstr(item, ".")))))
                            {
                                QStr sbli("/var/.sblvtmp/var/" % item);
                                crtfile(sbli);
                                if(! cpertime(srci, sbli)) goto err_1;
                                ++ThrdLng[0];
                            }
                        }

                    goto nitem_1;
                err_1:
                    ThrdDbg = "@/var/" % item;
                    return false;
                }

            nitem_1:
                if(ThrdKill) return false;
                ++lcnt;
            }

            in.seek(0);
            lcnt = 0;

            while(! in.atEnd())
            {
                QStr item(in.readLine());

                if(varitmst.at(lcnt++) == Isdir)
                {
                    QStr sbli("/var/.sblvtmp/var/" % item);

                    if(exist(sbli) && ! cpertime("/var/" % item, sbli))
                    {
                        ThrdDbg = "@/var/" % item;
                        return false;
                    }
                }

                if(ThrdKill) return false;
            }
        }
    }

    if(! cpertime("/var", "/var/.sblvtmp/var"))
    {
        ThrdDbg = "@/var";
        return false;
    }

    QSL usrs;

    {
        QFile file("/etc/passwd");
        if(! file.open(QIODevice::ReadOnly)) return false;

        while(! file.atEnd())
        {
            QStr usr(file.readLine().trimmed());
            if(usr.contains(":/home/") && isdir("/home/" % (usr = left(usr, instr(usr, ":") -1)))) usrs.prepend(usr);
        }
    }

    usrs.prepend(isdir("/root") ? "" : nullptr);
    if(ThrdKill) return false;

    bool uhl(dfree("/home") > 104857600 && dfree("/root") > 104857600 && [&usrs] {
            for(uchar a(1) ; a < usrs.count() ; ++a)
                if(! issmfs("/home", "/home/" % usrs.at(a))) return false;

            return true;
        }());

    if(ThrdKill) return false;

    if(uhl)
    {
        if(exist("/home/.sbuserdata")) stype("/home/.sbuserdata") == Isdir ? recrmdir("/home/.sbuserdata") : rmfile("/home/.sbuserdata");
        if(! crtdir("/home/.sbuserdata") || ! crtdir("/home/.sbuserdata/home")) return false;
    }
    else if(! crtdir(sdir[2] % "/.sblivesystemcreate/userdata") || ! crtdir(sdir[2] % "/.sblivesystemcreate/userdata/home"))
        return false;

    ++ThrdLng[0];
    if(ThrdKill) return false;
    QSL elst{".sbuserdata", ".cache/gvfs", ".local/share/Trash/files/", ".local/share/Trash/info/", ".Xauthority", ".ICEauthority"};

    {
        QFile file("/etc/systemback.excludes");
        if(! file.open(QIODevice::ReadOnly)) return false;

        while(! file.atEnd())
        {
            elst.append(file.readLine().trimmed());
            if(ThrdKill) return false;
        }
    }

    QSL excl{"_lost+found_", "_lost+found/*", "*/lost+found_", "*/lost+found/*", "_Systemback_", "_Systemback/*", "*/Systemback_", "*/Systemback/*", "*~_", "*~/*"};

    for(cQStr &usr : usrs)
    {
        if(! usr.isNull())
        {
            QStr usdir;

            if(uhl)
            {
                if(usr.isEmpty())
                {
                    usdir = "/root/.sbuserdata";
                    if(exist(usdir)) stype(usdir) == Isdir ? recrmdir(usdir) : rmfile(usdir);
                    if(! crtdir(usdir) || ! crtdir(usdir.append("/root"))) return false;
                }
                else
                {
                    usdir = "/home/.sbuserdata/home/" % usr;
                    if(! crtdir(usdir)) return false;
                }
            }
            else
            {
                usdir = sdir[2] % (usr.isEmpty() ? "/.sblivesystemcreate/userdata/root" : QStr("/.sblivesystemcreate/userdata/home/" % usr));
                if(! crtdir(usdir)) return false;
            }

            ++ThrdLng[0];
            QStr srcd(usr.isEmpty() ? "/root" : QStr("/home/" % usr));
            QBA useritms;
            useritms.reserve(usr.isEmpty() ? 50000 : 5000000);
            QUCL useritmst;
            useritmst.reserve(usr.isEmpty() ? 1000 : 100000);
            if(! rodir(useritms, useritmst, srcd, ! iudata)) return false;

            if(! useritmst.isEmpty())
            {
                lcnt = 0;
                QTS in(&useritms, QIODevice::ReadOnly);

                while(! in.atEnd())
                {
                    QStr item(in.readLine());

                    if(! like(item, excl) && ! exclcheck(elst, item))
                    {
                        QStr srci(srcd % '/' % item);

                        if(exist(srci))
                        {
                            switch(useritmst.at(lcnt)) {
                            case Islink:
                                if(uhl)
                                {
                                    if(! crthlnk(srci, usdir % '/' % item)) goto err_2;
                                }
                                else if(! cplink(srci, usdir % '/' % item))
                                    goto err_2;

                                ++ThrdLng[0];
                                break;
                            case Isdir:
                                if(! crtdir(usdir % '/' % item)) return false;
                                ++ThrdLng[0];
                                break;
                            case Isfile:
                                if(iudata || QFile(srci).size() <= 8000000)
                                {
                                    if(uhl)
                                    {
                                        if(! crthlnk(srci, usdir % '/' % item)) goto err_2;
                                    }
                                    else if(! cpfile(srci, usdir % '/' % item))
                                        goto err_2;

                                    ++ThrdLng[0];
                                }
                            }

                            goto nitem_2;
                        err_2:
                            ThrdDbg = '@' % srci;
                            return false;
                        }
                    }

                nitem_2:
                    if(ThrdKill) return false;
                    ++lcnt;
                }

                in.seek(0);
                lcnt = 0;

                while(! in.atEnd())
                {
                    QStr item(in.readLine());

                    if(useritmst.at(lcnt++) == Isdir)
                    {
                        QStr sbli(usdir % '/' % item);

                        if(exist(sbli) && ! cpertime(srcd % '/' % item, sbli))
                        {
                            ThrdDbg = '@' % srcd % '/' % item;
                            return false;
                        }
                    }

                    if(ThrdKill) return false;
                }
            }

            if(! iudata && ! usr.isEmpty() && isfile(srcd % "/.config/user-dirs.dirs"))
            {
                QFile file(srcd % "/.config/user-dirs.dirs");
                if(! file.open(QIODevice::ReadOnly)) return false;

                while(! file.atEnd())
                {
                    QStr cline(file.readLine().trimmed()), dir;

                    if(! cline.startsWith('#') && cline.contains("$HOME") && (dir = left(right(cline, - instr(cline, "/")), -1)).length() > 0)
                    {
                        QStr srci(srcd % '/' % dir);

                        if(isdir(srci))
                        {
                            QStr sbld(usdir % '/' % dir);

                            if(! isdir(sbld))
                            {
                                if(! cpdir(srci, sbld))
                                {
                                    ThrdDbg = '@' % srcd % '/' % dir;
                                    return false;
                                }

                                ++ThrdLng[0];
                            }
                        }
                    }

                    if(ThrdKill) return false;
                    continue;
                }

                file.close();
            }

            if(! cpertime(srcd, usdir))
            {
                ThrdDbg = '@' % srcd;
                return false;
            }
        }
    }

    return true;
}
