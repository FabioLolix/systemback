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

#include "sblib.hpp"
#include "libmount.hpp"
#include <QCoreApplication>
#include <QStringBuilder>
#include <QProcess>
#include <QTime>
#include <QDir>
#include <parted/parted.h>
#include <sys/resource.h>
#include <sys/sendfile.h>
#include <blkid/blkid.h>
#include <sys/statvfs.h>
#include <sys/utsname.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <linux/fs.h>
#include <unistd.h>
#include <dirent.h>
#include <signal.h>
#include <utime.h>
#include <fcntl.h>

sb sb::SBThrd;
QSL *sb::ThrdSlst;
QStr sb::ThrdStr[3], sb::ThrdDbg, sb::sdir[3], sb::schdle[7], sb::pnames[15];
ullong sb::ThrdLng[2]{0, 0};
int sb::sblock[3];
uchar sb::ThrdType, sb::ThrdChr, sb::pnumber(0);
schar sb::Progress(-1);
bool sb::ThrdBool, sb::ExecKill(true), sb::ThrdKill(true), sb::ThrdRslt;

sb::sb(QThread *parent) : QThread(parent)
{
    qputenv("PATH", "/usr/lib/systemback:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin");
    umask(0);
}

void sb::print(cQStr &txt)
{
    QTS(stdout) << "\033[1m" % txt % "\033[0m";
}

void sb::error(cQStr &txt)
{
    QTS(stderr) << "\033[1;31m" % txt % "\033[0m";
}

QStr sb::getarch()
{
    switch(sizeof(char *)) {
    case 4:
        return "i386";
    case 8:
        return "amd64";
    default:
        return "arch?";
    }
}

QStr sb::ckname()
{
    struct utsname snfo;
    uname(&snfo);
    return snfo.release;
}

QStr sb::rndstr(uchar vlen)
{
    QStr val, chrs("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz./"), chr;
    uchar clen(vlen == 16 ? 64 : 62);
    qsrand(QTime::currentTime().msecsSinceStartOfDay());

    do {
        chr = chrs.mid(qrand() % clen, 1);
        if(! val.endsWith(chr)) val.append(chr);
    } while(val.length() < vlen);

    return val;
}

QStr sb::fload(cQStr &path)
{
    QFile file(path);
    if(! file.open(QIODevice::ReadOnly)) return nullptr;
    return file.readAll();
}

ullong sb::dfree(cQStr &path)
{
    struct statvfs dstat;
    return statvfs(path.toStdString().c_str(), &dstat) == -1 ? 0 : dstat.f_bavail * dstat.f_bsize;
}

bool sb::pisrng(cQStr &pname, ushort *pid)
{
    DIR *dir(opendir("/proc"));
    struct dirent *ent;

    while((ent = readdir(dir)))
        if(! like(ent->d_name, {"_._", "_.._"}) && ent->d_type == DT_DIR && isnum(QStr(ent->d_name)) && islink("/proc/" % QStr(ent->d_name) % "/exe") && QFile::readLink("/proc/" % QStr(ent->d_name) % "/exe").endsWith('/' % pname))
        {
            if(pid) *pid = QStr(ent->d_name).toUShort();
            closedir(dir);
            return true;
        }

    closedir(dir);
    return false;
}

bool sb::crtfile(cQStr &path, cQStr &txt)
{
    if(! ilike(stype(path), {Notexist, Isfile}) || ! isdir(left(path, rinstr(path, "/") - 1))) return false;
    QFile file(path);
    if(! file.open(QFile::WriteOnly | QFile::Truncate) || file.write(txt.toLocal8Bit()) == -1) return false;
    file.flush();
    return true;
}

bool sb::islnxfs(cQStr &dirpath)
{
    QStr fpath(dirpath % '/' % rndstr() % "_sbdirtestfile");
    if(! crtfile(fpath) || chmod(fpath.toStdString().c_str(), 0776) == -1) return false;
    struct stat fstat;
    bool err(stat(fpath.toStdString().c_str(), &fstat) == -1);
    QFile::remove(fpath);
    return err ? false : fstat.st_mode == 33278;
}

bool sb::lock(uchar type)
{
    cchar *lfile;

    switch(type) {
    case Sblock:
        lfile = isdir("/run") ? "/run/systemback.lock" : "/var/run/systemback.lock";
        break;
    case Dpkglock:
        lfile = "/var/lib/dpkg/lock";
        break;
    case Schdlrlock:
        lfile = isdir("/run") ? "/run/sbscheduler.lock" : "/var/run/sbscheduler.lock";
        break;
    default:
        return false;
    }

    return (sblock[type] = open(lfile, O_RDWR | O_CREAT, 0644)) != -1 && lockf(sblock[type], F_TLOCK, 0) == 0;
}

void sb::unlock(uchar type)
{
    if(type < 3) close(sblock[type]);
}

bool sb::ickernel()
{
    QStr ckernel(ckname()), fend[2]{"order", "builtin"};

    for(uchar a(0) ; a < 2 ; ++a)
        if(isfile("/lib/modules/" % ckernel % "/modules." % fend[a]))
        {
            QFile file("/lib/modules/" % ckernel % "/modules." % fend[a]);

            if(file.open(QIODevice::ReadOnly))
                while(! file.atEnd())
                    if(like(file.readLine().trimmed(), {"*aufs.ko_", "*overlayfs.ko_", "*unionfs.ko_"})) return true;
        }

    return false;
}

bool sb::efiprob()
{
    if(getarch() == "amd64")
    {
        if(isdir("/sys/firmware/efi")) return true;
        QStr ckernel(ckname());

        if(isfile("/lib/modules/" % ckernel % "/modules.builtin"))
        {
            QFile file("/lib/modules/" % ckernel % "/modules.builtin");

            if(file.open(QIODevice::ReadOnly))
                while(! file.atEnd())
                    if(file.readLine().trimmed().endsWith("efivars.ko")) return false;
        }

        if(isfile("/proc/modules") && ! fload("/proc/modules").contains("efivars ") && isfile("/lib/modules/" % ckernel % "/modules.order"))
        {
            QFile file("/lib/modules/" % ckernel % "/modules.order");

            if(file.open(QIODevice::ReadOnly))
                while(! file.atEnd())
                {
                    QStr cline(file.readLine().trimmed());
                    if(cline.endsWith("efivars.ko") && isfile("/lib/modules/" % ckernel % '/' % cline) && exec("modprobe efivars", nullptr, true) == 0 && isdir("/sys/firmware/efi")) return true;
                }
        }
    }

    return false;
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

void sb::cfgwrite()
{
    crtfile("/etc/systemback.conf", "storagedir=" % sdir[0] % "\nliveworkdir=" % sdir[2] % "\npointsnumber=" % QStr::number(pnumber) % "\ntimer=" % schdle[0] % "\nschedule=" % schdle[1] % ":" % schdle[2] % ":" % schdle[3] % ":" % schdle[4] % "\nsilentmode=" % schdle[5] % "\nwindowposition=" % schdle[6] % '\n');
}

void sb::xrestart()
{
    ushort pid;
    if(pisrng("Xorg", &pid)) kill(pid, SIGTERM);
}

bool sb::execsrch(cQStr &fname, cQStr &ppath)
{
    for(cQStr &path : QStr(qgetenv("PATH")).split(':'))
    {
        QStr fpath(ppath % path % '/' % fname);
        if(isfile(fpath)) return access(fpath, Exec);
    }

    return false;
}

uchar sb::exec(cQStr &cmd, cQStr &envv, bool silent, bool bckgrnd)
{
    if(ExecKill) ExecKill = false;
    uchar rprcnt(0);

    if(cmd.startsWith("mksquashfs"))
        rprcnt = 1;
    else if(cmd.startsWith("genisoimage"))
        rprcnt = 2;
    else if(cmd.startsWith("tar -cf"))
        rprcnt = 3;
    else if(like(cmd, {"_tar -xf*", "*--no-same-permissions_"}, All))
        rprcnt = 4;

    if(rprcnt > 0) Progress = 0;
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
            inum += proc.readAllStandardOutput().count('\n');
            cperc = (inum * 100 + 50) / ThrdLng[0];
            if(Progress < cperc) Progress = cperc;
            QTS(stderr) << QStr(proc.readAllStandardError());

            if(dfree(sdir[2]) < 104857600)
            {
                proc.kill();
                return 255;
            }

            break;
        case 2:
        {
            QStr pout(proc.readAllStandardError());
            cperc = mid(pout, rinstr(pout, "%") - 5, 2).toUShort();
            if(Progress < cperc) Progress = cperc;
            break;
        }
        case 3:
            if(ThrdLng[0] == 0)
            {
                QStr itms;
                rodir(itms, sdir[2] % "/.sblivesystemcreate");
                QTS in(&itms, QIODevice::ReadOnly);

                while(! in.atEnd())
                {
                    QStr line(in.readLine()), item(right(line, -1));
                    if(line.left(1).toUShort() == Isfile) ThrdLng[0] += fsize(sdir[2] % "/.sblivesystemcreate/" % item);
                }
            }
            else if(isfile(ThrdStr[0]))
            {
                cperc = (fsize(ThrdStr[0]) * 100 + 50) / ThrdLng[0];
                if(Progress < cperc) Progress = cperc;
            }

            break;
        case 4:
            QStr itms;
            rodir(itms, ThrdStr[0]);
            QTS in(&itms, QIODevice::ReadOnly);
            ullong size(0);

            while(! in.atEnd())
            {
                QStr line(in.readLine()), item(right(line, -1));
                if(line.left(1).toUShort() == Isfile) size += fsize(ThrdStr[0] % '/' % item);
            }

            cperc = (size * 100 + 50) / ThrdLng[0];
            if(Progress < cperc) Progress = cperc;
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

    if(itm.startsWith("/dev/"))
    {
        if(QStr('\n' % mnts).contains('\n' % itm % (itm.length() > 8 ? " " : nullptr)))
            return true;
        else
        {
            blkid_probe pr(blkid_new_probe_from_filename(itm.toStdString().c_str()));
            blkid_do_probe(pr);
            cchar *uuid("");
            blkid_probe_lookup_value(pr, "UUID", &uuid, nullptr);
            blkid_free_probe(pr);
            return QStr(uuid).isEmpty() ? false : QStr('\n' % mnts).contains("\n/dev/disk/by-uuid/" % QStr(uuid) % ' ');
        }
    }
    else if(itm.endsWith('/') && itm.length() > 1)
        return mnts.contains(' ' % left(itm, -1));
    else
        return mnts.contains(' ' % itm % ' ');
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
                    QFile::rename(sdir[1] % (a > 10 ? QStr("/H0" % QStr::number(a - 9)) : QStr(a < 9 ? "/S0" : "/S") % QStr::number(a + 1)) % '_' % pnames[a], sdir[1] % (a > 10 ? QStr("/H0" % QStr::number(a - 10)) : "/S0" % QStr::number(a)) % '_' % pnames[a]);
                    if(! rerun) rerun = true;
                }
        }
    } while(rerun);
}

void sb::supgrade(cQSL &estr)
{
    exec("apt-get update");

    for(;;)
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
                                    if(ritem.startsWith("vmlinuz-" % subk % '-')) rklist.append(' ' % subk % "-*");
                            }
                        }
                    }
            }

            uchar cproc(rklist.isEmpty() ? 0 : exec("apt-get autoremove --purge " % rklist));

            if(ilike(cproc, {0, 1}))
            {
                QStr iplist;
                QProcess proc;
                proc.start("dpkg -l", QProcess::ReadOnly);

                while(proc.state() == QProcess::Starting || proc.state() == QProcess::Running)
                {
                    msleep(10);
                    qApp->processEvents();
                }

                QStr sout(proc.readAllStandardOutput());
                QTS in(&sout, QIODevice::ReadOnly);

                while(! in.atEnd())
                {
                    QStr line(in.readLine());
                    if(line.startsWith("rc")) iplist.append(' ' % mid(line, 5, instr(line, " ", 5) - 5));
                }

                if(! iplist.isEmpty()) exec("dpkg --purge " % iplist);
                exec("apt-get clean");

                {
                    QSL dlst(QDir("/var/cache/apt").entryList(QDir::Files));

                    for(uchar a(0) ; a < dlst.count() ; ++a)
                    {
                        cQStr &item(dlst.at(a));
                        if(item.contains(".bin.")) QFile::remove("/var/cache/apt/" % item);
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

void sb::cfgread()
{
    bool cfgupdt(false);

    if(isfile("/etc/systemback.conf"))
    {
        QFile file("/etc/systemback.conf");

        if(file.open(QIODevice::ReadOnly))
            while(! file.atEnd())
            {
                QStr cline(file.readLine().trimmed()), cval(right(cline, - instr(cline, "=")));

                if(cline.startsWith("storagedir="))
                    sdir[0] = cval;
                else if(cline.startsWith("liveworkdir="))
                    sdir[2] = cval;
                else if(cline.startsWith("schedule="))
                {
                    QSL vals(cval.split(':'));
                    for(uchar a(0) ; ! ilike(a, QSIL() << vals.count() << 4) ; ++a) schdle[a + 1] = vals.at(a);
                }
                else if(cline.startsWith("pointsnumber="))
                    pnumber = cval.toUShort();
                else if(cline.startsWith("timer="))
                    schdle[0] = cval;
                else if(cline.startsWith("silentmode="))
                    schdle[5] = cval;
                else if(cline.startsWith("windowposition="))
                    schdle[6] = cval;
            }
    }

    if(sdir[0].isEmpty())
    {
        sdir[0] = "/home";
        cfgupdt = true;
        if(! isdir("/home/Systemback")) QDir().mkdir("/home/Systemback");
        if(! isfile("/home/Systemback/.sbschedule")) crtfile("/home/Systemback/.sbschedule");
    }
    else if(! isdir(sdir[0] % "/Systemback") && isdir(sdir[0]) && QDir().mkdir(sdir[0] % "/Systemback"))
        crtfile(sdir[0] % "/Systemback/.sbschedule");

    if(sdir[2].isEmpty())
    {
        sdir[2] = "/home";
        if(! cfgupdt) cfgupdt = true;
    }

    if(! like(schdle[0], {"_on_", "_off_"}))
    {
        schdle[0] = "off";
        if(! cfgupdt) cfgupdt = true;
    }

    if(schdle[1].isEmpty() || schdle[2].isEmpty() || schdle[3].isEmpty() || schdle[4].isEmpty())
    {
        schdle[1] = '1';
        schdle[2] = schdle[3] = '0';
        schdle[4] = "10";
        if(! cfgupdt) cfgupdt = true;
    }
    else if(schdle[1].toUShort() > 7 || schdle[2].toUShort() > 23 || schdle[3].toUShort() > 59 || schdle[4].toUShort() < 10 || schdle[4].toUShort() > 99)
    {
        schdle[1] = '1';
        schdle[2] = schdle[3] = '0';
        schdle[4] = "10";
        if(! cfgupdt) cfgupdt = true;
    }
    else if(schdle[1].toUShort() == 0 && schdle[2].toUShort() == 0 && schdle[3].toUShort() < 30)
    {
        schdle[3] = "30";
        if(! cfgupdt) cfgupdt = true;
    }

    if(! like(schdle[5], {"_on_", "_off_"}))
    {
        schdle[5] = "off";
        if(! cfgupdt) cfgupdt = true;
    }

    if(! like(schdle[6], {"_topleft_", "_topright_", "_center_", "_bottomleft_", "_bottomright_"}))
    {
        schdle[6] = "topright";
        if(! cfgupdt) cfgupdt = true;
    }

    if(pnumber < 3 || pnumber > 10)
    {
        pnumber = 5;
        if(! cfgupdt) cfgupdt = true;
    }

    sdir[1] = sdir[0] % "/Systemback";
    if(cfgupdt) cfgwrite();
}

bool sb::copy(cQStr &srcfile, cQStr &newfile)
{
    if(! isfile(srcfile)) return false;
    ThrdType = Copy;
    ThrdStr[0] = srcfile;
    ThrdStr[1] = newfile;
    SBThrd.start();
    thrdelay();
    return ThrdRslt;
}

QStr sb::ruuid(cQStr &part)
{
    ThrdType = Ruuid;
    ThrdStr[0] = part;
    SBThrd.start();
    thrdelay();
    return ThrdStr[1];
}

bool sb::srestore(uchar mthd, cQStr &usr, cQStr &srcdir, cQStr &trgt, bool sfstab)
{
    ThrdType = Srestore;
    ThrdChr = mthd;
    ThrdStr[0] = usr;
    ThrdStr[1] = srcdir;
    ThrdStr[2] = trgt;
    ThrdBool = sfstab;
    SBThrd.start();
    thrdelay();
    return ThrdRslt;
}

bool sb::mkpart(cQStr &dev, ullong start, ullong len, uchar type)
{
    if(dev.length() > 9) return false;
    ThrdType = Mkpart;
    ThrdStr[0] = dev;
    ThrdLng[0] = start;
    ThrdLng[1] = len;
    ThrdChr = type;
    SBThrd.start();
    thrdelay();
    return ThrdRslt;
}

bool sb::mount(cQStr &dev, cQStr &mpoint, cQStr &moptns)
{
    ThrdType = Mount;
    ThrdStr[0] = dev;
    ThrdStr[1] = mpoint;
    ThrdStr[2] = moptns;
    SBThrd.start();
    thrdelay();
    return ThrdRslt;
}

bool sb::scopy(uchar mthd, cQStr &usr, cQStr &srcdir)
{
    ThrdType = Scopy;
    ThrdChr = mthd;
    ThrdStr[0] = usr;
    ThrdStr[1] = srcdir;
    SBThrd.start();
    thrdelay();
    return ThrdRslt;
}

bool sb::crtrpoint(cQStr &sdir, cQStr &pname)
{
    ThrdType = Crtrpoint;
    ThrdStr[0] = sdir;
    ThrdStr[1] = pname;
    SBThrd.start();
    thrdelay();
    return ThrdRslt;
}

bool sb::setpflag(cQStr &part, cQStr &flag)
{
    if(part.length() < 9) return false;
    ThrdType = Setpflag;
    ThrdStr[0] = part;
    ThrdStr[1] = flag;
    SBThrd.start();
    thrdelay();
    return ThrdRslt;
}

bool sb::lvprpr(bool iudata)
{
    ThrdType = Lvprpr;
    ThrdBool = iudata;
    SBThrd.start();
    thrdelay();
    return ThrdRslt;
}

bool sb::mkptable(cQStr &dev, cQStr &type)
{
    if(dev.length() > 9) return false;
    ThrdType = Mkptable;
    ThrdStr[0] = dev;
    ThrdStr[1] = type;
    SBThrd.start();
    thrdelay();
    return ThrdRslt;
}

bool sb::remove(cQStr &path)
{
    ThrdType = Remove;
    ThrdStr[0] = path;
    SBThrd.start();
    thrdelay();
    return ThrdRslt;
}

bool sb::umount(cQStr &dev)
{
    ThrdType = Umount;
    ThrdStr[0] = dev;
    SBThrd.start();
    thrdelay();
    return ThrdRslt;
}

void sb::readlvdevs(QSL &strlst)
{
    ThrdType = Readlvdevs;
    ThrdSlst = &strlst;
    SBThrd.start();
    thrdelay();
}

void sb::readprttns(QSL &strlst)
{
    ThrdType = Readprttns;
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
    ThrdType = Delpart;
    ThrdStr[0] = part;
    SBThrd.start();
    thrdelay();
}

inline QStr sb::rlink(cQStr &path, ushort blen)
{
    char rpath[blen];
    rpath[readlink(path.toStdString().c_str(), rpath, blen)] = '\0';
    return rpath;
}

inline ullong sb::psalign(ullong start, ushort ssize)
{
    if(start <= 1048576 / ssize) return 1048576 / ssize;
    ushort rem(start % (1048576 / ssize));
    return rem > 0 ? start + 1048576 / ssize - rem : start;
}

inline ullong sb::pealign(ullong end, ushort ssize)
{
    ushort rem(end % (1048576 / ssize));
    return rem > 0 ? rem < (1048576 / ssize) - 1 ? end - rem - 1 : end : end - 1;
}

inline ullong sb::devsize(cQStr &dev)
{
    ullong bsize;
    int odev;
    bool err;

    if(! (err = (odev = open(dev.toStdString().c_str(), O_RDONLY)) == -1))
    {
        if(ioctl(odev, BLKGETSIZE64, &bsize) == -1) err = true;
        close(odev);
    }

    return err ? 0 : bsize;
}

inline uchar sb::fcomp(cQStr &file1, cQStr &file2)
{
    struct stat fstat[2];
    if(ilike(-1, QSIL() << stat(file1.toStdString().c_str(), &fstat[0]) << stat(file2.toStdString().c_str(), &fstat[1]))) return 0;
    if(fstat[0].st_size == fstat[1].st_size && fstat[0].st_mtim.tv_sec == fstat[1].st_mtim.tv_sec) return fstat[0].st_mode == fstat[1].st_mode && fstat[0].st_uid == fstat[1].st_uid && fstat[0].st_gid == fstat[1].st_gid ? 2 : 1;
    return 0;
}

inline bool sb::cpertime(cQStr &sourcepath, cQStr &destpath)
{
    struct stat istat[2];
    if(ilike(-1, QSIL() << stat(sourcepath.toStdString().c_str(), &istat[0]) << stat(destpath.toStdString().c_str(), &istat[1]))) return false;

    if(istat[0].st_uid != istat[1].st_uid || istat[0].st_gid != istat[1].st_gid)
    {
        if(chown(destpath.toStdString().c_str(), istat[0].st_uid, istat[0].st_gid) == -1 || ((istat[0].st_mode != (istat[0].st_mode & ~(S_ISUID | S_ISGID)) || istat[0].st_mode != istat[1].st_mode) && chmod(destpath.toStdString().c_str(), istat[0].st_mode) == -1)) return false;
    }
    else if(istat[0].st_mode != istat[1].st_mode && chmod(destpath.toStdString().c_str(), istat[0].st_mode) == -1)
        return false;

    if(istat[0].st_atim.tv_sec != istat[1].st_atim.tv_sec || istat[0].st_mtim.tv_sec != istat[1].st_mtim.tv_sec)
    {
        struct utimbuf sitimes;
        sitimes.actime = istat[0].st_atim.tv_sec;
        sitimes.modtime = istat[0].st_mtim.tv_sec;
        if(utime(destpath.toStdString().c_str(), &sitimes) == -1) return false;
    }

    return true;
}

inline bool sb::cplink(cQStr &srclink, cQStr &newlink)
{
    struct stat sistat;
    if(lstat(srclink.toStdString().c_str(), &sistat) == -1 || ! S_ISLNK(sistat.st_mode) || ! QFile::link(rlink(srclink, sistat.st_size), newlink)) return false;
    struct timeval sitimes[2];
    sitimes[0].tv_sec = sistat.st_atim.tv_sec;
    sitimes[1].tv_sec = sistat.st_mtim.tv_sec;
    sitimes[0].tv_usec = sitimes[1].tv_usec = 0;
    return lutimes(newlink.toStdString().c_str(), sitimes) == 0;
}

inline bool sb::cpfile(cQStr &srcfile, cQStr &newfile)
{
    struct stat fstat;
    if(stat(srcfile.toStdString().c_str(), &fstat) == -1) return false;
    int src, dst;
    if((src = open(srcfile.toStdString().c_str(), O_RDONLY | O_NOATIME)) == -1) return false;
    bool err;

    if(! (err = (dst = creat(newfile.toStdString().c_str(), fstat.st_mode)) == -1))
    {
        if(fstat.st_size > 0)
        {
            llong size(0);

            do {
                llong csize(size);
                if((size += sendfile(dst, src, nullptr, fstat.st_size - size)) <= csize) err = true;
            } while(! err && size < fstat.st_size);
        }

        close(dst);
    }

    close(src);
    if(err || (fstat.st_uid + fstat.st_gid > 0 && (chown(newfile.toStdString().c_str(), fstat.st_uid, fstat.st_gid) == -1 || (fstat.st_mode != (fstat.st_mode & ~(S_ISUID | S_ISGID)) && chmod(newfile.toStdString().c_str(), fstat.st_mode) == -1)))) return false;
    struct utimbuf sftimes;
    sftimes.actime = fstat.st_atim.tv_sec;
    sftimes.modtime = fstat.st_mtim.tv_sec;
    return utime(newfile.toStdString().c_str(), &sftimes) == 0;
}

inline bool sb::cpdir(cQStr &srcdir, cQStr &newdir)
{
    struct stat dstat;
    if(stat(srcdir.toStdString().c_str(), &dstat) == -1 || ! S_ISDIR(dstat.st_mode) || mkdir(newdir.toStdString().c_str(), dstat.st_mode) == -1 || (dstat.st_uid + dstat.st_gid > 0 && (chown(newdir.toStdString().c_str(), dstat.st_uid, dstat.st_gid) == -1 || (dstat.st_mode != (dstat.st_mode & ~(S_ISUID | S_ISGID)) && chmod(newdir.toStdString().c_str(), dstat.st_mode) == -1)))) return false;
    struct utimbuf sdtimes;
    sdtimes.actime = dstat.st_atim.tv_sec;
    sdtimes.modtime = dstat.st_mtim.tv_sec;
    return utime(newdir.toStdString().c_str(), &sdtimes) == 0;
}

inline bool sb::exclcheck(cQSL &elist, cQStr &item)
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

inline bool sb::issmfs(cQStr &item1, cQStr &item2)
{
    struct stat istat[2];
    return ilike(-1, QSIL() << stat(item1.toStdString().c_str(), &istat[0]) << stat(item2.toStdString().c_str(), &istat[1])) ? false : istat[0].st_dev == istat[1].st_dev;
}

inline bool sb::lcomp(cQStr &link1, cQStr &link2)
{
    struct stat istat[2];
    if(ilike(-1, QSIL() << lstat(link1.toStdString().c_str(), &istat[0]) << lstat(link2.toStdString().c_str(), &istat[1])) || ! S_ISLNK(istat[0].st_mode) || ! S_ISLNK(istat[1].st_mode) || istat[0].st_mtim.tv_sec != istat[1].st_mtim.tv_sec) return false;
    QStr lnk(rlink(link1, istat[0].st_size));
    return ! lnk.isEmpty() && lnk == rlink(link2, istat[1].st_size);
}

inline bool sb::isnum(cQStr &txt)
{
    for(uchar a(0) ; a < txt.length() ; ++a)
        if(! txt.at(a).isDigit()) return false;

    return ! txt.isEmpty();
}

inline bool sb::rodir(QStr &str, cQStr &path, bool hidden, uchar oplen)
{
    QStr prepath(str.isEmpty() ? nullptr : QStr(right(path, - (oplen == 1 ? 1 : oplen + 1)) % '/'));
    DIR *dir(opendir(path.toStdString().c_str()));
    struct dirent *ent;

    while(! ThrdKill && (ent = readdir(dir)))
        if(! like(ent->d_name, {"_._", "_.._"}) && (! hidden || QStr(ent->d_name).startsWith('.')))
        {
            switch(ent->d_type) {
            case DT_LNK:
                str.append(QStr::number(Islink) % prepath % QStr(ent->d_name) % '\n');
                break;
            case DT_DIR:
                str.append(QStr::number(Isdir) % prepath % QStr(ent->d_name) % '\n');
                rodir(str, path % '/' % QStr(ent->d_name), false, (oplen == 0 ? path.length() : oplen));
                break;
            case DT_REG:
                str.append(QStr::number(Isfile) % prepath % QStr(ent->d_name) % '\n');
                break;
            case DT_UNKNOWN:
                QStr fpath(path % '/' % QStr(ent->d_name));

                switch(stype(fpath)) {
                case Islink:
                    str.append(QStr::number(Islink) % prepath % QStr(ent->d_name) % '\n');
                    break;
                case Isdir:
                    str.append(QStr::number(Isdir) % prepath % QStr(ent->d_name) % '\n');
                    rodir(str, fpath, false, (oplen == 0 ? path.length() : oplen));
                    break;
                case Isfile:
                    str.append(QStr::number(Isfile) % prepath % QStr(ent->d_name) % '\n');
                }
            }
        }

    closedir(dir);
    return ! ThrdKill;
}

inline bool sb::odir(QSL &strlst, cQStr &path, bool hidden)
{
    DIR *dir(opendir(path.toStdString().c_str()));
    struct dirent *ent;

    while(! ThrdKill && (ent = readdir(dir)))
        if(! like(ent->d_name, {"_._", "_.._"}) && (! hidden || QStr(ent->d_name).startsWith('.'))) strlst.append(ent->d_name);

    closedir(dir);
    return ! ThrdKill;
}

inline bool sb::recrmdir(cQStr &path, bool slimit)
{
    DIR *dir(opendir(path.toStdString().c_str()));
    struct dirent *ent;

    while(! ThrdKill && (ent = readdir(dir)))
        if(! like(ent->d_name, {"_._", "_.._"}))
        {
            QStr fpath(path % '/' % QStr(ent->d_name));

            switch(ent->d_type) {
            case DT_UNKNOWN:
                switch(stype(fpath)) {
                case Isdir:
                    if(! recrmdir(fpath, slimit))
                    {
                        closedir(dir);
                        return false;
                    }

                    QDir().rmdir(fpath);
                    break;
                case Isfile:
                    if(slimit && QFile(fpath).size() > 8000000) break;
                default:
                    QFile::remove(fpath);
                }

                break;
            case DT_DIR:
                if(! recrmdir(fpath, slimit))
                {
                    closedir(dir);
                    return false;
                }

                QDir().rmdir(fpath);
                break;
            case DT_REG:
                if(slimit && QFile(fpath).size() > 8000000) break;
            default:
                QFile::remove(fpath);
            }
        }

    closedir(dir);
    return ThrdKill ? false : QDir().rmdir(path) ? true : slimit;
}

inline bool sb::fspchk(cQStr &dir)
{
    return dfree(dir.isEmpty() ? "/" : dir) > 10485760;
}

void sb::run()
{
    if(ThrdKill) ThrdKill = false;
    if(! ThrdDbg.isEmpty()) ThrdDbg.clear();

    switch(ThrdType) {
    case Remove:
        switch(stype(ThrdStr[0])) {
        case Isdir:
            ThrdRslt = recrmdir(ThrdStr[0]);
            break;
        default:
            ThrdRslt = QFile::remove(ThrdStr[0]);
        }

        break;
    case Copy:
        ThrdRslt = cpfile(ThrdStr[0], ThrdStr[1]);
        break;
    case Sync:
        sync();
        break;
    case Mount:
    {
        struct libmnt_context *mcxt(mnt_new_context());
        mnt_context_set_source(mcxt, ThrdStr[0].toStdString().c_str());
        mnt_context_set_target(mcxt, ThrdStr[1].toStdString().c_str());

        if(! ThrdStr[2].isEmpty())
            mnt_context_set_options(mcxt, ThrdStr[2].toStdString().c_str());
        else if(isdir(ThrdStr[0]))
            mnt_context_set_options(mcxt, "bind");

        ThrdRslt = mnt_context_mount(mcxt) == 0;
        mnt_free_context(mcxt);
        break;
    }
    case Umount:
    {
        struct libmnt_context *ucxt(mnt_new_context());
        mnt_context_set_target(ucxt, ThrdStr[0].toStdString().c_str());
        mnt_context_enable_force(ucxt, true);
        mnt_context_enable_lazy(ucxt, true);
        ThrdRslt = mnt_context_umount(ucxt) == 0;
        mnt_free_context(ucxt);
        break;
    }
    case Readprttns:
    {
        for(cQStr &spath : QDir("/dev").entryList(QDir::System))
        {
            QStr path("/dev/" % spath);

            if(like(path, {"_/dev/sd*", "_/dev/hd*"}) && path.length() == 8 && devsize(path) > 536870911)
            {
                PedDevice *dev(ped_device_get(path.toStdString().c_str()));
                PedDisk *dsk(ped_disk_new(dev));
                QStr type(dsk == nullptr ? nullptr : dsk->type->name), dtxt(path % '\n' % QStr::number(dev->length * dev->sector_size) % '\n');

                if(type == "msdos")
                    ThrdSlst->append(dtxt % QStr::number(MSDOS));
                else if(type == "gpt")
                    ThrdSlst->append(dtxt % QStr::number(GPT));
                else
                {
                    ThrdSlst->append(dtxt % QStr::number(Clear));

                    if(type.isNull())
                        goto next_2;
                    else
                        goto next_1;
                }

                {
                    PedPartition *prt(nullptr);
                    QLIL egeom;

                    while((prt = ped_disk_next_partition(dsk, prt)))
                        if(prt->geom.length >= 1048576 / dev->sector_size)
                        {
                            if(prt->num > 0)
                            {
                                QStr ppath(path % QStr::number(prt->num));

                                if(stype(ppath) == Isblock)
                                {
                                    if(prt->type == PED_PARTITION_EXTENDED)
                                    {
                                        ThrdSlst->append(ppath % '\n' % QStr::number(prt->geom.length * dev->sector_size) % '\n' % QStr::number(Extended) % '\n' % QStr::number(prt->geom.start * dev->sector_size));
                                        egeom << prt->geom.start << prt->geom.end;
                                    }
                                    else
                                    {
                                        if(egeom.count() > 2)
                                        {
                                            ullong start(psalign(egeom.at(2), dev->sector_size));
                                            ThrdSlst->append(path % "?\n" % QStr::number((pealign(egeom.at(3), dev->sector_size) - start + 1) * dev->sector_size - (prt->type == PED_PARTITION_LOGICAL ? 2097152 : 1048576 - dev->sector_size)) % '\n' % QStr::number(Emptyspace) % '\n' % QStr::number(start * dev->sector_size + 1048576));
                                            egeom.removeAt(3);
                                            egeom.removeAt(2);
                                        }

                                        blkid_probe pr(blkid_new_probe_from_filename(ppath.toStdString().c_str()));
                                        blkid_do_probe(pr);
                                        cchar *uuid(""), *fstype("?"), *label("");

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
                                    ullong start(psalign(egeom.at(2), dev->sector_size));
                                    ThrdSlst->append(path % "?\n" % QStr::number((pealign(egeom.at(3), dev->sector_size) - start + 1) * dev->sector_size - 1048576) % '\n' % QStr::number(Emptyspace) % '\n' % QStr::number(start * dev->sector_size + 1048576));
                                    egeom.clear();
                                }

                                llong fgeom[2]{llong(prt->geom.start < 1048576 / dev->sector_size ? 1048576 / dev->sector_size : psalign(prt->geom.start, dev->sector_size)), llong(prt->next && prt->next->type == PED_PARTITION_METADATA ? type == "msdos" ? prt->next->geom.end : prt->next->geom.end - (34816 / dev->sector_size * 10 + 5) / 10 : pealign(prt->geom.end, dev->sector_size))};
                                if(fgeom[1] - fgeom[0] > 1048576 / dev->sector_size - 2) ThrdSlst->append(path % "?\n" % QStr::number((fgeom[1] - fgeom[0] + 1) * dev->sector_size) % '\n' % QStr::number(Freespace) % '\n' % QStr::number(fgeom[0] * dev->sector_size));
                            }
                            else if(! egeom.isEmpty())
                            {
                                if(prt->geom.end <= egeom.at(1))
                                {
                                    switch(egeom.count()) {
                                    case 2:
                                        if(prt->geom.length >= 3145728 / dev->sector_size) egeom << (prt->geom.start - egeom.at(0) < 1048576 / dev->sector_size ? egeom.at(0) : prt->geom.start) << prt->geom.end + (prt->geom.end == egeom.at(1) ? 1 : 0);
                                        break;
                                    default:
                                        egeom.replace(3, prt->geom.end + (prt->geom.end == egeom.at(1) ? 1 : 0));
                                    }
                                }
                                else
                                    egeom.clear();
                            }
                        }

                    if(egeom.count() > 2)
                    {
                        ullong start(psalign(egeom.at(2), dev->sector_size));
                        ThrdSlst->append(path % "?\n" % QStr::number((pealign(egeom.at(3), dev->sector_size) - start + 1) * dev->sector_size - 1048576) % '\n' % QStr::number(Emptyspace) % '\n' % QStr::number(start * dev->sector_size + 1048576));
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
        QStr fstab(fload("/etc/fstab"));

        for(cQStr &item : QDir("/dev/disk/by-id").entryList(QDir::Files))
        {
            if(item.startsWith("usb-") && islink("/dev/disk/by-id/" % item))
            {
                QStr path(rlink("/dev/disk/by-id/" % item, 10));

                if(! path.isEmpty())
                {
                    path = "/dev/" % right(path, -6);

                    if(path.length() == 8 && like(path, {"_/dev/sd*", "_/dev/hd*"}))
                    {
                        ullong size(devsize(path));

                        if(size > 536870911)
                        {
                            if(! fstab.isEmpty())
                            {
                                QSL fchk('_' % path % '*');

                                {
                                    PedDevice *dev(ped_device_get(path.toStdString().c_str()));
                                    PedDisk *dsk(ped_disk_new(dev));

                                    if(dsk != nullptr)
                                    {
                                        PedPartition *prt(nullptr);

                                        while((prt = ped_disk_next_partition(dsk, prt)))
                                            if(prt->num > 0)
                                            {
                                                QStr ppath(path % QStr::number(prt->num));

                                                if(stype(ppath) == Isblock)
                                                {
                                                    blkid_probe pr(blkid_new_probe_from_filename(ppath.toStdString().c_str()));
                                                    blkid_do_probe(pr);
                                                    cchar *uuid("");
                                                    blkid_probe_lookup_value(pr, "UUID", &uuid, nullptr);
                                                    blkid_free_probe(pr);
                                                    if(! QStr(uuid).isEmpty()) fchk.append("_UUID=" % QStr(uuid) % '*');
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
            }

        next_3:;
        }

        ThrdSlst->sort();
        break;
    }
    case Ruuid:
    {
        blkid_probe pr(blkid_new_probe_from_filename(ThrdStr[0].toStdString().c_str()));
        blkid_do_probe(pr);
        cchar *uuid("");
        blkid_probe_lookup_value(pr, "UUID", &uuid, nullptr);
        blkid_free_probe(pr);
        ThrdStr[1] = uuid;
        break;
    }
    case Setpflag:
        if(stype(ThrdStr[0]) == Isblock && stype(left(ThrdStr[0], 8)) == Isblock)
        {
            PedDevice *dev(ped_device_get(left(ThrdStr[0], 8).toStdString().c_str()));
            PedDisk *dsk(ped_disk_new(dev));
            PedPartition *prt(nullptr);
            if(ThrdRslt) ThrdRslt = false;

            while(! ThrdKill && (prt = ped_disk_next_partition(dsk, prt)))
                if(QStr(ped_partition_get_path(prt)) == ThrdStr[0])
                {
                    if(ped_partition_set_flag(prt, ped_partition_flag_get_by_name(ThrdStr[1].toStdString().c_str()), 1) == 1 && ped_disk_commit_to_dev(dsk) == 1) ThrdRslt = true;
                    ped_disk_commit_to_os(dsk);
                }

            ped_disk_destroy(dsk);
            ped_device_destroy(dev);
        }
        else
            ThrdRslt = false;

        break;
    case Mkptable:
        if(stype(ThrdStr[0]) == Isblock)
        {
            PedDevice *dev(ped_device_get(ThrdStr[0].toStdString().c_str()));
            PedDisk *dsk(ped_disk_new_fresh(dev, ped_disk_type_get(ThrdStr[1].toStdString().c_str())));
            ThrdRslt = ped_disk_commit_to_dev(dsk) == 1;
            ped_disk_commit_to_os(dsk);
            ped_disk_destroy(dsk);
            ped_device_destroy(dev);
        }
        else
            ThrdRslt = false;

        break;
    case Mkpart:
        if(stype(ThrdStr[0]) == Isblock)
        {
            PedDevice *dev(ped_device_get(ThrdStr[0].toStdString().c_str()));
            PedDisk *dsk(ped_disk_new(dev));
            PedPartition *prt(nullptr);
            if(ThrdRslt) ThrdRslt = false;

            if(ThrdLng[0] > 0 && ThrdLng[1] > 0)
            {
                PedPartition *crtprt(ped_partition_new(dsk, ThrdChr == Primary ? PED_PARTITION_NORMAL : ThrdChr == Extended ? PED_PARTITION_EXTENDED : PED_PARTITION_LOGICAL, ped_file_system_type_get("ext2"), psalign(ThrdLng[0] / dev->sector_size, dev->sector_size), ullong(dev->length - 1048576 / dev->sector_size) >= (ThrdLng[0] + ThrdLng[1]) / dev->sector_size - 1 ? pealign((ThrdLng[0] + ThrdLng[1]) / dev->sector_size - 1, dev->sector_size) : (ThrdLng[0] + ThrdLng[1]) / dev->sector_size - 1));
                if(ped_disk_add_partition(dsk, crtprt, ped_constraint_exact(&crtprt->geom)) == 1 && ped_disk_commit_to_dev(dsk) == 1) ThrdRslt = true;
                ped_disk_commit_to_os(dsk);
            }
            else
                while(! ThrdKill && (prt = ped_disk_next_partition(dsk, prt)))
                    if(prt->type == PED_PARTITION_FREESPACE && prt->geom.length >= 1048576 / dev->sector_size)
                    {
                        PedPartition *crtprt(ped_partition_new(dsk, PED_PARTITION_NORMAL, ped_file_system_type_get("ext2"), ThrdLng[0] == 0 ? psalign(prt->geom.start, dev->sector_size) : psalign(ThrdLng[0] / dev->sector_size, dev->sector_size), ThrdLng[1] == 0 ? prt->next && prt->next->type == PED_PARTITION_METADATA ? prt->next->geom.end : pealign(prt->geom.end, dev->sector_size) : pealign((ThrdLng[0] + ThrdLng[1]) / dev->sector_size - 1, dev->sector_size)));
                        if(ped_disk_add_partition(dsk, crtprt, ped_constraint_exact(&crtprt->geom)) == 1 && ped_disk_commit_to_dev(dsk) == 1) ThrdRslt = true;
                        ped_disk_commit_to_os(dsk);
                        break;
                    }

            ped_disk_destroy(dsk);
            ped_device_destroy(dev);
        }
        else
            ThrdRslt = false;

        break;
    case Delpart:
        if(stype(ThrdStr[0]) == Isblock)
        {
            PedDevice *dev(ped_device_get(left(ThrdStr[0], 8).toStdString().c_str()));
            PedDisk *dsk(ped_disk_new(dev));

            if(ped_disk_delete_partition(dsk, ped_disk_get_partition(dsk, right(ThrdStr[0], -8).toUShort())) == 1)
            {
                ped_disk_commit_to_dev(dsk);
                ped_disk_commit_to_os(dsk);
            }

            ped_disk_destroy(dsk);
            ped_device_destroy(dev);
        }

        break;
    case Crtrpoint:
        ThrdRslt = thrdcrtrpoint(ThrdStr[0], ThrdStr[1]);
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

bool sb::thrdcrtrpoint(cQStr &sdir, cQStr &pname)
{
    QStr sysitms[12], rootitms;

    if((isdir("/bin") && ! rodir(sysitms[0], "/bin")) ||
        (isdir("/boot") && ! rodir(sysitms[1], "/boot")) ||
        (isdir("/etc") && ! rodir(sysitms[2], "/etc")) ||
        (isdir("/lib") && ! rodir(sysitms[3], "/lib")) ||
        (isdir("/lib32") && ! rodir(sysitms[4], "/lib32")) ||
        (isdir("/lib64") && ! rodir(sysitms[5], "/lib64")) ||
        (isdir("/opt") && ! rodir(sysitms[6], "/opt")) ||
        (isdir("/sbin") && ! rodir(sysitms[7], "/sbin")) ||
        (isdir("/selinux") && ! rodir(sysitms[8], "/selinux")) ||
        (isdir("/srv") && ! rodir(sysitms[9], "/srv")) ||
        (isdir("/usr") && ! rodir(sysitms[10], "/usr")) ||
        (isdir("/var") && ! rodir(sysitms[11], "/var")) ||
        (isdir("/root") && ! rodir(rootitms, "/root", true))) return false;

    QSL homeitms, usrs;
    uint anum(0);

    {
        QFile file("/etc/passwd");
        if(! file.open(QIODevice::ReadOnly)) return false;

        while(! file.atEnd())
        {
            QStr usr(file.readLine().trimmed());

            if(usr.contains(":/home/"))
            {
                usr = left(usr, instr(usr, ":") -1);

                if(isdir("/home/" % usr))
                {
                    usrs.append(usr);
                    homeitms.append(nullptr);
                    if(! rodir(homeitms.last(), "/home/" % usr, true)) return false;
                }
            }
        }
    }

    for(uchar a(0) ; a < homeitms.count() ; ++a) anum += homeitms.at(a).count('\n');
    anum += rootitms.count('\n');
    for(uchar a(0) ; a < 12 ; ++a) anum += sysitms[a].count('\n');
    Progress = 0;
    QStr trgt(sdir % '/' % pname);
    if(! QDir().mkdir(trgt)) return false;
    QSL elist({".sbuserdata", ".cache/gvfs", ".local/share/Trash/files/", ".local/share/Trash/info/", ".Xauthority", ".ICEauthority"});

    {
        QFile file("/etc/systemback.excludes");
        if(! file.open(QIODevice::ReadOnly)) return false;

        while(! file.atEnd())
        {
            QStr cline(file.readLine().trimmed());
            if(cline.startsWith('.')) elist.append(cline);
            if(ThrdKill) return false;
        }
    }

    QStr *cditms;
    uint cnum(0);
    uchar cperc;
    QSL rplst;

    for(cQStr &item : QDir(sdir).entryList(QDir::Dirs | QDir::NoDotAndDotDot))
    {
        if(like(item, {"_S01_*", "_S02_*", "_S03_*", "_S04_*", "_S05_*", "_S06_*", "_S07_*", "_S08_*", "_S09_*", "_S10_*", "_H01_*", "_H02_*", "_H03_*", "_H04_*", "_H05_*"})) rplst.append(item);
        if(ThrdKill) return false;
    }

    if(isdir("/home"))
    {
        if(! QDir().mkdir(trgt % "/home")) return false;

        for(uchar a(0) ; a < usrs.count() ; ++a)
        {
            cQStr &usr(usrs.at(a));
            if(! QDir().mkdir(trgt % "/home/" % usr)) return false;
            cditms = &homeitms[a];
            QTS in(cditms, QIODevice::ReadOnly);

            while(! in.atEnd())
            {
                QStr line(in.readLine()), item(right(line, -1));
                ++cnum;
                cperc = (cnum * 100 + 50) / anum;
                if(Progress < cperc) Progress = cperc;

                if(! like(item, {"_lost+found_", "_lost+found/*", "*/lost+found_", "*/lost+found/*", "_Systemback_", "_Systemback/*", "*/Systemback_", "*/Systemback/*", "*~_", "*~/*"}) && ! exclcheck(elist, item) && exist("/home/" % usr % '/' % item))
                {
                    switch(line.left(1).toUShort()) {
                    case Islink:
                        for(cQStr &cpname : rplst)
                        {
                            if(stype(sdir % '/' % cpname % "/home/" % usr % '/' % item) == Islink && lcomp("/home/" % usr % '/' % item, sdir % '/' % cpname % "/home/" % usr % '/' % item))
                            {
                                if(link(QStr(sdir % '/' % cpname % "/home/" % usr % '/' % item).toStdString().c_str(), QStr(trgt % "/home/" % usr % '/' % item).toStdString().c_str()) == -1) goto err_1;
                                goto nitem_1;
                            }

                            if(ThrdKill) return false;
                        }

                        if(! cplink("/home/" % usr % '/' % item, trgt % "/home/" % usr % '/' % item)) goto err_1;
                        break;
                    case Isdir:
                        if(! QDir().mkdir(trgt % "/home/" % usr % '/' % item)) return false;
                        break;
                    case Isfile:
                        if(QFile("/home/" % usr % '/' % item).size() <= 8000000)
                        {
                            for(cQStr &cpname : rplst)
                            {
                                if(stype(sdir % '/' % cpname % "/home/" % usr % '/' % item) == Isfile && fcomp("/home/" % usr % '/' % item, sdir % '/' % cpname % "/home/" % usr % '/' % item) == 2)
                                {
                                    if(link(QStr(sdir % '/' % cpname % "/home/" % usr % '/' % item).toStdString().c_str(), QStr(trgt % "/home/" % usr % '/' % item).toStdString().c_str()) == -1) goto err_1;
                                    goto nitem_1;
                                }

                                if(ThrdKill) return false;
                            }

                            if(! cpfile("/home/" % usr % '/' % item, trgt % "/home/" % usr % '/' % item)) goto err_1;
                        }
                    }
                }

            nitem_1:
                if(ThrdKill) return false;
                continue;
            err_1:
                ThrdDbg = "@/home/" % usr % '/' % item;
                return false;
            }

            in.setString(cditms, QIODevice::ReadOnly);

            while(! in.atEnd())
            {
                QStr line(in.readLine()), item(right(line, -1));
                if(line.left(1).toUShort() == Isdir && exist(trgt % "/home/" % usr % '/' % item) && ! cpertime("/home/" % usr % '/' % item, trgt % "/home/" % usr % '/' % item)) goto err_2;
                if(ThrdKill) return false;
                continue;
            err_2:
                ThrdDbg = "@/home/" % usr % '/' % item;
                return false;
            }

            if(! cpertime("/home/" % usr, trgt % "/home/" % usr))
            {
                ThrdDbg = "@/home/" % usr, trgt % "/home/" % usr;
                return false;
            }

            cditms->clear();
        }

        if(! cpertime("/home", trgt % "/home"))
        {
            ThrdDbg = "@/home";
            return false;
        }
    }

    if(isdir("/root"))
    {
        if(! QDir().mkdir(trgt % "/root")) return false;
        QTS in(&rootitms, QIODevice::ReadOnly);

        while(! in.atEnd())
        {
            QStr line(in.readLine()), item(right(line, -1));
            ++cnum;
            cperc = (cnum * 100 + 50) / anum;
            if(Progress < cperc) Progress = cperc;

            if(! like(item, {"_lost+found_", "_lost+found/*", "*/lost+found_", "*/lost+found/*", "_Systemback_", "_Systemback/*", "*/Systemback_", "*/Systemback/*", "*~_", "*~/*"}) && ! exclcheck(elist, item) && exist("/root/" % item))
            {
                switch(line.left(1).toUShort()) {
                case Islink:
                    for(cQStr &cpname : rplst)
                    {
                        if(stype(sdir % '/' % cpname % "/root/" % item) == Islink && lcomp("/root/" % item, sdir % '/' % cpname % "/root/" % item))
                        {
                            if(link(QStr(sdir % '/' % cpname % "/root/" % item).toStdString().c_str(), QStr(trgt % "/root/" % item).toStdString().c_str()) == -1) goto err_3;
                            goto nitem_2;
                        }

                        if(ThrdKill) return false;
                    }

                    if(! cplink("/root/" % item, trgt % "/root/" % item)) goto err_3;
                    break;
                case Isdir:
                    if(! QDir().mkdir(trgt % "/root/" % item)) return false;
                    break;
                case Isfile:
                    if(QFile("/root/" % item).size() <= 8000000)
                    {
                        for(cQStr &cpname : rplst)
                        {
                            if(stype(sdir % '/' % cpname % "/root/" % item) == Isfile && fcomp("/root/" % item, sdir % '/' % cpname % "/root/" % item) == 2)
                            {
                                if(link(QStr(sdir % '/' % cpname % "/root/" % item).toStdString().c_str(), QStr(trgt % "/root/" % item).toStdString().c_str()) == -1) goto err_3;
                                goto nitem_2;
                            }

                            if(ThrdKill) return false;
                        }

                        if(! cpfile("/root/" % item, trgt % "/root/" % item)) goto err_3;
                    }
                }
            }

        nitem_2:
            if(ThrdKill) return false;
            continue;
        err_3:
            ThrdDbg = "@/root/" % item;
            return false;
        }

        in.setString(&rootitms, QIODevice::ReadOnly);

        while(! in.atEnd())
        {
            QStr line(in.readLine()), item(right(line, -1));
            if(line.left(1).toUShort() == Isdir && exist(trgt % "/root/" % item) && ! cpertime("/root/" % item, trgt % "/root/" % item)) goto err_4;
            if(ThrdKill) return false;
            continue;
        err_4:
            ThrdDbg = "@/root/" % item;
            return false;
        }

        rootitms.clear();

        if(! cpertime("/root", trgt % "/root"))
        {
            ThrdDbg = "@/root";
            return false;
        }
    }

    for(cQStr &item : QDir("/").entryList(QDir::Files))
    {
        if(like(item, {"_initrd.img*", "_vmlinuz*"}) && islink('/' % item))
        {
            for(cQStr &cpname : rplst)
            {
                if(stype(sdir % '/' % cpname % '/' % item) == Islink && lcomp('/' % item, sdir % '/' % cpname % '/' % item))
                {
                    if(link(QStr(sdir % '/' % cpname % '/' % item).toStdString().c_str(), QStr(trgt % '/' % item).toStdString().c_str()) == -1) goto err_5;
                    goto nitem_3;
                }

                if(ThrdKill) return false;
            }

            if(! cplink('/' % item, trgt % '/' % item)) return false;
        }

    nitem_3:
        if(ThrdKill) return false;
        continue;
    err_5:
        ThrdDbg = "@/" % item;
        return false;
    }

    for(cQStr &cdir : {"/cdrom", "/dev", "/mnt", "/proc", "/run", "/sys", "/tmp"})
    {
        if(isdir(cdir) && ! cpdir(cdir, trgt % cdir)) return false;
        if(ThrdKill) return false;
    }

    elist = {"/etc/mtab", "/var/.sblvtmp", "/var/cache/fontconfig/", "/var/lib/dpkg/lock", "/var/lib/udisks/mtab", "/var/lib/ureadahead/", "/var/log/", "/var/run/", "/var/tmp/"};
    QSL dlst = {"/bin", "/boot", "/etc", "/lib", "/lib32", "/lib64", "/opt", "/sbin", "/selinux", "/srv", "/usr", "/var"};

    for(uchar a(0) ; a < dlst.count() ; ++a)
    {
        cQStr &cdir(dlst.at(a));

        if(isdir(cdir))
        {
            if(! QDir().mkdir(trgt % cdir)) return false;
            cditms = &sysitms[a];
            QTS in(cditms, QIODevice::ReadOnly);

            while(! in.atEnd())
            {
                QStr line(in.readLine()), item(right(line, -1));
                ++cnum;
                cperc = (cnum * 100 + 50) / anum;
                if(Progress < cperc) Progress = cperc;

                if(! like(cdir % '/' % item, {"+_/var/cache/apt/*", "-*.bin_", "-*.bin.*"}, Mixed) && ! like(cdir % '/' % item, {"_/var/cache/apt/archives/*", "*.deb_"}, All) && ! like(item, {"_lost+found_", "_lost+found/*", "*/lost+found_", "*/lost+found/*", "_Systemback_", "_Systemback/*", "*/Systemback_", "*/Systemback/*", "*.dpkg-old_", "*~_", "*~/*"}) && ! exclcheck(elist, QStr(cdir % '/' % item)) && exist(cdir % '/' % item))
                {
                    switch(line.left(1).toUShort()) {
                    case Islink:
                        for(cQStr &cpname : rplst)
                        {
                            if(stype(sdir % '/' % cpname % cdir % '/' % item) == Islink && lcomp(cdir % '/' % item, sdir % '/' % cpname % cdir % '/' % item))
                            {
                                if(link(QStr(sdir % '/' % cpname % cdir % '/' % item).toStdString().c_str(), QStr(trgt % cdir % '/' % item).toStdString().c_str()) == -1) goto err_6;
                                goto nitem_4;
                            }

                            if(ThrdKill) return false;
                        }

                        if(! cplink(cdir % '/' % item, trgt % cdir % '/' % item)) goto err_6;
                        break;
                    case Isdir:
                        if(! QDir().mkdir(trgt % cdir % '/' % item)) return false;
                        break;
                    case Isfile:
                        for(cQStr &cpname : rplst)
                        {
                            if(stype(sdir % '/' % cpname % cdir % '/' % item) == Isfile && fcomp(cdir % '/' % item, sdir % '/' % cpname % cdir % '/' % item) == 2)
                            {
                                if(link(QStr(sdir % '/' % cpname % cdir % '/' % item).toStdString().c_str(), QStr(trgt % cdir % '/' % item).toStdString().c_str()) == -1) goto err_6;
                                goto nitem_4;
                            }

                            if(ThrdKill) return false;
                        }

                        if(! cpfile(cdir % '/' % item, trgt % cdir % '/' % item)) goto err_6;
                    }
                }

            nitem_4:
                if(ThrdKill) return false;
                continue;
            err_6:
                ThrdDbg = '@' % cdir % '/' % item;
                return false;
            }

            in.setString(cditms, QIODevice::ReadOnly);

            while(! in.atEnd())
            {
                QStr line(in.readLine()), item(right(line, -1));
                if(line.left(1).toUShort() == Isdir && exist(trgt % cdir % '/' % item) && ! cpertime(cdir % '/' % item, trgt % cdir % '/' % item)) goto err_7;
                if(ThrdKill) return false;
                continue;
            err_7:
                ThrdDbg = '@' % cdir % '/' % item;
                return false;
            }

            if(! cpertime(cdir, trgt % cdir))
            {
                ThrdDbg = '@' % cdir;
                return false;
            }

            cditms->clear();
        }
    }

    if(isdir("/media"))
    {
        if(! QDir().mkdir(trgt % "/media")) return false;

        if(isfile("/etc/fstab"))
        {
            dlst = QDir("/media").entryList(QDir::Dirs | QDir::NoDotAndDotDot);
            QFile file("/etc/fstab");
            if(! file.open(QIODevice::ReadOnly)) return false;

            for(uchar a(0) ; a < dlst.count() ; ++a)
            {
                cQStr &item(dlst.at(a));
                if(a > 0 && ! file.open(QIODevice::ReadOnly)) return false;

                while(! file.atEnd())
                {
                    QStr cline(file.readLine().trimmed().replace('\t', ' ')), fdir;

                    if(! cline.startsWith('#') && like(cline.replace("\\040", " "), {"* /media/" % item % " *", "* /media/" % item % "/*"}))
                    {
                        for(QStr cdname : QStr(mid(cline, instr(cline, "/media/") + 7, instr(cline, " ", instr(cline, "/media/")) - instr(cline, "/media/") - 7)).split('/'))
                            if(! cdname.isEmpty())
                            {
                                fdir.append('/' % cdname.replace("\\040", " "));
                                if(! isdir(trgt % "/media" % fdir) && ! cpdir("/media" % fdir, trgt % "/media" % fdir)) goto err_8;
                            }
                    }

                    if(ThrdKill) return false;
                    continue;
                err_8:
                    ThrdDbg = "@/media" % fdir;
                    return false;
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
        QStr logitms;
        if(! rodir(logitms, "/var/log")) return false;
        QTS in(&logitms, QIODevice::ReadOnly);

        while(! in.atEnd())
        {
            QStr line(in.readLine()), item(right(line, -1));

            switch(line.left(1).toUShort()) {
            case Isdir:
                if(! cpdir("/var/log/" % item, trgt % "/var/log/" % item)) goto err_9;
                break;
            case Isfile:
                if(! like(item, {"*.gz_", "*.old_"}) && (! item.contains('.') || ! isnum(right(item, - rinstr(item, ".")))))
                {
                    crtfile(trgt % "/var/log/" % item);
                    if(! cpertime("/var/log/" % item, trgt % "/var/log/" % item)) goto err_9;
                }
            }

            if(ThrdKill) return false;
            continue;
        err_9:
            ThrdDbg = "@/var/log/" % item;
            return false;
        }

        in.setString(&logitms, QIODevice::ReadOnly);

        while(! in.atEnd())
        {
            QStr line(in.readLine()), item(right(line, -1));
            if(line.left(1).toUShort() == Isdir && exist(trgt % "/var/log/" % item) && ! cpertime("/var/log/" % item, trgt % "/var/log/" % item)) goto err_10;
            if(ThrdKill) return false;
            continue;
        err_10:
            ThrdDbg = "@/var/log/" % item;
            return false;
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
    QSL homeitms, usrs;
    QStr sysitms[12], rootitms;
    uint anum(0);

    if(mthd != 2)
    {
        if(! ilike(mthd, {4, 6}))
        {
            if(isdir(srcdir % "/root") && ! rodir(rootitms, srcdir % "/root", true)) return false;

            if(isdir(srcdir % "/home"))
                for(cQStr &usr : QDir(srcdir % "/home").entryList(QDir::Dirs | QDir::NoDotAndDotDot))
                {
                    usrs.append(usr);
                    homeitms.append(nullptr);
                    if(! rodir(homeitms.last(), srcdir % "/home/" % usr, true)) return false;
                }
        }
        else if(usr == "root")
        {
            if(isdir(srcdir % "/root") && ! rodir(rootitms, srcdir % "/root", true)) return false;
        }
        else if(isdir(srcdir % "/home/" % usr))
        {
            usrs.append(usr);
            homeitms.append(nullptr);
            if(! rodir(homeitms.last(), srcdir % "/home/" % usr, true)) return false;
        }

        for(uchar a(0) ; a < homeitms.count() ; ++a) anum += homeitms.at(a).count('\n');
        anum += rootitms.count('\n');
    }

    QStr *cditms;
    uint cnum(0);
    uchar cperc;

    if(mthd < 3)
    {
        if((isdir(srcdir % "/bin") && ! rodir(sysitms[0], srcdir % "/bin")) ||
            (isdir(srcdir % "/boot") && ! rodir(sysitms[1], srcdir % "/boot")) ||
            (isdir(srcdir % "/etc") && ! rodir(sysitms[2], srcdir % "/etc")) ||
            (isdir(srcdir % "/lib") && ! rodir(sysitms[3], srcdir % "/lib")) ||
            (isdir(srcdir % "/lib32") && ! rodir(sysitms[4], srcdir % "/lib32")) ||
            (isdir(srcdir % "/lib64") && ! rodir(sysitms[5], srcdir % "/lib64")) ||
            (isdir(srcdir % "/opt") && ! rodir(sysitms[6], srcdir % "/opt")) ||
            (isdir(srcdir % "/sbin") && ! rodir(sysitms[7], srcdir % "/sbin")) ||
            (isdir(srcdir % "/selinux") && ! rodir(sysitms[8], srcdir % "/selinux")) ||
            (isdir(srcdir % "/srv") && ! rodir(sysitms[9], srcdir % "/srv")) ||
            (isdir(srcdir % "/usr") && ! rodir(sysitms[10], srcdir % "/usr")) ||
            (isdir(srcdir % "/var") && ! rodir(sysitms[11], srcdir % "/var"))) return false;

        for(uchar a(0) ; a < 12 ; ++a) anum += sysitms[a].count('\n');
        Progress = 0;

        for(cQStr &item : QDir(trgt.isEmpty() ? "/" : trgt).entryList(QDir::Files))
        {
            if(like(item, {"_initrd.img*", "_vmlinuz*"}) && ! islink(trgt % '/' % item) && ! exist(srcdir % '/' % item) && ! lcomp(srcdir % '/' % item, trgt % '/' % item)) QFile::remove(trgt % '/' % item);
            if(ThrdKill) return false;
        }

        for(cQStr &item : QDir(srcdir).entryList(QDir::Files))
        {
            if(like(item, {"_initrd.img*", "_vmlinuz*"}) && ! exist(trgt % '/' % item)) cplink(srcdir % '/' % item, trgt % '/' % item);
            if(ThrdKill) return false;
        }

        for(cQStr &cdir : {"/cdrom", "/dev", "/home", "/mnt", "/root", "/proc", "/run", "/sys", "/tmp"})
        {
            if(isdir(srcdir % cdir))
            {
                if(isdir(trgt % cdir))
                    cpertime(srcdir % cdir, trgt % cdir);
                else
                {
                    if(exist(trgt % cdir)) QFile::remove(trgt % cdir);
                    if(! cpdir(srcdir % cdir, trgt % cdir) && ! fspchk(trgt)) return false;
                }
            }
            else if(exist(trgt % cdir))
                stype(trgt % cdir) == Isdir ? recrmdir(trgt % cdir) : QFile::remove(trgt % cdir);

            if(ThrdKill) return false;
        }

        QSL elist;
        if(trgt.isEmpty()) elist = {"/etc/mtab", "/var/cache/fontconfig/", "/var/lib/dpkg/lock", "/var/lib/udisks/mtab", "/var/run/", "/var/tmp/"};
        if(trgt.isEmpty() || (isfile("/mnt/etc/xdg/autostart/sbschedule.desktop") && isfile("/mnt/etc/xdg/autostart/sbschedule-kde.desktop") && isfile("/mnt/usr/bin/systemback") && isfile("/mnt/usr/lib/systemback/libsystemback.so.1.0.0") && isfile("/mnt/usr/lib/systemback/sbscheduler") && isfile("/mnt/usr/lib/systemback/sbsustart") && isfile("/mnt/usr/lib/systemback/sbsysupgrade")&& isdir("/mnt/usr/share/systemback/lang") && isfile("/mnt/usr/share/systemback/efi.tar.gz") && isfile("/mnt/usr/share/systemback/splash.png") && isfile("/mnt/var/lib/dpkg/info/systemback.list") && isfile("/mnt/var/lib/dpkg/info/systemback.md5sums"))) elist.append({"/etc/systemback*", "/etc/xdg/autostart/sbschedule*", "systemback*", "/usr/lib/systemback/", "/usr/share/systemback/", "/var/lib/dpkg/info/systemback*"});
        if(sfstab) elist.append("/etc/fstab");
        QSL dlst = {"/bin", "/boot", "/etc", "/lib", "/lib32", "/lib64", "/opt", "/sbin", "/selinux", "/srv", "/usr", "/var"};

        for(uchar a(0) ; a < dlst.count() ; ++a)
        {
            cQStr &cdir(dlst.at(a));

            if(isdir(srcdir % cdir))
            {
                if(isdir(trgt % cdir))
                {
                    QSL sdlst;
                    if(! odir(sdlst, trgt % cdir)) return false;

                    for(cQStr &item : sdlst)
                    {
                        if(! like(item, {"_lost+found_", "_Systemback_"}) && ! exclcheck(elist, QStr(cdir % '/' % item)) && ! exist(srcdir % cdir % '/' % item)) stype(trgt % cdir % '/' % item) == Isdir ? recrmdir(trgt % cdir % '/' % item) : QFile::remove(trgt % cdir % '/' % item);
                        if(ThrdKill) return false;
                    }
                }
                else
                {
                    if(exist(trgt % cdir)) QFile::remove(trgt % cdir);
                    if(! QDir().mkdir(trgt % cdir) && ! fspchk(trgt)) return false;
                }

                cditms = &sysitms[a];
                QTS in(cditms, QIODevice::ReadOnly);

                while(! in.atEnd())
                {
                    QStr line(in.readLine()), item(right(line, -1));
                    ++cnum;
                    cperc = (cnum * 100 + 50) / anum;
                    if(Progress < cperc) Progress = cperc;

                    if(! like(item, {"_lost+found_", "_lost+found/*", "*/lost+found_", "*/lost+found/*", "_Systemback_", "_Systemback/*", "*/Systemback_", "*/Systemback/*"}) && ! exclcheck(elist, QStr(cdir % '/' % item)))
                    {
                        switch(line.left(1).toUShort()) {
                        case Islink:
                            switch(stype(trgt % cdir % '/' % item)) {
                            case Islink:
                                if(lcomp(srcdir % cdir % '/' % item, trgt % cdir % '/' % item)) goto nitem_1;
                            case Isfile:
                                QFile::remove(trgt % cdir % '/' % item);
                                break;
                            case Isdir:
                                recrmdir(trgt % cdir % '/' % item);
                            }

                            if(! cplink(srcdir % cdir % '/' % item, trgt % cdir % '/' % item) && ! fspchk(trgt)) return false;
                            break;
                        case Isdir:
                            switch(stype(trgt % cdir % '/' % item)) {
                            case Isdir:
                            {
                                QSL sdlst;
                                if(! odir(sdlst, trgt % cdir % '/' % item)) return false;

                                for(cQStr &sitem : sdlst)
                                {
                                    if(! like(sitem, {"_lost+found_", "_Systemback_"}) && ! exclcheck(elist, QStr(cdir % '/' % item % '/' % sitem)) && ! exist(srcdir % cdir % '/' % item % '/' % sitem)) stype(trgt % cdir % '/' % item % '/' % sitem) == Isdir ? recrmdir(trgt % cdir % '/' % item % '/' % sitem) : QFile::remove(trgt % cdir % '/' % item % '/' % sitem);
                                    if(ThrdKill) return false;
                                }

                                goto nitem_1;
                            }
                            case Islink:
                            case Isfile:
                                QFile::remove(trgt % cdir % '/' % item);
                            }

                            if(! QDir().mkdir(trgt % cdir % '/' % item) && ! fspchk(trgt)) return false;
                            break;
                        case Isfile:
                            switch(stype(trgt % cdir % '/' % item)) {
                            case Isfile:
                                switch(fcomp(srcdir % cdir % '/' % item, trgt % cdir % '/' % item)) {
                                case 1:
                                    cpertime(srcdir % cdir % '/' % item, trgt % cdir % '/' % item);
                                case 2:
                                    goto nitem_1;
                                }
                            case Islink:
                                QFile::remove(trgt % cdir % '/' % item);
                                break;
                            case Isdir:
                                recrmdir(trgt % cdir % '/' % item);
                            }

                            if(! cpfile(srcdir % cdir % '/' % item, trgt % cdir % '/' % item) && ! fspchk(trgt)) return false;
                        }
                    }

                nitem_1:
                    if(ThrdKill) return false;
                }

                in.setString(cditms, QIODevice::ReadOnly);

                while(! in.atEnd())
                {
                    QStr line(in.readLine()), item(right(line, -1));
                    if(line.left(1).toUShort() == Isdir && exist(trgt % cdir % '/' % item)) cpertime(srcdir % cdir % '/' % item, trgt % cdir % '/' % item);
                    if(ThrdKill) return false;
                }

                cditms->clear();
            }
            else if(exist(trgt % cdir))
                stype(trgt % cdir) == Isdir ? recrmdir(trgt % cdir) : QFile::remove(trgt % cdir);

            cpertime(srcdir % cdir, trgt % cdir);
        }

        if(isdir(srcdir % "/media"))
        {
            if(isdir(trgt % "/media"))
                for(cQStr &item : QDir(trgt % "/media").entryList(QDir::Dirs | QDir::NoDotAndDotDot))
                {
                    if(! exist(srcdir % "/media/" % item) && ! mcheck(trgt % "/media/" % item % '/')) recrmdir(trgt % "/media/" % item);
                    if(ThrdKill) return false;
                }
            else
            {
                if(exist(trgt % "/media")) QFile::remove(trgt % "/media");
                if(! QDir().mkdir(trgt % "/media") && ! fspchk(trgt)) return false;
            }

            QStr mediaitms;
            if(! rodir(mediaitms, srcdir % "/media")) return false;
            QTS in(&mediaitms, QIODevice::ReadOnly);

            while(! in.atEnd())
            {
                QStr line(in.readLine()), item(right(line, -1));

                if(exist(trgt % "/media/" % item))
                {
                    if(stype(trgt % "/media/" % item) != Isdir)
                    {
                        QFile::remove(trgt % "/media/" % item);
                        if(! QDir().mkdir(trgt % "/media/" % item) && ! fspchk(trgt)) return false;
                    }

                    cpertime(srcdir % "/media/" % item, trgt % "/media/" % item);
                }
                else if(! cpdir(srcdir % "/media/" % item, trgt % "/media/" % item) && ! fspchk(trgt))
                    return false;

                if(ThrdKill) return false;
            }

            cpertime(srcdir % "/media", trgt % "/media");
        }
        else if(exist(trgt % "/media"))
            stype(trgt % "/media") == Isdir ? recrmdir(trgt % "/media") : QFile::remove(trgt % "/media");

        if(srcdir == "/.systembacklivepoint" && isdir("/.systembacklivepoint/.systemback"))
        {
            QStr sbitms;
            if(! rodir(sbitms, srcdir % "/.systemback")) return false;
            QTS in(&sbitms, QIODevice::ReadOnly);

            while(! in.atEnd())
            {
                QStr line(in.readLine()), item(right(line, -1));

                switch(line.left(1).toUShort()) {
                case Islink:
                    if((item != "etc/fstab" || ! sfstab) && ! cplink(srcdir % "/.systemback/" % item, trgt % '/' % item) && ! fspchk(trgt)) return false;
                    break;
                case Isfile:
                    if((item != "etc/fstab" || ! sfstab) && ! cpfile(srcdir % "/.systemback/" % item, trgt % '/' % item) && ! fspchk(trgt)) return false;
                }

                if(ThrdKill) return false;
            }

            in.setString(&sbitms, QIODevice::ReadOnly);

            while(! in.atEnd())
            {
                QStr line(in.readLine()), item(right(line, -1));
                if(line.left(1).toUShort() == Isdir) cpertime(srcdir % "/.systemback/" % item, trgt % '/' % item);
                if(ThrdKill) return false;
            }
        }
    }
    else
        Progress = 0;

    if(mthd != 2)
    {
        QSL elist({".cache/gvfs", ".gvfs", ".local/share/Trash/files/", ".local/share/Trash/info/", ".Xauthority", ".ICEauthority"});

        {
            QFile file(srcdir % "/etc/systemback.excludes");
            if(! file.open(QIODevice::ReadOnly)) return false;

            while(! file.atEnd())
            {
                QStr cline(file.readLine().trimmed());
                if(cline.startsWith('.')) elist.append(cline);
                if(ThrdKill) return false;
            }
        }

        bool skppd;

        if(! ilike(mthd, {4, 6}) || usr == "root")
        {
            if(! ilike(mthd, {3, 4}))
            {
                QSL sdlst;
                if(! odir(sdlst, trgt % "/root", true)) return false;

                for(cQStr &item : sdlst)
                {
                    if(! item.endsWith('~') && ! exclcheck(elist, item) && ! exist(srcdir % "/root/" % item))
                    {
                        switch(stype(trgt % "/root/" % item)) {
                        case Isdir:
                            recrmdir(trgt % "/root/" % item, true);
                            break;
                        case Isfile:
                            if(QFile(trgt % "/root/" % item).size() > 8000000) break;
                        case Islink:
                            QFile::remove(trgt % "/root/" % item);
                        }
                    }

                    if(ThrdKill) return false;
                }
            }

            QTS in(&rootitms, QIODevice::ReadOnly);

            while(! in.atEnd())
            {
                QStr line(in.readLine()), item(right(line, -1));
                ++cnum;
                cperc = (cnum * 100 + 50) / anum;
                if(Progress < cperc) Progress = cperc;

                if(! like(item, {"_lost+found_", "_lost+found/*", "*/lost+found_", "*/lost+found/*", "_Systemback_", "_Systemback/*", "*/Systemback_", "*/Systemback/*", "*~_", "*~/*"}) && ! exclcheck(elist, item))
                {
                    switch(line.left(1).toUShort()) {
                    case Islink:
                        switch(stype(trgt % "/root/" % item)) {
                        case Islink:
                            if(lcomp(srcdir % "/root/" % item, trgt % "/root/" % item)) goto nitem_2;
                        case Isfile:
                            QFile::remove(trgt % "/root/" % item);
                            break;
                        case Isdir:
                            recrmdir(trgt % "/root/" % item);
                        }

                        if(! cplink(srcdir % "/root/" % item, trgt % "/root/" % item) && ! fspchk(trgt)) return false;
                        break;
                    case Isdir:
                        switch(stype(trgt % "/root/" % item)) {
                        case Isdir:
                        {
                            if(! ilike(mthd, {3, 4}))
                            {
                                QSL sdlst;
                                if(! odir(sdlst, trgt % "/root/" % item)) return false;

                                for(cQStr &sitem : sdlst)
                                {
                                    if(! like(sitem, {"_lost+found_", "_Systemback_", "*~_"}) && ! exclcheck(elist, QStr(item % '/' % sitem)) && ! exist(srcdir % "/root/" % item % '/' % sitem))
                                    {
                                        switch(stype(trgt % "/root/" % item % '/' % sitem)) {
                                        case Isdir:
                                            recrmdir(trgt % "/root/" % item % '/' % sitem, true);
                                            break;
                                        case Isfile:
                                            if(QFile(trgt % "/root/" % item % '/' % sitem).size() > 8000000) break;
                                        case Islink:
                                            QFile::remove(trgt % "/root/" % item % '/' % sitem);
                                        }
                                    }

                                    if(ThrdKill) return false;
                                }
                            }

                            goto nitem_2;
                        }
                        case Islink:
                        case Isfile:
                            QFile::remove(trgt % "/root/" % item);
                        }

                        if(! QDir().mkdir(trgt % "/root/" % item) && ! fspchk(trgt)) return false;
                        break;
                    case Isfile:
                        skppd = QFile(srcdir % "/root/" % item).size() > 8000000;

                        switch(stype(trgt % "/root/" % item)) {
                        case Isfile:
                            switch(fcomp(trgt % "/root/" % item, srcdir % "/root/" % item)) {
                            case 1:
                                cpertime(srcdir % "/root/" % item, trgt % "/root/" % item);
                            case 2:
                                goto nitem_2;
                            }

                            if(skppd) goto nitem_2;
                        case Islink:
                            QFile::remove(trgt % "/root/" % item);
                            break;
                        case Isdir:
                            recrmdir(trgt % "/root/" % item);
                        }

                        if(! skppd && ! cpfile(srcdir % "/root/" % item, trgt % "/root/" % item) && ! fspchk(trgt)) return false;
                    }
                }

            nitem_2:
                if(ThrdKill) return false;
            }

            in.setString(&rootitms, QIODevice::ReadOnly);

            while(! in.atEnd())
            {
                QStr line(in.readLine()), item(right(line, -1));
                if(line.left(1).toUShort() == Isdir && exist(trgt % "/root/" % item)) cpertime(srcdir % "/root/" % item, trgt % "/root/" % item);
                if(ThrdKill) return false;
            }

            rootitms.clear();
        }

        for(uchar a(0) ; a < usrs.count() ; ++a)
        {
            cQStr &usr(usrs.at(a));

            if(! isdir(trgt % "/home/" % usr))
            {
                if(exist(trgt % "/home/" % usr)) QFile::remove(trgt % "/home/" % usr);
                if(! QDir().mkdir(trgt % "/home/" % usr) && ! fspchk(trgt)) return false;
            }
            else if(! ilike(mthd, {3, 4}))
            {
                QSL sdlst;
                if(! odir(sdlst, trgt % "/home/" % usr, true)) return false;

                for(cQStr &item : sdlst)
                {
                    if(! item.endsWith('~') && ! exclcheck(elist, item) && ! exist(srcdir % "/home/" % usr % '/' % item))
                    {
                        switch(stype(trgt % "/home/" % usr % '/' % item)) {
                        case Isdir:
                            recrmdir(trgt % "/home/" % usr % '/' % item, true);
                            break;
                        case Isfile:
                            if(QFile(trgt % "/home/" % usr % '/' % item).size() > 8000000) break;
                        case Islink:
                            QFile::remove(trgt % "/home/" % usr % '/' % item);
                        }
                    }

                    if(ThrdKill) return false;
                }
            }

            cditms = &homeitms[a];
            QTS in(cditms, QIODevice::ReadOnly);

            while(! in.atEnd())
            {
                QStr line(in.readLine()), item(right(line, -1));
                ++cnum;
                cperc = (cnum * 100 + 50) / anum;
                if(Progress < cperc) Progress = cperc;

                if(! like(item, {"_lost+found_", "_lost+found/*", "*/lost+found_", "*/lost+found/*", "_Systemback_", "_Systemback/*", "*/Systemback_", "*/Systemback/*", "*~_", "*~/*"}) && ! exclcheck(elist, item))
                {
                    switch(line.left(1).toUShort()) {
                    case Islink:
                        switch(stype(trgt % "/home/" % usr % '/' % item)) {
                        case Islink:
                            if(lcomp(srcdir % "/home/" % usr % '/' % item, trgt % "/home/" % usr % '/' % item)) goto nitem_3;
                        case Isfile:
                            QFile::remove(trgt % "/home/" % usr % '/' % item);
                            break;
                        case Isdir:
                            recrmdir(trgt % "/home/" % usr % '/' % item);
                        }

                        if(! cplink(srcdir % "/home/" % usr % '/' % item, trgt % "/home/" % usr % '/' % item) && ! fspchk(trgt)) return false;
                        break;
                    case Isdir:
                        switch(stype(trgt % "/home/" % usr % '/' % item)) {
                        case Isdir:
                        {
                            if(! ilike(mthd, {3, 4}))
                            {
                                QSL sdlst;
                                if(! odir(sdlst, trgt % "/home/" % usr % '/' % item)) return false;

                                for(cQStr &sitem : sdlst)
                                {
                                    if(! like(sitem, {"_lost+found_", "_Systemback_", "*~_"}) && ! exclcheck(elist, QStr(item % '/' % sitem)) && ! exist(srcdir % "/home/" % usr % '/' % item % '/' % sitem))
                                    {
                                        switch(stype(trgt % "/home/" % usr % '/' % item % '/' % sitem)) {
                                        case Isdir:
                                            recrmdir(trgt % "/home/" % usr % '/' % item % '/' % sitem, true);
                                            break;
                                        case Isfile:
                                            if(QFile(trgt % "/home/" % usr % '/' % item % '/' % sitem).size() > 8000000) break;
                                        case Islink:
                                            QFile::remove(trgt % "/home/" % usr % '/' % item % '/' % sitem);
                                        }
                                    }

                                    if(ThrdKill) return false;
                                }
                            }

                            goto nitem_3;
                        }
                        case Islink:
                        case Isfile:
                            QFile::remove(trgt % "/home/" % usr % '/' % item);
                        }

                        if(! QDir().mkdir(trgt % "/home/" % usr % '/' % item) && ! fspchk(trgt)) return false;
                        break;
                    case Isfile:
                        skppd = QFile(srcdir % "/home/" % usr % '/' % item).size() > 8000000;

                        switch(stype(trgt % "/home/" % usr % '/' % item)) {
                        case Isfile:
                            switch(fcomp(trgt % "/home/" % usr % '/' % item, srcdir % "/home/" % usr % '/' % item)) {
                            case 1:
                                cpertime(srcdir % "/home/" % usr % '/' % item, trgt % "/home/" % usr % '/' % item);
                            case 2:
                                goto nitem_3;
                            }

                            if(skppd) goto nitem_3;
                        case Islink:
                            QFile::remove(trgt % "/home/" % usr % '/' % item);
                            break;
                        case Isdir:
                            recrmdir(trgt % "/home/" % usr % '/' % item);
                        }

                        if(! skppd && ! cpfile(srcdir % "/home/" % usr % '/' % item, trgt % "/home/" % usr % '/' % item) && ! fspchk(trgt)) return false;
                    }
                }

            nitem_3:
                if(ThrdKill) return false;
            }

            in.setString(cditms, QIODevice::ReadOnly);

            while(! in.atEnd())
            {
                QStr line(in.readLine()), item(right(line, -1));
                if(line.left(1).toUShort() == Isdir && exist(trgt % "/home/" % usr % '/' % item)) cpertime(srcdir % "/home/" % usr % '/' % item, trgt % "/home/" % usr % '/' % item);
                if(ThrdKill) return false;
            }

            cditms->clear();
            cpertime(srcdir % "/home/" % usr, trgt % "/home/" % usr);
        }
    }

    return true;
}

bool sb::thrdscopy(uchar mthd, cQStr &usr, cQStr &srcdir)
{
    QStr sysitms[12], rootitms;

    if((isdir(srcdir % "/bin") && ! rodir(sysitms[0], srcdir % "/bin")) ||
        (isdir(srcdir % "/boot") && ! rodir(sysitms[1], srcdir % "/boot")) ||
        (isdir(srcdir % "/etc") && ! rodir(sysitms[2], srcdir % "/etc")) ||
        (isdir(srcdir % "/lib") && ! rodir(sysitms[3], srcdir % "/lib")) ||
        (isdir(srcdir % "/lib32") && ! rodir(sysitms[4], srcdir % "/lib32")) ||
        (isdir(srcdir % "/lib64") && ! rodir(sysitms[5], srcdir % "/lib64")) ||
        (isdir(srcdir % "/opt") && ! rodir(sysitms[6], srcdir % "/opt")) ||
        (isdir(srcdir % "/sbin") && ! rodir(sysitms[7], srcdir % "/sbin")) ||
        (isdir(srcdir % "/selinux") && ! rodir(sysitms[8], srcdir % "/selinux")) ||
        (isdir(srcdir % "/srv") && ! rodir(sysitms[9], srcdir % "/srv")) ||
        (isdir(srcdir % "/usr") && ! rodir(sysitms[10], srcdir % "/usr")) ||
        (isdir(srcdir % "/var") && ! rodir(sysitms[11], srcdir % "/var")) ||
        (isdir(srcdir % "/root") && ! (mthd == 5 ? rodir(rootitms, srcdir % "/etc/skel") : rodir(rootitms, srcdir % "/root", true)))) return false;

    QSL homeitms, usrs;
    uint anum(0);
    for(uchar a(0) ; a < 12 ; ++a) anum += sysitms[a].count('\n');
    anum += rootitms.count('\n');

    if(mthd > 0)
    {
        if(isdir(srcdir % "/home"))
        {
            if(ilike(mthd, {1, 2}))
            {
                if(srcdir.isEmpty())
                {
                    QFile file("/etc/passwd");
                    if(! file.open(QIODevice::ReadOnly)) return false;

                    while(! file.atEnd())
                    {
                        QStr usr(file.readLine().trimmed());

                        if(usr.contains(":/home/"))
                        {
                            usr = left(usr, instr(usr, ":") -1);
                            if(isdir("/home/" % usr)) usrs.append(usr);
                        }
                    }
                }
                else
                    for(cQStr &usr : QDir(srcdir % "/home").entryList(QDir::Dirs | QDir::NoDotAndDotDot)) usrs.append(usr);
            }

            if(ThrdKill) return false;

            if(mthd < 3)
                for(uchar a(0) ; a < usrs.count() ; ++a)
                {
                    cQStr &usr(usrs.at(a));
                    homeitms.append(nullptr);
                    if(! rodir(homeitms.last(), srcdir % "/home/" % usr, mthd == 2)) return false;
                }
            else if(isdir(srcdir % "/home/" % usr))
            {
                usrs.append(usr);
                homeitms.append(nullptr);
                if(mthd < 5 && ! rodir(homeitms.last(), srcdir % "/home/" % usr, mthd == 3)) return false;
            }
        }

        for(uchar a(0) ; a < homeitms.count() ; ++a) anum += homeitms.at(a).count('\n');
    }

    Progress = 0;
    QStr *cditms, macid;
    uint cnum(0);
    uchar cperc;
    QSL elist({".cache/gvfs", ".gvfs", ".local/share/Trash/files/", ".local/share/Trash/info/", ".Xauthority", ".ICEauthority"});

    {
        QFile file(srcdir % "/etc/systemback.excludes");
        if(! file.open(QIODevice::ReadOnly)) return false;

        while(! file.atEnd())
        {
            QStr cline(file.readLine().trimmed());
            if(cline.startsWith('.')) elist.append(cline);
            if(ThrdKill) return false;
        }
    }

    if(mthd > 2)
    {
        QStr mid(isfile(srcdir % "/var/lib/dbus/machine-id") ? "/var/lib/dbus/machine-id" : isfile(srcdir % "/etc/machine-id") ? "/etc/machine-id" : nullptr);

        if(! mid.isEmpty())
        {
            QFile file(srcdir % mid);
            if(! file.open(QIODevice::ReadOnly)) return false;
            QStr line(file.readLine().trimmed());
            if(line.length() == 32) macid = line;
         }
    }

    bool skppd;

    if(isdir(srcdir % "/home"))
    {
        if(! isdir("/.sbsystemcopy/home") && ! QDir().mkdir("/.sbsystemcopy/home"))
        {
            QFile::rename("/.sbsystemcopy/home", "/.sbsystemcopy/home_" % rndstr());
            if(! QDir().mkdir("/.sbsystemcopy/home")) return false;
        }

        if(mthd > 0)
            for(uchar a(0) ; a < usrs.count() ; ++a)
            {
                cQStr &usr(usrs.at(a));

                if(! isdir("/.sbsystemcopy/home/" % usr))
                {
                    if(exist("/.sbsystemcopy/home/" % usr)) QFile::remove("/.sbsystemcopy/home/" % usr);

                    if(! cpdir(srcdir % "/home/" % usr, "/.sbsystemcopy/home/" % usr))
                    {
                        ThrdDbg = "@/home/" % usr;
                        return false;
                    }
                }

                cditms = mthd == 5 ? &rootitms : &homeitms[a];
                QTS in(cditms, QIODevice::ReadOnly);

                while(! in.atEnd())
                {
                    QStr line(in.readLine()), item(right(line, -1));
                    ++cnum;
                    cperc = (cnum * 100 + 50) / anum;
                    if(Progress < cperc) Progress = cperc;

                    if((mthd == 5 || (! like(item, {"_lost+found_", "_lost+found/*", "*/lost+found_", "*/lost+found/*", "_Systemback_", "_Systemback/*", "*/Systemback_", "*/Systemback/*", "*~_", "*~/*"}) && ! exclcheck(elist, item) && (macid.isEmpty() || ! item.contains(macid)))) && (! srcdir.isEmpty() || exist((mthd == 5 ? "/etc/skel/" : QStr("/home/" % usr % '/')) % item)))
                    {
                        switch(line.left(1).toUShort()) {
                        case Islink:
                            switch(stype("/.sbsystemcopy/home/" % usr % '/' % item)) {
                            case Islink:
                                if(! lcomp(srcdir % (mthd == 5 ? "/etc/skel/" : QStr("/home/" % usr % '/')) % item, "/.sbsystemcopy/home/" % usr % '/' % item)) goto nitem_1;
                            case Isfile:
                                QFile::remove("/.sbsystemcopy/home/" % usr % '/' % item);
                                break;
                            case Isdir:
                                recrmdir("/.sbsystemcopy/home/" % usr % '/' % item);
                            }

                            if(! cplink(srcdir % (mthd == 5 ? "/etc/skel/" : QStr("/home/" % usr % '/')) % item, "/.sbsystemcopy/home/" % usr % '/' % item)) goto err_1;
                            break;
                        case Isdir:
                            switch(stype("/.sbsystemcopy/home/" % usr % '/' % item)) {
                            case Isdir:
                                goto nitem_1;
                            default:
                                QFile::remove("/.sbsystemcopy/home/" % usr % '/' % item);
                            }

                            if(! QDir().mkdir("/.sbsystemcopy/home/" % usr % '/' % item)) return false;
                            break;
                        case Isfile:
                            skppd = ilike(mthd, {2, 3}) ? (QFile(srcdir % "/home/" % usr % '/' % item).size() > 8000000) : false;

                            switch(stype("/.sbsystemcopy/home/" % usr % '/' % item)) {
                            case Isfile:
                                if(mthd == 5)
                                {
                                    switch(fcomp("/.sbsystemcopy/home/" % usr % '/' % item, srcdir % "/etc/skel/" % item)) {
                                    case 1:
                                        if(! cpertime(srcdir % "/etc/skel/" % item, "/.sbsystemcopy/home/" % usr % '/' % item)) goto err_1;
                                    case 2:
                                        goto nitem_1;
                                    }
                                }
                                else
                                {
                                    switch(fcomp("/.sbsystemcopy/home/" % usr % '/' % item, srcdir % "/home/" % usr % '/' % item)) {
                                    case 1:
                                        if(! cpertime(srcdir % "/home/" % usr % '/' % item, "/.sbsystemcopy/home/" % usr % '/' % item)) goto err_1;
                                    case 2:
                                        goto nitem_1;
                                    }

                                    if(skppd) goto nitem_1;
                                }
                            case Islink:
                                QFile::remove("/.sbsystemcopy/home/" % usr % '/' % item);
                                break;
                            case Isdir:
                                recrmdir("/.sbsystemcopy/home/" % usr % '/' % item);
                            }

                            if(mthd == 5)
                            {
                                if(! cpfile(srcdir % "/etc/skel/" % item, "/.sbsystemcopy/home/" % usr % '/' % item)) goto err_1;
                            }
                            else if(! skppd && ! cpfile(srcdir % "/home/" % usr % '/' % item, "/.sbsystemcopy/home/" % usr % '/' % item))
                                goto err_1;
                        }
                    }

                nitem_1:
                    if(ThrdKill) return false;
                    continue;
                err_1:
                    ThrdDbg = (mthd == 5 ? "@/etc/skel/" : QStr("@/home/" % usr % '/')) % item;
                    return false;
                }

                in.setString(cditms, QIODevice::ReadOnly);

                while(! in.atEnd())
                {
                    QStr line(in.readLine()), item(right(line, -1));
                    if(line.left(1).toUShort() == Isdir && exist("/.sbsystemcopy/home/" % usr % '/' % item) && ! cpertime(srcdir % (mthd == 5 ? "/etc/skel/" : QStr("/home/" % usr % '/')) % item, "/.sbsystemcopy/home/" % usr % '/' % item)) goto err_2;
                    if(ThrdKill) return false;
                    continue;
                err_2:
                    ThrdDbg = (mthd == 5 ? "@/etc/skel/" : QStr("@/home/" % usr % '/')) % item;
                    return false;
                }

                if(mthd < 5 && a < 5) cditms->clear();

                if(isfile(srcdir % "/home/" % usr % "/.config/user-dirs.dirs"))
                {
                    QFile file(srcdir % "/home/" % usr % "/.config/user-dirs.dirs");
                    if(! file.open(QIODevice::ReadOnly)) return false;

                    while(! file.atEnd())
                    {
                        QStr cline(file.readLine().trimmed()), dir;

                        if(! cline.startsWith('#') && cline.contains("$HOME"))
                        {
                            dir = left(right(cline, - instr(cline, "/")), -1);

                            if(dir.length() > 0 && ! isdir("/.sbsystemcopy/home/" % usr % '/' % dir))
                            {
                                if(isdir(srcdir % "/home/" % usr % '/' % dir))
                                {
                                    if(! cpdir(srcdir % "/home/" % usr % '/' % dir, "/.sbsystemcopy/home/" % usr % '/' % dir)) goto err_3;
                                }
                                else if(srcdir.startsWith(sdir[1]) && (! QDir().mkdir("/.sbsystemcopy/home/" % usr % '/' % dir) || ! cpertime("/.sbsystemcopy/home/" % usr, "/.sbsystemcopy/home/" % usr % '/' % dir)))
                                    goto err_3;
                            }
                        }

                        if(ThrdKill) return false;
                        continue;
                    err_3:
                        ThrdDbg = "@/home/" % usr % '/' % dir;
                        return false;
                    }
                }

                if(! cpertime(srcdir % "/home/" % usr, "/.sbsystemcopy/home/" % usr))
                {
                    ThrdDbg = "@/home/" % usr;
                    return false;
                }
            }

        if(! cpertime(srcdir % "/home", "/.sbsystemcopy/home"))
        {
            ThrdDbg = "@/home";
            return false;
        }
    }

    if(isdir(srcdir % "/root"))
    {
        if(! isdir("/.sbsystemcopy/root") && ! QDir().mkdir("/.sbsystemcopy/root"))
        {
            QFile::rename("/.sbsystemcopy/root", "/.sbsystemcopy/root_" % rndstr());
            if(! QDir().mkdir("/.sbsystemcopy/root")) return false;
        }

        QTS in(&rootitms, QIODevice::ReadOnly);

        while(! in.atEnd())
        {
            QStr line(in.readLine()), item(right(line, -1));
            ++cnum;
            cperc = (cnum * 100 + 50) / anum;
            if(Progress < cperc) Progress = cperc;

            if((mthd == 5 || (! like(item, {"_lost+found_", "_lost+found/*", "*/lost+found_", "*/lost+found/*", "_Systemback_", "_Systemback/*", "*/Systemback_", "*/Systemback/*", "*~_", "*~/*"}) && ! exclcheck(elist, item) && (macid.isEmpty() || ! item.contains(macid)))) && (! srcdir.isEmpty() || exist((mthd == 5 ? "/etc/skel/" : "/root/") % item)))
            {
                switch(line.left(1).toUShort()) {
                case Islink:
                    switch(stype("/.sbsystemcopy/root/" % item)) {
                    case Islink:
                        if(! lcomp(srcdir % (mthd == 5 ? "/etc/skel/" : "/root/") % item, "/.sbsystemcopy/root/" % item)) goto nitem_2;
                    case Isfile:
                        QFile::remove("/.sbsystemcopy/root/" % item);
                        break;
                    case Isdir:
                        recrmdir("/.sbsystemcopy/root/" % item);
                    }

                    if(! cplink(srcdir % (mthd == 5 ? "/etc/skel/" : "/root/") % item, "/.sbsystemcopy/root/" % item)) goto err_4;
                    break;
                case Isdir:
                    switch(stype("/.sbsystemcopy/root/" % item)) {
                    case Isdir:
                        goto nitem_2;
                        break;
                    default:
                        QFile::remove("/.sbsystemcopy/root/" % item);
                    }

                    if(! QDir().mkdir("/.sbsystemcopy/root/" % item)) return false;
                    break;
                case Isfile:
                    skppd = ilike(mthd, {2, 3}) ? (QFile(srcdir % "/root/" % item).size() > 8000000) : false;

                    switch(stype("/.sbsystemcopy/root/" % item)) {
                    case Isfile:
                        if(mthd == 5)
                        {
                            switch(fcomp("/.sbsystemcopy/root/" % item, srcdir % "/etc/skel/" % item)) {
                            case 1:
                                if(! cpertime(srcdir % "/etc/skel/" % item, "/.sbsystemcopy/root/" % item)) goto err_4;
                            case 2:
                                goto nitem_2;
                            }
                        }
                        else
                        {
                            switch(fcomp("/.sbsystemcopy/root/" % item, srcdir % "/root/" % item)) {
                            case 1:
                                if(! cpertime(srcdir % "/root/" % item, "/.sbsystemcopy/root/" % item)) goto err_4;
                            case 2:
                                goto nitem_2;
                            }

                            if(skppd) goto nitem_2;
                        }
                    case Islink:
                        QFile::remove("/.sbsystemcopy/root/" % item);
                        break;
                    case Isdir:
                        recrmdir("/.sbsystemcopy/root/" % item);
                    }

                    if(mthd == 5)
                    {
                        if(! cpfile(srcdir % "/etc/skel/" % item, "/.sbsystemcopy/root/" % item)) goto err_4;
                    }
                    else if(! skppd && ! cpfile(srcdir % "/root/" % item, "/.sbsystemcopy/root/" % item))
                        goto err_4;
                }
            }

        nitem_2:
            if(ThrdKill) return false;
            continue;
        err_4:
            ThrdDbg = (mthd == 5 ? "@/etc/skel/" : "@/root/") % item;
            return false;
        }

        in.setString(&rootitms, QIODevice::ReadOnly);

        while(! in.atEnd())
        {
            QStr line(in.readLine()), item(right(line, -1));
            if(line.left(1).toUShort() == Isdir && exist("/.sbsystemcopy/root/" % item) && ! cpertime(srcdir % (mthd == 5 ? "/etc/skel/" : "/root/") % item, "/.sbsystemcopy/root/" % item)) goto err_5;
            if(ThrdKill) return false;
            continue;
        err_5:
            ThrdDbg = (mthd == 5 ? "@/etc/skel/" : "@/root/") % item;
            return false;
        }

        if(! cpertime(srcdir % "/root", "/.sbsystemcopy/root"))
        {
            ThrdDbg = "@/root";
            return false;
        }
        rootitms.clear();

    }

    for(cQStr &item : QDir("/.sbsystemcopy").entryList(QDir::Files))
    {
        if(like(item, {"_initrd.img*", "_vmlinuz*"}) && ! islink("/.sbsystemcopy/" % item) && ! islink(srcdir % '/' % item) && ! lcomp(srcdir % '/' % item, "/.sbsystemcopy/" % item) && ! QFile::remove("/.sbsystemcopy/" % item)) return false;
        if(ThrdKill) return false;
    }

    for(cQStr &item : QDir(srcdir.isEmpty() ? "/" : srcdir).entryList(QDir::Files))
    {
        if(like(item, {"_initrd.img*", "_vmlinuz*"}) && islink(srcdir % '/' % item) && ! exist("/.sbsystemcopy/" % item) && ! cplink(srcdir % '/' % item, "/.sbsystemcopy/" % item)) return false;
        if(ThrdKill) return false;
    }

    for(cQStr &cdir : {"/cdrom", "/dev", "/home", "/mnt", "/root", "/proc", "/run", "/sys", "/tmp"})
    {
        if(isdir(srcdir % cdir))
        {
            if(! isdir("/.sbsystemcopy" % cdir))
            {
                if(exist("/.sbsystemcopy" % cdir)) QFile::remove("/.sbsystemcopy" % cdir);

                if(! cpdir(srcdir % cdir, "/.sbsystemcopy" % cdir))
                {
                    ThrdDbg = '@' % cdir;
                    return false;
                }
            }
            else if(! cpertime(srcdir % cdir, "/.sbsystemcopy" % cdir))
            {
                ThrdDbg = '@' % cdir;
                return false;
            }
        }
        else if(exist("/.sbsystemcopy" % cdir))
            stype("/.sbsystemcopy" % cdir) == Isdir ? recrmdir("/.sbsystemcopy" % cdir) : QFile::remove("/.sbsystemcopy" % cdir);

        if(ThrdKill) return false;
    }

    elist = {"/boot/efi/EFI", "/etc/mtab", "/var/cache/fontconfig/", "/var/lib/dpkg/lock", "/var/lib/udisks/mtab", "/var/log", "/var/run/", "/var/tmp/"};
    if(mthd > 2) elist.append({"/etc/machine-id", "/etc/systemback.conf", "/etc/systemback.excludes", "/var/lib/dbus/machine-id"});
    if(srcdir == "/.systembacklivepoint" && fload("/proc/cmdline").contains("noxconf")) elist.append("/etc/X11/xorg.conf");
    QSL dlst = {"/bin", "/boot", "/etc", "/lib", "/lib32", "/lib64", "/opt", "/sbin", "/selinux", "/srv", "/usr", "/var"};

    for(uchar a(0) ; a < dlst.count() ; ++a)
    {
        cQStr &cdir(dlst.at(a));

        if(isdir(srcdir % cdir))
        {
            if(isdir("/.sbsystemcopy" % cdir))
            {
                QSL sdlst;
                if(! odir(sdlst, "/.sbsystemcopy" % cdir)) return false;

                for(cQStr &item : sdlst)
                {
                    if(! like(item, {"_lost+found_", "_Systemback_"}) && ! exist(srcdir % cdir % '/' % item)) stype("/.sbsystemcopy" % cdir % '/' % item) == Isdir ? recrmdir("/.sbsystemcopy" % cdir % '/' % item) : QFile::remove("/.sbsystemcopy" % cdir % '/' % item);
                    if(ThrdKill) return false;
                }
            }
            else
            {
                if(exist("/.sbsystemcopy" % cdir)) QFile::remove("/.sbsystemcopy" % cdir);
                if(! QDir().mkdir("/.sbsystemcopy" % cdir)) return false;
            }

            cditms = &sysitms[a];
            QTS in(cditms, QIODevice::ReadOnly);

            while(! in.atEnd())
            {
                QStr line(in.readLine()), item(right(line, -1));
                ++cnum;
                cperc = (cnum * 100 + 50) / anum;
                if(Progress < cperc) Progress = cperc;

                if(! like(item, {"_lost+found_", "_lost+found/*", "*/lost+found_", "*/lost+found/*", "_Systemback_", "_Systemback/*", "*/Systemback_", "*/Systemback/*"}) && ! exclcheck(elist, QStr(cdir % '/' % item)) && (macid.isEmpty() || ! item.contains(macid)) && (mthd < 3 || ! like(cdir % '/' % item, {"_/etc/udev/rules.d*", "*-persistent-*"}, All)) && (! srcdir.isEmpty() || exist(cdir % '/' % item)))
                {
                    switch(line.left(1).toUShort()) {
                    case Islink:
                        switch(stype("/.sbsystemcopy" % cdir % '/' % item)) {
                        case Islink:
                            if(lcomp(srcdir % cdir % '/' % item, "/.sbsystemcopy" % cdir % '/' % item)) goto nitem_3;
                        case Isfile:
                            QFile::remove("/.sbsystemcopy" % cdir % '/' % item);
                            break;
                        case Isdir:
                            recrmdir("/.sbsystemcopy" % cdir % '/' % item);
                        }

                        if(! cplink(srcdir % cdir % '/' % item, "/.sbsystemcopy" % cdir % '/' % item)) goto err_6;
                        break;
                    case Isdir:
                        switch(stype("/.sbsystemcopy" % cdir % '/' % item)) {
                        case Isdir:
                        {
                            QSL sdlst;
                            if(! odir(sdlst, "/.sbsystemcopy" % cdir % '/' % item)) return false;

                            for(cQStr &sitem : sdlst)
                            {
                                if(! like(sitem, {"_lost+found_", "_Systemback_"}) && (! exist(srcdir % cdir % '/' % item % '/' % sitem) || cdir % '/' % item % '/' % sitem == "/etc/X11/xorg.conf")) stype("/.sbsystemcopy" % cdir % '/' % item % '/' % sitem) == Isdir ? recrmdir("/.sbsystemcopy" % cdir % '/' % item % '/' % sitem) : QFile::remove("/.sbsystemcopy" % cdir % '/' % item % '/' % sitem);
                                if(ThrdKill) return false;
                            }

                            goto nitem_3;
                        }
                        case Islink:
                        case Isfile:
                            QFile::remove("/.sbsystemcopy" % cdir % '/' % item);
                        }

                        if(! QDir().mkdir("/.sbsystemcopy" % cdir % '/' % item)) return false;
                        break;
                    case Isfile:
                        switch(stype("/.sbsystemcopy" % cdir % '/' % item)) {
                        case Isfile:
                            switch(fcomp("/.sbsystemcopy" % cdir % '/' % item, srcdir % cdir % '/' % item)) {
                            case 1:
                                if(! cpertime(srcdir % cdir % '/' % item, "/.sbsystemcopy" % cdir % '/' % item)) goto err_6;
                            case 2:
                                goto nitem_3;
                            }
                        case Islink:
                            QFile::remove("/.sbsystemcopy" % cdir % '/' % item);
                            break;
                        case Isdir:
                            recrmdir("/.sbsystemcopy" % cdir % '/' % item);
                        }

                        if(! cpfile(srcdir % cdir % '/' % item, "/.sbsystemcopy" % cdir % '/' % item)) goto err_6;
                    }
                }

            nitem_3:
                if(ThrdKill) return false;
                continue;
            err_6:
                ThrdDbg = '@' % cdir % '/' % item;
                return false;
            }

            in.setString(cditms, QIODevice::ReadOnly);

            while(! in.atEnd())
            {
                QStr line(in.readLine()), item(right(line, -1));
                if(line.left(1).toUShort() == Isdir && exist("/.sbsystemcopy" % cdir % '/' % item) && ! cpertime(srcdir % cdir % '/' % item, "/.sbsystemcopy" % cdir % '/' % item)) goto err_7;
                if(ThrdKill) return false;
                continue;
            err_7:
                ThrdDbg = '@' % cdir % '/' % item;
                return false;
            }

            if(! cpertime(srcdir % cdir, "/.sbsystemcopy" % cdir))
            {
                ThrdDbg = '@' % cdir;
                return false;
            }

            cditms->clear();
        }
        else if(exist("/.sbsystemcopy" % cdir))
            stype("/.sbsystemcopy" % cdir) == Isdir ? recrmdir("/.sbsystemcopy" % cdir) : QFile::remove("/.sbsystemcopy" % cdir);
    }

    if(isdir(srcdir % "/media"))
    {
        if(isdir("/.sbsystemcopy/media"))
            for(cQStr &item : QDir("/media").entryList(QDir::Dirs | QDir::NoDotAndDotDot))
            {
                if(! exist(srcdir % "/media/" % item) && ! mcheck("/.sbsystemcopy/media/" % item % '/')) recrmdir("/.sbsystemcopy/media/" % item);
                if(ThrdKill) return false;
            }
        else
        {
            if(exist("/.sbsystemcopy/media")) QFile::remove("/.sbsystemcopy/media");
            if(! QDir().mkdir("/.sbsystemcopy/media")) return false;
        }

        if(srcdir.isEmpty())
        {
            if(isfile("/etc/fstab"))
            {
                dlst = QDir("/media").entryList(QDir::Dirs | QDir::NoDotAndDotDot);
                QFile file("/etc/fstab");
                if(! file.open(QIODevice::ReadOnly)) return false;

                for(uchar a(0) ; a < dlst.count() ; ++a)
                {
                    cQStr &item(dlst.at(a));
                    if(a > 0 && ! file.open(QIODevice::ReadOnly)) return false;

                    while(! file.atEnd())
                    {
                        QStr cline(file.readLine().trimmed().replace('\t', ' ')), fdir;

                        if(! cline.startsWith('#') && like(cline.replace("\\040", " "), {"* /media/" % item % " *", "* /media/" % item % "/*"}))
                            for(QStr cdname : QStr(mid(cline, instr(cline, "/media/") + 7, instr(cline, " ", instr(cline, "/media/")) - instr(cline, "/media/") - 7)).split('/'))
                                if(! cdname.isEmpty())
                                {
                                    fdir.append('/' % cdname.replace("\\040", " "));
                                    if(! isdir("/.sbsystemcopy/media" % fdir) && ! cpdir("/media" % fdir, "/.sbsystemcopy/media" % fdir)) goto err_8;
                                }

                        if(ThrdKill) return false;
                        continue;
                    err_8:
                        ThrdDbg = "@/media" % fdir;
                        return false;
                    }

                    file.close();
                }
            }
        }
        else
        {
            QStr mediaitms;
            if(! rodir(mediaitms, srcdir % "/media")) return false;
            QTS in(&mediaitms, QIODevice::ReadOnly);

            while(! in.atEnd())
            {
                QStr line(in.readLine()), item(right(line, -1));

                if(exist("/.sbsystemcopy/media/" % item))
                {
                    if(stype("/.sbsystemcopy/media/" % item) != Isdir)
                    {
                        QFile::remove("/.sbsystemcopy/media/" % item);
                        if(! QDir().mkdir("/.sbsystemcopy/media/" % item)) return false;
                    }

                    if(! cpertime(srcdir % "/media/" % item, "/.sbsystemcopy/media/" % item)) goto err_9;
                }
                else if(! cpdir(srcdir % "/media/" % item, "/.sbsystemcopy/media/" % item))
                    goto err_9;

                if(ThrdKill) return false;
                continue;
            err_9:
                ThrdDbg = "@/media/" % item;
                return false;
            }
        }

        if(! cpertime(srcdir % "/media", "/.sbsystemcopy/media"))
        {
            ThrdDbg = "@/media";
            return false;
        }
    }
    else if(exist("/.sbsystemcopy/media"))
        stype("/.sbsystemcopy/media") == Isdir ? recrmdir("/.sbsystemcopy/media") : QFile::remove("/.sbsystemcopy/media");

    if(exist("/.sbsystemcopy/var/log")) stype("/.sbsystemcopy/var/log") == Isdir ? recrmdir("/.sbsystemcopy/var/log") : QFile::remove("/.sbsystemcopy/var/log");

    if(isdir(srcdir % "/var/log"))
    {
        if(! QDir().mkdir("/.sbsystemcopy/var/log")) return false;
        QStr logitms;
        if(! rodir(logitms, srcdir % "/var/log")) return false;
        QTS in(&logitms, QIODevice::ReadOnly);

        while(! in.atEnd())
        {
            QStr line(in.readLine()), item(right(line, -1));

            switch(line.left(1).toUShort()) {
            case Isdir:
                if(! cpdir(srcdir % "/var/log/" % item, "/.sbsystemcopy/var/log/" % item)) goto err_10;
                break;
            case Isfile:
                if(! like(item, {"*.gz_", "*.old_"}) && (! item.contains('.') || ! isnum(right(item, - rinstr(item, ".")))))
                {
                    crtfile("/.sbsystemcopy/var/log/" % item);
                    if(! cpertime(srcdir % "/var/log/" % item, "/.sbsystemcopy/var/log/" % item)) goto err_10;
                }
            }

            if(ThrdKill) return false;
            continue;
        err_10:
            ThrdDbg = "@/var/log/" % item;
            return false;
        }

        in.setString(&logitms, QIODevice::ReadOnly);

        while(! in.atEnd())
        {
            QStr line(in.readLine()), item(right(line, -1));
            if(line.left(1).toUShort() == Isdir && ! cpertime(srcdir % "/var/log/" % item, "/.sbsystemcopy/var/log/" % item)) goto err_11;
            if(ThrdKill) return false;
            continue;
        err_11:
            ThrdDbg = "@/var/log/" % item;
            return false;
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
        QStr sbitms;
        if(! rodir(sbitms, srcdir % "/.systemback")) return false;
        QTS in(&sbitms, QIODevice::ReadOnly);

        while(! in.atEnd())
        {
            QStr line(in.readLine()), item(right(line, -1));

            switch(line.left(1).toUShort()) {
            case Islink:
                if(item != "etc/fstab" && ! cplink("/.systembacklivepoint/.systemback/" % item, "/.sbsystemcopy/" % item)) goto err_12;
                break;
            case Isfile:
                if(item != "etc/fstab" && ! cpfile("/.systembacklivepoint/.systemback/" % item, "/.sbsystemcopy/" % item)) goto err_12;
            }

            if(ThrdKill) return false;
            continue;
        err_12:
            ThrdDbg = "@/.systemback/" % item;
            return false;
        }

        in.setString(&sbitms, QIODevice::ReadOnly);

        while(! in.atEnd())
        {
            QStr line(in.readLine()), item(right(line, -1));
            if(line.left(1).toUShort() == Isdir && ! cpertime("/.systembacklivepoint/.systemback/" % item, "/.sbsystemcopy/" % item)) goto err_13;
            if(ThrdKill) return false;
            continue;
        err_13:
            ThrdDbg = "@/.systemback/" % item;
            return false;
        }
    }

    return true;
}

bool sb::thrdlvprpr(bool iudata)
{
    if(ThrdLng[0] > 0) ThrdLng[0] = 0;
    QStr sitms;

    for(cQStr &item : {"/bin", "/boot", "/etc", "/lib", "/lib32", "/lib64", "/opt", "/sbin", "/selinux", "/usr", "/initrd.img", "/initrd.img.old", "/vmlinuz", "/vmlinuz.old"})
        if(isdir(item))
        {
            if(! rodir(sitms, item)) return false;
            ++ThrdLng[0];
        }

    ThrdLng[0] += sitms.count('\n');
    sitms.clear();
    if(! QDir().mkdir(sdir[2] % "/.sblivesystemcreate/.systemback") || ! QDir().mkdir(sdir[2] % "/.sblivesystemcreate/.systemback/etc")) return false;

    if(isdir("/etc/udev"))
    {
        if(! QDir().mkdir(sdir[2] % "/.sblivesystemcreate/.systemback/etc/udev")) return false;

        if(isdir("/etc/udev/rules.d") && (! QDir().mkdir(sdir[2] % "/.sblivesystemcreate/.systemback/etc/udev/rules.d") ||
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
    if(exist("/.sblvtmp")) stype("/.sblvtmp") == Isdir ? recrmdir("/.sblvtmp") : QFile::remove("/.sblvtmp");
    if(! QDir().mkdir("/.sblvtmp")) return false;

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
        if(! QDir().mkdir("/media")) return false;
    }
    else if(exist("/media/.sblvtmp"))
       stype("/media/.sblvtmp") == Isdir ? recrmdir("/media/.sblvtmp") : QFile::remove("/media/.sblvtmp");

    if(! QDir().mkdir("/media/.sblvtmp") || ! QDir().mkdir("/media/.sblvtmp/media")) return false;
    ++ThrdLng[0];
    if(ThrdKill) return false;

    if(isfile("/etc/fstab"))
    {
        QSL dlst(QDir("/media").entryList(QDir::Dirs | QDir::NoDotAndDotDot));
        QFile file("/etc/fstab");
        if(! file.open(QIODevice::ReadOnly)) return false;

        for(uchar a(0) ; a < dlst.count() ; ++a)
        {
            cQStr &item(dlst.at(a));
            if(a > 0 && ! file.open(QIODevice::ReadOnly)) return false;

            while(! file.atEnd())
            {
                QStr cline(file.readLine().trimmed().replace('\t', ' ')), fdir;

                if(! cline.startsWith('#') && like(cline.replace("\\040", " "), {"* /media/" % item % " *", "* /media/" % item % "/*"}))
                    for(QStr cdname : QStr(mid(cline, instr(cline, "/media/") + 7, instr(cline, " ", instr(cline, "/media/")) - instr(cline, "/media/") - 7)).split('/'))
                        if(! cdname.isEmpty())
                        {
                            fdir.append('/' % cdname.replace("\\040", " "));

                            if(! isdir("/media/.sblvtmp/media" % fdir))
                            {
                                if(! cpdir("/media" % fdir, "/media/.sblvtmp/media" % fdir)) goto err_1;
                                ++ThrdLng[0];
                            }
                        }

                if(ThrdKill) return false;
                continue;
            err_1:
                ThrdDbg = "@/media" % fdir;
                return false;
            }

            file.close();
        }
    }

    if(exist("/var/.sblvtmp")) stype("/var/.sblvtmp") == Isdir ? recrmdir("/var/.sblvtmp") : QFile::remove("/var/.sblvtmp");
    if(ThrdKill) return false;
    QStr varitms;
    if(! rodir(varitms, "/var")) return false;
    QTS in(&varitms, QIODevice::ReadOnly);
    if(ThrdKill) return false;
    if(! QDir().mkdir("/var/.sblvtmp") || ! QDir().mkdir("/var/.sblvtmp/var")) return false;
    ++ThrdLng[0];
    QSL elist({"lib/dpkg/lock", "lib/udisks/mtab", "lib/ureadahead/", "log/", "run/", "tmp/"});

    while(! in.atEnd())
    {
        QStr line(in.readLine()), item(right(line, -1));

        if(exist("/var/" % item))
        {
            if(! like(item, {"+_cache/apt/*", "-*.bin_", "-*.bin.*"}, Mixed) && ! like(item, {"_cache/apt/archives/*", "*.deb_"}, All) && ! like(item, {"_lost+found_", "_lost+found/*", "*/lost+found_", "*/lost+found/*", "_Systemback_", "_Systemback/*", "*/Systemback_", "*/Systemback/*", "*.dpkg-old_", "*~_", "*~/*"}) && ! exclcheck(elist, item))
            {
                switch(line.left(1).toUShort()) {
                case Islink:
                    if(! cplink("/var/" % item, "/var/.sblvtmp/var/" % item)) goto err_2;
                    ++ThrdLng[0];
                    break;
                case Isdir:
                    if(! QDir().mkdir("/var/.sblvtmp/var/" % item)) return false;
                    ++ThrdLng[0];
                    break;
                case Isfile:
                    if(issmfs("/var/.sblvtmp", "/var/" % item) && link(QStr("/var/" % item).toStdString().c_str(), QStr("/var/.sblvtmp/var/" % item).toStdString().c_str()) == -1) goto err_2;
                    ++ThrdLng[0];
                }
            }
            else if(item.startsWith("log"))
            {
                switch(line.left(1).toUShort()) {
                case Isdir:
                    if(! QDir().mkdir("/var/.sblvtmp/var/" % item)) return false;
                    ++ThrdLng[0];
                    break;
                case Isfile:
                    if(! like(item, {"*.gz_", "*.old_"}) && (! item.contains('.') || ! isnum(right(item, - rinstr(item, ".")))))
                    {
                        crtfile("/var/.sblvtmp/var/" % item);
                        if(! cpertime("/var/" % item, "/var/.sblvtmp/var/" % item)) goto err_2;
                        ++ThrdLng[0];
                    }
                }
            }
        }

        if(ThrdKill) return false;
        continue;
    err_2:
        ThrdDbg = "@/var/" % item;
        return false;
    }

    in.setString(&varitms, QIODevice::ReadOnly);

    while(! in.atEnd())
    {
        QStr line(in.readLine()), item(right(line, -1));
        if(line.left(1).toUShort() == Isdir && exist("/var/.sblvtmp/var/" % item) && ! cpertime("/var/" % item, "/var/.sblvtmp/var/" % item)) goto err_3;
        if(ThrdKill) return false;
        continue;
    err_3:
        ThrdDbg = "@/var/" % item;
        return false;
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

            if(usr.contains(":/home/"))
            {
                usr = left(usr, instr(usr, ":") -1);
                if(isdir("/home/" % usr)) usrs.append(usr);
            }
        }
    }

    if(ThrdKill) return false;
    bool uhl(true);

    if(dfree("/home") > 104857600 && dfree("/root") > 104857600)
        for(cQStr &usr : usrs)
        {
            if(! issmfs("/home", "/home/" % usr))
            {
                uhl = false;
                break;
            }

            if(ThrdKill) return false;
        }
    else
        uhl = false;

    if(uhl)
    {
        if(exist("/home/.sbuserdata")) stype("/home/.sbuserdata") == Isdir ? recrmdir("/home/.sbuserdata") : QFile::remove("/home/.sbuserdata");
        if(! QDir().mkdir("/home/.sbuserdata") || ! QDir().mkdir("/home/.sbuserdata/home")) return false;
    }
    else if(! QDir().mkdir(sdir[2] % "/.sblivesystemcreate/userdata") || ! QDir().mkdir(sdir[2] % "/.sblivesystemcreate/userdata/home"))
        return false;

    ++ThrdLng[0];
    if(ThrdKill) return false;
    elist = {".sbuserdata", ".cache/gvfs", ".local/share/Trash/files/", ".local/share/Trash/info/", ".Xauthority", ".ICEauthority"};

    {
        QFile file("/etc/systemback.excludes");
        if(! file.open(QIODevice::ReadOnly)) return false;

        while(! file.atEnd())
        {
            QStr cline(file.readLine().trimmed());
            elist.append(cline);
            if(ThrdKill) return false;
        }
    }

    if(isdir("/root"))
    {
        QStr usdir;

        if(uhl)
        {
            usdir = "/root/.sbuserdata";
            if(exist("/root/.sbuserdata")) stype("/root/.sbuserdata") == Isdir ? recrmdir("/root/.sbuserdata") : QFile::remove("/root/.sbuserdata");
            if(! QDir().mkdir(usdir)) return false;
        }
        else
            usdir = sdir[2] % "/.sblivesystemcreate/userdata";

        if(! QDir().mkdir(usdir % "/root")) return false;
        ++ThrdLng[0];
        if(ThrdKill) return false;
        QStr rootitms;
        if(! rodir(rootitms, "/root", ! iudata)) return false;
        in.setString(&rootitms, QIODevice::ReadOnly);

        while(! in.atEnd())
        {
            QStr line(in.readLine()), item(right(line, -1));

            if(! like(item, {"_lost+found_", "_lost+found/*", "*/lost+found_", "*/lost+found/*", "_Systemback_", "_Systemback/*", "*/Systemback_", "*/Systemback/*", "*~_", "*~/*"}) && ! exclcheck(elist, item) && exist("/root/" % item))
            {
                switch(line.left(1).toUShort()) {
                case Islink:
                    if(uhl)
                    {
                        if(link(QStr("/root/" % item).toStdString().c_str(), QStr(usdir % "/root/" % item).toStdString().c_str()) == -1) goto err_4;
                    }
                    else if(! cplink("/root/" % item, usdir % "/root/" % item))
                        goto err_4;

                    ++ThrdLng[0];
                    break;
                case Isdir:
                    if(! QDir().mkdir(usdir % "/root/" % item)) return false;
                    ++ThrdLng[0];
                    break;
                case Isfile:
                    if(iudata || QFile("/root/" % item).size() <= 8000000)
                    {
                        if(uhl)
                        {
                            if(link(QStr("/root/" % item).toStdString().c_str(), QStr(usdir % "/root/" % item).toStdString().c_str()) == -1) goto err_4;
                        }
                        else if(! cpfile("/root/" % item, usdir % "/root/" % item))
                            goto err_4;

                        ++ThrdLng[0];
                    }
                }
            }

            if(ThrdKill) return false;
            continue;
        err_4:
            ThrdDbg = "@/root/" % item;
            return false;
        }

        in.setString(&rootitms, QIODevice::ReadOnly);

        while(! in.atEnd())
        {
            QStr line(in.readLine()), item(right(line, -1));
            if(line.left(1).toUShort() == Isdir && exist(usdir % "/root/" % item) && ! cpertime("/root/" % item, usdir % "/root/" % item)) goto err_5;
            if(ThrdKill) return false;
            continue;
        err_5:
            ThrdDbg = "@/root/" % item;
            return false;
        }

        if(! cpertime("/root", usdir % "/root"))
        {
            ThrdDbg = "@/root";
            return false;
        }
    }

    for(cQStr &udir : usrs)
    {
        QStr usdir(uhl ? "/home/.sbuserdata/home" : QStr(sdir[2] % "/.sblivesystemcreate/userdata/home"));
        if(! QDir().mkdir(usdir % '/' % udir)) return false;
        ++ThrdLng[0];
        QStr useritms;
        if(! rodir(useritms, "/home/" % udir, ! iudata)) return false;
        in.setString(&useritms, QIODevice::ReadOnly);

        while(! in.atEnd())
        {
            QStr line(in.readLine()), item(right(line, -1));

            if(! like(item, {"_lost+found_", "_lost+found/*", "*/lost+found_", "*/lost+found/*", "_Systemback_", "_Systemback/*", "*/Systemback_", "*/Systemback/*", "*~_", "*~/*"}) && ! exclcheck(elist, item) && exist("/home/" % udir % '/' % item))
            {
                switch(line.left(1).toUShort()) {
                case Islink:
                    if(uhl)
                    {
                        if(link(QStr("/home/" % udir % '/' % item).toStdString().c_str(), QStr(usdir % '/' % udir % '/' % item).toStdString().c_str()) == -1) goto err_6;
                    }
                    else if(! cplink("/home/" % udir % '/' % item, usdir % '/' % udir % '/' % item))
                        goto err_6;

                    ++ThrdLng[0];
                    break;
                case Isdir:
                    if(! QDir().mkdir(usdir % '/' % udir % '/' % item)) return false;
                    ++ThrdLng[0];
                    break;
                case Isfile:
                    if(iudata || QFile("/home/" % udir % '/' % item).size() <= 8000000)
                    {
                        if(uhl)
                        {
                            if(link(QStr("/home/" % udir % '/' % item).toStdString().c_str(), QStr(usdir % '/' % udir % '/' % item).toStdString().c_str()) == -1) goto err_6;
                        }
                        else if(! cpfile("/home/" % udir % '/' % item, usdir % '/' % udir % '/' % item))
                            goto err_6;

                        ++ThrdLng[0];
                    }
                }
            }

            if(ThrdKill) return false;
            continue;
        err_6:
            ThrdDbg = "@/home/" % udir % '/' % item;
            return false;
        }

        in.setString(&useritms, QIODevice::ReadOnly);

        while(! in.atEnd())
        {
            QStr line(in.readLine()), item(right(line, -1));
            if(line.left(1).toUShort() == Isdir && exist(usdir % '/' % udir % '/' % item) && ! cpertime("/home/" % udir % '/' % item, usdir % '/' % udir % '/' % item)) goto err_7;
            if(ThrdKill) return false;
            continue;
        err_7:
            ThrdDbg = "@/home/" % udir % '/' % item;
            return false;
        }

        if(! iudata && isfile("/home/" % udir % "/.config/user-dirs.dirs"))
        {
            QFile file("/home/" % udir % "/.config/user-dirs.dirs");
            if(! file.open(QIODevice::ReadOnly)) return false;

            while(! file.atEnd())
            {
                QStr cline(file.readLine().trimmed()), dir;

                if(! cline.startsWith('#') && cline.contains("$HOME"))
                {
                    dir = left(right(cline, - instr(cline, "/")), -1);

                    if(dir.length() > 0 && isdir("/home/" % udir % '/' % dir) && ! isdir(usdir % '/' % udir % '/' % dir))
                    {
                        if(! cpdir("/home/" % udir % '/' % dir, usdir % '/' % udir % '/' % dir)) goto err_8;
                        ++ThrdLng[0];
                    }
                }

                if(ThrdKill) return false;
                continue;
            err_8:
                ThrdDbg = "@/home/" % udir % '/' % dir;
                return false;
            }

            file.close();
        }

        if(! cpertime("/home/" % udir, usdir % '/' % udir))
        {
            ThrdDbg = "@/home/" % udir;
            return false;
        }
    }

    return true;
}
