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
#include <QTextStream>
#include <QProcess>
#include <QTime>
#include <QDir>
#include <sys/resource.h>
#include <blkid/blkid.h>
#include <sys/statfs.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <linux/fs.h>
#include <unistd.h>
#include <dirent.h>
#include <utime.h>
#include <fcntl.h>

sb sb::SBThrd;
QStr sb::sdir[3], sb::schdle[7], sb::pnames[15], sb::trn[2];
uchar sb::pnumber(0);
char sb::sblock, sb::dpkglock, sb::schdlrlock;
bool sb::ExecKill;

sb::sb(QThread *parent) : QThread(parent)
{
    ThrdLng = 0;
    ExecKill = SBThrd.ThrdKill = true;
}

QStr sb::getarch()
{
    char *a;

    switch(sizeof(a)) {
    case 4:
        return "i386";
    case 8:
        return "amd64";
    default:
        return "arch?";
    }
}

void sb::print(QStr txt)
{
    QTS(stdout) << "\033[1m" % txt % "\033[0m";
}

void sb::error(QStr txt)
{
    QTS(stderr) << "\033[1;31m" % txt % "\033[0m";
}

void sb::delay(ushort msec)
{
    QTime time;
    time.start();

    while(uint(time.elapsed()) < msec)
    {
        msleep(10);
        qApp->processEvents();
    }
}

QStr sb::left(QStr txt, short len)
{
    ushort plen(abs(len));
    if(len < 0) return (txt.length() >= plen) ? txt.left(txt.length() - plen) : NULL;
    return (txt.length() >= plen) ? txt.left(len) : txt;
}

QStr sb::right(QStr txt, short len)
{
    ushort plen(abs(len));
    if(len < 0) return (txt.length() >= plen) ? txt.right(txt.length() - plen) : NULL;
    return (txt.length() >= plen) ? txt.right(len) : txt;
}

QStr sb::mid(QStr txt, ushort start, ushort len)
{
    if(txt.length() >= start) return (txt.length() - start + 1 >= len) ? txt.mid(start - 1, len) : right(txt, - start + 1);
    return NULL;
}

QStr sb::replace(QStr txt, QStr stxt, QStr rtxt)
{
    while(txt.contains(stxt)) txt = txt.replace(txt.indexOf(stxt), stxt.length(), rtxt);
    return txt;
}

bool sb::like(QStr txt, QSL lst)
{
    for(uchar a(0) ; a < lst.count() ; ++a)
    {
        QStr stxt(lst.at(a));

        if(stxt.startsWith("*"))
        {
            if(stxt.endsWith("*"))
            {
                if(txt.contains(stxt.mid(1, stxt.length() - 2))) return true;
            }
            else if(txt.endsWith(stxt.mid(1, stxt.length() - 2)))
                return true;
        }
        else if(stxt.endsWith("*"))
        {
            if(txt.startsWith(stxt.mid(1, stxt.length() - 2))) return true;
        }
        else if(txt == stxt.mid(1, stxt.length() - 2))
            return true;
    }

    return false;
}

bool sb::ilike(short num, QSIL lst)
{
    for(uchar a(0) ; a < lst.count() ; ++a)
        if(num == lst.at(a)) return true;

    return false;
}

ushort sb::instr(QStr txt, QStr stxt, ushort start)
{
    return (txt.length() + 1 >= stxt.length() + start) ? txt.indexOf(stxt, start - 1) + 1 : 0;
}

ushort sb::rinstr(QStr txt, QStr stxt, ushort start)
{
    return (start == 0) ? txt.lastIndexOf(stxt) + 1 : (start >= stxt.length()) ? txt.left(start).lastIndexOf(stxt) + 1 : 0;
}

QStr sb::rndstr(uchar vlen)
{
    QStr val, chrs("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz./"), chr;
    uchar clen((vlen == 16) ? 64 : 62);

    while(val.length() < vlen)
    {
        chr = chrs.mid(rand() % clen, 1);
        if(! val.endsWith(chr)) val.append(chr);
    }

    return val;
}

bool sb::exist(QStr path)
{
    struct stat64 buf;
    return lstat64(path.toStdString().c_str(), &buf) ? false : true;
}

bool sb::access(QStr path, uchar mode)
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

uchar sb::stype(QStr path)
{
    struct stat64 buf;
    if(lstat64(path.toStdString().c_str(), &buf)) return Notexist;
    if(S_ISREG(buf.st_mode)) return Isfile;
    if(S_ISDIR(buf.st_mode)) return Isdir;
    if(S_ISLNK(buf.st_mode)) return Islink;
    if(S_ISBLK(buf.st_mode)) return Isblock;
    return Unknow;
}

QStr sb::rlink(QStr path)
{
    struct stat64 lnkstat;
    lstat64(path.toStdString().c_str(), &lnkstat);
    char rpath[lnkstat.st_size];
    rpath[readlink(path.toStdString().c_str(), rpath, sizeof(rpath))] = '\0';
    return rpath;
}

QString sb::fload(QString path)
{
    QFile file(path);
    file.open(QIODevice::ReadOnly);
    QTS in(&file);
    return in.readAll();
}

bool sb::crtfile(QStr path, QStr txt)
{
    if(! ilike(stype(path), QSIL() << Notexist << Isfile) || ! isdir(path.left(path.lastIndexOf('/')))) return false;
    QFile file(path);
    file.open(QFile::WriteOnly | QFile::Truncate);
    QTS out(&file);
    out << txt;
    file.flush();
    file.close();
    return true;
}

bool sb::lock(uchar type)
{
    switch(type) {
    case Sblock:
    {
        QStr lfile(isdir("/run") ? "/run/systemback.lock" : "/var/run/systemback.lock");
        if(! exist(lfile)) crtfile(lfile, NULL);
        sblock = open(lfile.toStdString().c_str(), O_RDWR);
        return (lockf(sblock, F_TLOCK, 0) == -1) ? false : true;
    }
    case Dpkglock:
        if(! exist("/var/lib/dpkg/lock")) crtfile("/var/lib/dpkg/lock", NULL);
        dpkglock = open("/var/lib/dpkg/lock", O_RDWR);
        return (lockf(dpkglock, F_TLOCK, 0) == -1) ? false : true;
    case Schdlrlock:
        QStr lfile(isdir("/run") ? "/run/sbscheduler.lock" : "/var/run/sbscheduler.lock");
        if(! exist(lfile)) crtfile(lfile, NULL);
        schdlrlock = open(lfile.toStdString().c_str(), O_RDWR);
        return (lockf(schdlrlock, F_TLOCK, 0) == -1) ? false : true;
    }

    return false;
}

void sb::unlock(uchar type)
{
    switch(type) {
    case Sblock:
        close(sblock);
        break;
    case Dpkglock:
        close(dpkglock);
        break;
    case Schdlrlock:
        close(schdlrlock);
    }
}

quint64 sb::dfree(QStr path)
{
    struct statfs64 dstat;
    return (statfs64(path.toStdString().c_str(), &dstat) == -1) ? 0 : dstat.f_bavail * dstat.f_bsize;
}

quint64 sb::fsize(QStr path)
{
    return QFileInfo(path).size();
}

bool sb::remove(QStr path)
{
    SBThrd.ThrdType = Remove;
    SBThrd.ThrdStr[0] = path;
    SBThrd.start();
    SBThrd.thrdelay();
    return SBThrd.ThrdRslt ? true : false;
}

bool sb::islnxfs(QStr dirpath)
{
    QStr fpath(dirpath % "/dacc5b5f4502_sbdirtestfile");
    if(! crtfile(fpath, NULL)) return false;
    chmod(fpath.toStdString().c_str(), 0776);
    struct stat64 fstat;
    stat64(fpath.toStdString().c_str(), &fstat);
    QFile::remove(fpath);
    return (fstat.st_mode == 33278) ? true : false;
}

void sb::cfgread()
{
    bool cfgupdt(false);

    if(isfile("/etc/systemback.conf"))
    {
        QFile file("/etc/systemback.conf");
        file.open(QIODevice::ReadOnly);
        QTS in(&file);

        while(! in.atEnd())
        {
            QStr cline(in.readLine());

            if(cline.startsWith("storagedir="))
                sdir[0] = right(cline, - instr(cline, "="));
            else if(cline.startsWith("liveworkdir="))
                sdir[2] = right(cline, - instr(cline, "="));
            else if(cline.startsWith("schedule="))
            {
                QStr cval(right(cline, - instr(cline, "=")));
                schdle[1] = left(cval, instr(cval, ":") - 1);
                schdle[2] = mid(cval, schdle[1].length() + 2, instr(cval, ":", schdle[1].length() + 2) - instr(cval, ":") - 1);
                schdle[3] = right(left(cval, instr(cval, ":", QStr(schdle[1] % schdle[2]).length() + 3) - 1), - QStr(schdle[1] % schdle[2]).length() - 2);
                schdle[4] = right(cval, - rinstr(cval, ":"));
            }
            else if(cline.startsWith("pointsnumber=3"))
                pnumber = 3;
            else if(cline.startsWith("pointsnumber=4"))
                pnumber = 4;
            else if(cline.startsWith("pointsnumber=5"))
                pnumber = 5;
            else if(cline.startsWith("pointsnumber=6"))
                pnumber = 6;
            else if(cline.startsWith("pointsnumber=7"))
                pnumber = 7;
            else if(cline.startsWith("pointsnumber=8"))
                pnumber = 8;
            else if(cline.startsWith("pointsnumber=9"))
                pnumber = 9;
            else if(cline.startsWith("pointsnumber=10"))
                pnumber = 10;
            else if(cline.startsWith("timer=on"))
                schdle[0] = "on";
            else if(cline.startsWith("timer=off"))
                schdle[0] = "off";
            else if(cline.startsWith("silentmode=on"))
                schdle[5] = "on";
            else if(cline.startsWith("silentmode=off"))
                schdle[5] = "off";
            else if(cline.startsWith("windowposition=topleft"))
                schdle[6] = "topleft";
            else if(cline.startsWith("windowposition=topright"))
                schdle[6] = "topright";
            else if(cline.startsWith("windowposition=center"))
                schdle[6] = "center";
            else if(cline.startsWith("windowposition=bottomleft"))
                schdle[6] = "bottomleft";
            else if(cline.startsWith("windowposition=bottomright"))
                schdle[6] = "bottomright";
        }

        file.close();
    }

    if(sdir[0].isEmpty())
    {
        sdir[0] = "/home";
        cfgupdt = true;
        if(! isdir("/home/Systemback")) QDir().mkdir("/home/Systemback");
        if(! isfile("/home/Systemback/.sbschedule")) crtfile("/home/Systemback/.sbschedule", NULL);
    }

    if(sdir[2].isEmpty())
    {
        sdir[2] = "/home";
        if(! cfgupdt) cfgupdt = true;
    }

    if(schdle[0].isEmpty())
    {
        schdle[0] = "off";
        if(! cfgupdt) cfgupdt = true;
    }

    if(schdle[1].isEmpty() || schdle[2].isEmpty() || schdle[3].isEmpty() || schdle[4].isEmpty())
    {
        schdle[1] = "1";
        schdle[2] = schdle[3] = "0";
        schdle[4] = "10";
        if(! cfgupdt) cfgupdt = true;
    }
    else if(schdle[1].toShort() > 7 || schdle[2].toShort() > 23 || schdle[3].toShort() > 59 || schdle[4].toShort() < 10 || schdle[4].toShort() > 99)
    {
        schdle[1] = "1";
        schdle[2] = schdle[3] = "0";
        schdle[4] = "10";
        if(! cfgupdt) cfgupdt = true;
    }
    else if(schdle[1].toShort() == 0 && schdle[2].toShort() == 0 && schdle[3].toShort()< 30)
    {
        schdle[3] = "30";
        if(! cfgupdt) cfgupdt = true;
    }

    if(schdle[5].isEmpty())
    {
        schdle[5] = "off";
        if(! cfgupdt) cfgupdt = true;
    }

    if(schdle[6].isEmpty())
    {
        schdle[6] = "topright";
        if(! cfgupdt) cfgupdt = true;
    }

    if(pnumber == 0)
    {
        pnumber = 5;
        if(! cfgupdt) cfgupdt = true;
    }

    sdir[1] = sdir[0] % "/Systemback";
    if(cfgupdt) cfgwrite();
}

void sb::cfgwrite()
{
    crtfile("/etc/systemback.conf", "storagedir=" % sdir[0] % "\nliveworkdir=" % sdir[2] % "\npointsnumber=" % QStr::number(pnumber) % "\ntimer=" % schdle[0] % "\nschedule=" % schdle[1] % ":" % schdle[2] % ":" % schdle[3] % ":" % schdle[4] % "\nsilentmode=" % schdle[5] % "\nwindowposition=" % schdle[6] % '\n');
}

bool sb::cpertime(QStr sourcepath, QStr destpath)
{
    struct stat64 sistat;
    if(stat64(sourcepath.toStdString().c_str(), &sistat) == -1) return false;
    struct stat64 distat;
    if(stat64(destpath.toStdString().c_str(), &distat) == -1) return false;

    if(sistat.st_uid != distat.st_uid || sistat.st_gid != distat.st_gid)
        if(chown(destpath.toStdString().c_str(), sistat.st_uid, sistat.st_gid) == -1) return false;

    if(sistat.st_mode != distat.st_mode)
        if(chmod(destpath.toStdString().c_str(), sistat.st_mode) == -1) return false;

    if(sistat.st_atim.tv_sec != distat.st_atim.tv_sec || sistat.st_mtim.tv_sec != distat.st_mtim.tv_sec)
    {
        struct utimbuf sitimes;
        sitimes.actime = sistat.st_atim.tv_sec;
        sitimes.modtime = sistat.st_mtim.tv_sec;
        return (utime(destpath.toStdString().c_str(), &sitimes) == -1) ? false : true;
    }

    return true;
}

bool sb::cpfile(QStr sourcefile, QStr newfile)
{
    struct stat64 sfstat;
    if(stat64(sourcefile.toStdString().c_str(), &sfstat) == -1) return false;

    if(SBThrd.isRunning())
    {
        if(! QFile(sourcefile).copy(newfile)) return false;
    }
    else
    {
        SBThrd.ThrdType = Copy;
        SBThrd.ThrdStr[0] = sourcefile;
        SBThrd.ThrdStr[1] = newfile;
        SBThrd.start();
        SBThrd.thrdelay();
    }

    struct stat64 nfstat;
    if(stat64(newfile.toStdString().c_str(), &nfstat) == -1) return false;
    if(sfstat.st_size != nfstat.st_size) return false;

    if(sfstat.st_uid != nfstat.st_uid || sfstat.st_gid != nfstat.st_gid)
        if(chown(newfile.toStdString().c_str(), sfstat.st_uid, sfstat.st_gid) == -1) return false;

    if(sfstat.st_mode != nfstat.st_mode)
        if(chmod(newfile.toStdString().c_str(), sfstat.st_mode) == -1) return false;

    struct utimbuf sftimes;
    sftimes.actime = sfstat.st_atim.tv_sec;
    sftimes.modtime = sfstat.st_mtim.tv_sec;
    return (utime(newfile.toStdString().c_str(), &sftimes) == -1) ? false : true;
}

bool sb::lcomp(QStr link1, QStr link2)
{
    if(rlink(link1) != rlink(link2)) return false;
    struct stat64 l1stat, l2stat;;
    if(lstat64(link1.toStdString().c_str(), &l1stat) == -1) return false;
    if(lstat64(link2.toStdString().c_str(), &l2stat) == -1) return false;
    if(l1stat.st_mtim.tv_sec != l2stat.st_mtim.tv_sec) return false;
    return true;
}

bool sb::cplink(QStr sourcelink, QStr newlink)
{
    if(! QFile::link(rlink(sourcelink), newlink)) return false;
    struct stat64 sistat;
    if(lstat64(sourcelink.toStdString().c_str(), &sistat) == -1) return false;
    struct timeval sitimes[2];
    sitimes[0].tv_sec = sistat.st_atim.tv_sec;
    sitimes[1].tv_sec = sistat.st_mtim.tv_sec;
    sitimes[0].tv_usec = sitimes[1].tv_usec = 0;
    return (lutimes(newlink.toStdString().c_str(), sitimes) == -1) ? false : true;
}

bool sb::cpdir(QStr sourcedir, QStr newdir)
{
    struct stat64 sdstat;
    if(stat64(sourcedir.toStdString().c_str(), &sdstat) == -1) return false;
    if(mkdir(newdir.toStdString().c_str(), sdstat.st_mode) == -1) return false;
    struct stat64 ndstat;
    if(stat64(newdir.toStdString().c_str(), &ndstat) == -1) return false;

    if(sdstat.st_uid != ndstat.st_uid || sdstat.st_gid != ndstat.st_gid)
        if(chown(newdir.toStdString().c_str(), sdstat.st_uid, sdstat.st_gid) == -1) return false;

    if(sdstat.st_mode != ndstat.st_mode)
        if(chmod(newdir.toStdString().c_str(), sdstat.st_mode) == -1) return false;

    struct utimbuf sdtimes;
    sdtimes.actime = sdstat.st_atim.tv_sec;
    sdtimes.modtime = sdstat.st_mtim.tv_sec;
    return (utime(newdir.toStdString().c_str(), &sdtimes) == -1) ? false : true;
}

void sb::fssync()
{
    SBThrd.ThrdType = Sync;
    SBThrd.start();
    SBThrd.thrdelay();
}

quint64 sb::devsize(QStr device)
{
    quint64 bsize;
    int dev(open(device.toStdString().c_str(), O_RDONLY));
    ioctl(dev, BLKGETSIZE64, &bsize);
    close(dev);
    return bsize;
}

bool sb::mcheck(QStr item)
{
    QStr mnts(fload("/proc/self/mounts"));
    if(item.contains(" ")) item = replace(item, " ", "\\040");

    if(item.startsWith("/dev/"))
        if(item.length() > 8)
        {
            if(QStr('\n' % mnts).contains('\n' % item % ' '))
                return true;
            else
            {
                blkid_probe pr(blkid_new_probe_from_filename(item.toStdString().c_str()));
                blkid_do_probe(pr);
                const char *uuid;
                bool ismtd(false);

                if(blkid_probe_lookup_value(pr, "UUID", &uuid, NULL) != -1)
                    if(! QStr(uuid).isEmpty())
                        if(QStr('\n' % mnts).contains("\n/dev/disk/by-uuid/" % QStr(uuid) % ' ')) ismtd = true;

                blkid_free_probe(pr);
                return ismtd;
            }
        }
        else
            return (QStr('\n' % mnts).contains('\n' % item)) ? true : false;
    else if(item.endsWith('/') && item.length() > 1)
        return (mnts.contains(' ' % left(item, -1))) ? true : false;
    else
        return (mnts.contains(' ' % item % ' ')) ? true : false;
}

bool sb::mount(QStr device, QStr mpoint, QStr moptions)
{
    SBThrd.ThrdType = Mount;
    SBThrd.ThrdStr[0] = device;
    SBThrd.ThrdStr[1] = mpoint;
    SBThrd.ThrdStr[2] = moptions;
    SBThrd.start();
    SBThrd.thrdelay();
    return SBThrd.ThrdRslt ? true : false;
}

bool sb::umount(QStr device)
{
    SBThrd.ThrdType = Umount;
    SBThrd.ThrdStr[0] = device;
    SBThrd.start();
    SBThrd.thrdelay();
    return SBThrd.ThrdRslt ? true : false;
}

uchar sb::fcomp(QStr file1, QStr file2)
{
    struct stat64 f1stat, f2stat;
    if(stat64(file1.toStdString().c_str(), &f1stat) == -1) return false;
    if(stat64(file2.toStdString().c_str(), &f2stat) == -1) return false;
    if(f1stat.st_size == f2stat.st_size && f1stat.st_mtim.tv_sec == f2stat.st_mtim.tv_sec) return (f1stat.st_mode == f2stat.st_mode && f1stat.st_uid == f2stat.st_uid && f1stat.st_gid == f2stat.st_gid) ? 2 : 1;
    return 0;
}

bool sb::issmfs(QString item1, QString item2)
{
    struct stat64 i1stat, i2stat;
    if(stat64(item1.toStdString().c_str(), &i1stat) == -1) return false;
    if(stat64(item2.toStdString().c_str(), &i2stat) == -1) return false;
    return (i1stat.st_dev == i2stat.st_dev) ? true : false;
}

uchar sb::exec(QStr cmd, QStr envv, bool silent, bool bckgrnd)
{
    if(ExecKill) ExecKill = false;
    uchar rprcnt(0);

    if(cmd.startsWith("mksquashfs"))
        rprcnt = 1;
    else if(cmd.startsWith("genisoimage"))
        rprcnt = 2;
    else if(cmd.startsWith("tar -cf"))
        rprcnt = 3;
    else if(cmd.startsWith("tar -xf") && cmd.endsWith("--no-same-permissions"))
        rprcnt = 4;

    if(rprcnt > 0) SBThrd.Progress = 0;
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
    quint64 inum(0);
    uchar cperc;

    while(proc.state() == QProcess::Running)
    {
        msleep(10);
        qApp->processEvents();
        if(ExecKill) proc.kill();

        switch(rprcnt) {
        case 1:
            inum += proc.readAllStandardOutput().count('\n');
            cperc = (inum * 100 + 50) / SBThrd.ThrdLng;
            if(SBThrd.Progress < cperc) SBThrd.Progress = cperc;
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
            cperc = mid(pout, rinstr(pout, "%") - 5, 2).toShort();
            if(SBThrd.Progress < cperc) SBThrd.Progress = cperc;
            break;
        }
        case 3:
            if(SBThrd.ThrdLng == 0)
            {
                QStr itms(SBThrd.rodir(sb::sdir[2] % "/.sblivesystemcreate"));
                QTS in(&itms, QIODevice::ReadOnly);

                while(! in.atEnd())
                {
                    QStr line(in.readLine()), item(right(line, - instr(line, "_")));

                    if(left(line, instr(line, "_") - 1).toShort() == Isfile)
                        SBThrd.ThrdLng += fsize(sb::sdir[2] % "/.sblivesystemcreate/" % item);
                }
            }
            else if(isfile(SBThrd.ThrdStr[0]))
            {
                cperc = (fsize(SBThrd.ThrdStr[0]) * 100 + 50) / SBThrd.ThrdLng;
                if(SBThrd.Progress < cperc) SBThrd.Progress = cperc;
            }

            break;
        case 4:
            QStr itms(SBThrd.rodir(SBThrd.ThrdStr[0]));
            QTS in(&itms, QIODevice::ReadOnly);
            quint64 size(0);

            while(! in.atEnd())
            {
                QStr line(in.readLine()), item(right(line, - instr(line, "_")));

                if(left(line, instr(line, "_") - 1).toShort() == Isfile)
                    size += fsize(SBThrd.ThrdStr[0] % '/' % item);
            }

            cperc = (size * 100 + 50) / SBThrd.ThrdLng;
            if(SBThrd.Progress < cperc) SBThrd.Progress = cperc;
        }
    }

    return (proc.exitStatus() == QProcess::CrashExit) ? 255 : proc.exitCode();
}

bool sb::exclcheck(QSL elist, QStr item)
{
    for(ushort b(0) ; b < elist.count() ; ++b)
    {
        QStr excl(elist.at(b));

        if(excl.endsWith('/'))
        {
            if(item.startsWith(excl)) return true;
        }
        else if(excl.endsWith('*'))
        {
            if(item.startsWith(left(excl, -1))) return true;
        }
        else if(like(item, QSL() << '_' % excl % '_' << '_' % excl % "/*"))
            return true;
    }

    return false;
}

void sb::pupgrade()
{
    bool rerun(true);

    while(rerun)
    {
        rerun = false;
        if(! pnames[0].isEmpty()) pnames[0].clear();
        if(! pnames[1].isEmpty()) pnames[1].clear();
        if(! pnames[2].isEmpty()) pnames[2].clear();
        if(! pnames[3].isEmpty()) pnames[3].clear();
        if(! pnames[4].isEmpty()) pnames[4].clear();
        if(! pnames[5].isEmpty()) pnames[5].clear();
        if(! pnames[6].isEmpty()) pnames[6].clear();
        if(! pnames[7].isEmpty()) pnames[7].clear();
        if(! pnames[8].isEmpty()) pnames[8].clear();
        if(! pnames[9].isEmpty()) pnames[9].clear();
        if(! pnames[10].isEmpty()) pnames[10].clear();
        if(! pnames[11].isEmpty()) pnames[11].clear();
        if(! pnames[12].isEmpty()) pnames[12].clear();
        if(! pnames[13].isEmpty()) pnames[13].clear();
        if(! pnames[14].isEmpty()) pnames[14].clear();

        if(isdir(sdir[1]) && access(sdir[1], Write))
        {
            QSL dlst(QDir(sdir[1]).entryList(QDir::Dirs | QDir::NoDotAndDotDot));

            for(uchar a(0) ; a < dlst.count() ; ++a)
            {
                QStr iname(dlst.at(a));

                if(! islink(sdir[1] % '/' % iname) && ! iname.contains(" "))
                {
                    if(left(iname, 4) == "S01_")
                        pnames[0] = right(iname, -4);
                    else if(left(iname, 4) == "S02_")
                        pnames[1] = right(iname, -4);
                    else if(left(iname, 4) == "S03_")
                        pnames[2] = right(iname, -4);
                    else if(left(iname, 4) == "S04_")
                        pnames[3] = right(iname, -4);
                    else if(left(iname, 4) == "S05_")
                        pnames[4] = right(iname, -4);
                    else if(left(iname, 4) == "S06_")
                        pnames[5] = right(iname, -4);
                    else if(left(iname, 4) == "S07_")
                        pnames[6] = right(iname, -4);
                    else if(left(iname, 4) == "S08_")
                        pnames[7] = right(iname, -4);
                    else if(left(iname, 4) == "S09_")
                        pnames[8] = right(iname, -4);
                    else if(left(iname, 4) == "S10_")
                        pnames[9] = right(iname, -4);
                    else if(left(iname, 4) == "H01_")
                        pnames[10] = right(iname, -4);
                    else if(left(iname, 4) == "H02_")
                        pnames[11] = right(iname, -4);
                    else if(left(iname, 4) == "H03_")
                        pnames[12] = right(iname, -4);
                    else if(left(iname, 4) == "H04_")
                        pnames[13] = right(iname, -4);
                    else if(left(iname, 4) == "H05_")
                        pnames[14] = right(iname, -4);
                }
            }
        }

        if(! pnames[14].isEmpty() && pnames[13].isEmpty())
        {
            QFile::rename(sdir[1] % "/H05_" % pnames[14], sdir[1] % "/H04_" % pnames[14]);
            rerun = true;
        }

        if(! pnames[13].isEmpty() && pnames[12].isEmpty())
        {
            QFile::rename(sdir[1] % "/H04_" % pnames[13], sdir[1] % "/H03_" % pnames[13]);
            if(! rerun) rerun = true;
        }

        if(! pnames[12].isEmpty() && pnames[11].isEmpty())
        {
            QFile::rename(sdir[1] % "/H03_" % pnames[12], sdir[1] % "/H02_" % pnames[12]);
            if(! rerun) rerun = true;
        }

        if(! pnames[11].isEmpty() && pnames[10].isEmpty())
        {
            QFile::rename(sdir[1] % "/H02_" % pnames[11], sdir[1] % "/H01_" % pnames[11]);
            if(! rerun) rerun = true;
        }

        if(! pnames[9].isEmpty() && pnames[8].isEmpty())
        {
            QFile::rename(sdir[1] % "/S10_" % pnames[9], sdir[1] % "/S09_" % pnames[9]);
            if(! rerun) rerun = true;
        }

        if(! pnames[8].isEmpty() && pnames[7].isEmpty())
        {
            QFile::rename(sdir[1] % "/S09_" % pnames[8], sdir[1] % "/S08_" % pnames[8]);
            if(! rerun) rerun = true;
        }

        if(! pnames[7].isEmpty() && pnames[6].isEmpty())
        {
            QFile::rename(sdir[1] % "/S08_" % pnames[7], sdir[1] % "/S07_" % pnames[7]);
            if(! rerun) rerun = true;
        }

        if(! pnames[6].isEmpty() && pnames[5].isEmpty())
        {
            QFile::rename(sdir[1] % "/S07_" % pnames[6], sdir[1] % "/S06_" % pnames[6]);
            if(! rerun) rerun = true;
        }

        if(! pnames[5].isEmpty() && pnames[4].isEmpty())
        {
            QFile::rename(sdir[1] % "/S06_" % pnames[5], sdir[1] % "/S05_" % pnames[5]);
            if(! rerun) rerun = true;
        }

        if(! pnames[4].isEmpty() && pnames[3].isEmpty())
        {
            QFile::rename(sdir[1] % "/S05_" % pnames[4], sdir[1] % "/S04_" % pnames[4]);
            if(! rerun) rerun = true;
        }

        if(! pnames[3].isEmpty() && pnames[2].isEmpty())
        {
            QFile::rename(sdir[1] % "/S04_" % pnames[3], sdir[1] % "/S03_" % pnames[3]);
            if(! rerun) rerun = true;
        }

        if(! pnames[2].isEmpty() && pnames[1].isEmpty())
        {
            QFile::rename(sdir[1] % "/S03_" % pnames[2], sdir[1] % "/S02_" % pnames[2]);
            if(! rerun) rerun = true;
        }

        if(! pnames[1].isEmpty() && pnames[0].isEmpty())
        {
            QFile::rename(sdir[1] % "/S02_" % pnames[1], sdir[1] % "/S01_" % pnames[1]);
            if(! rerun) rerun = true;
        }
    }
}

void sb::supgrade()
{
    exec("apt-get update");

    while(true)
    {
        if(exec("apt-get install -fym --force-yes") == 0 && exec("dpkg --configure -a") == 0 && exec("apt-get dist-upgrade --no-install-recommends -ym --force-yes") == 0 && exec("apt-get autoremove --purge -y") == 0)
        {
            QSL dlst(QDir("/boot").entryList(QDir::Files));
            QStr rklist;

            for(short a(dlst.count() - 1) ; a > -1 ; --a)
            {
                QStr item(dlst.at(a));

                if(item.startsWith("vmlinuz-"))
                {
                    QStr vmlinuz(right(item, -8)), kernel(left(vmlinuz, instr(vmlinuz, "-") - 1)), kver(mid(vmlinuz, kernel.length() + 2, instr(vmlinuz, "-", kernel.length() + 2) - kernel.length() - 2));
                    bool ok;
                    kver.toShort(&ok);

                    if(ok && vmlinuz.startsWith(kernel % '-' % kver % '-'))
                    {
                        if(! rklist.contains(kernel % '-' % kver % "-*"))
                        {
                            for(ushort b(1) ; b < 101 ; ++b)
                            {
                                QStr subk(kernel % '-' % QStr::number(kver.toShort() - b));

                                for(short c(dlst.count() - 1) ; c > -1 ; --c)
                                    if(dlst.at(c).startsWith("vmlinuz-" % subk % '-')) rklist.append(' ' % subk % "-*");
                            }
                        }
                    }
                }
            }

            uchar cproc(0);
            if(! rklist.isEmpty()) cproc = exec("apt-get autoremove --purge " % rklist);

            if(ilike(cproc, QSIL() << 0 << 1))
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

                if(! iplist.isEmpty()) exec("bash -c dpkg --purge " % iplist);
                exec("apt-get clean");
                QSL dlst(QDir("/var/cache/apt").entryList(QDir::Files));

                for(uchar a(0) ; a < dlst.count() ; ++a)
                {
                    QStr item(dlst.at(a));
                    if(item.contains(".bin.")) QFile::remove("/var/cache/apt/" % item);
                }

                dlst = QDir("/lib/modules").entryList(QDir::Dirs | QDir::NoDotAndDotDot);

                for(uchar a(0) ; a < dlst.count() ; ++a)
                {
                    QStr item(dlst.at(a));
                    if(! exist("/boot/vmlinuz-" % item)) QDir("/lib/modules/" % item).removeRecursively();
                }

                break;
            }
        }
        else
            exec("dpkg --configure -a");

        exec("tput reset");
        exec("tput civis");

        for(uchar a(3) ; a > 0 ; --a)
        {
            error("\n " % trn[0] % '\n');
            print("\n " % trn[1] % ' ' % QStr::number(a));
            sleep(1);
            exec("tput cup 0 0");
        }

        exec("tput reset");
    }
}

void sb::thrdelay()
{
    while(SBThrd.isRunning())
    {
        msleep(10);
        qApp->processEvents();
    }
}

bool sb::pisrng(QString pname)
{
    DIR *dir(opendir("/proc"));
    struct dirent *ent;
    bool rnng(false);

    while((ent = readdir(dir)) != NULL)
    {
        if(! like(ent->d_name, QSL() << "_._" << "_.._") && ent->d_type == DT_DIR)
        {
            bool ok;
            QStr(ent->d_name).toShort(&ok);

            if(ok && islink("/proc/" % QStr(ent->d_name) % "/exe") && QFile::readLink("/proc/" % QStr(ent->d_name) % "/exe").endsWith('/' % pname))
            {
                rnng = true;
                break;
            }
        }
    }

    closedir(dir);
    return rnng;
}

bool sb::crtrpoint(QStr sdir, QStr pname)
{
    SBThrd.ThrdType = Crtrpoint;
    SBThrd.ThrdStr[0] = sdir;
    SBThrd.ThrdStr[1] = pname;
    SBThrd.start();
    SBThrd.thrdelay();
    return SBThrd.ThrdRslt ? true : false;
}

bool sb::srestore(uchar mthd, QStr usr, QStr srcdir, QStr trgt, bool sfstab)
{
    SBThrd.ThrdType = Srestore;
    SBThrd.ThrdChr = mthd;
    SBThrd.ThrdStr[0] = usr;
    SBThrd.ThrdStr[1] = srcdir;
    SBThrd.ThrdStr[2] = trgt;
    SBThrd.ThrdBool = sfstab;
    SBThrd.start();
    SBThrd.thrdelay();
    return SBThrd.ThrdRslt ? true : false;
}

bool sb::scopy(uchar mthd, QStr usr, QStr srcdir)
{
    SBThrd.ThrdType = Scopy;
    SBThrd.ThrdChr = mthd;
    SBThrd.ThrdStr[0] = usr;
    SBThrd.ThrdStr[1] = srcdir;
    SBThrd.start();
    SBThrd.thrdelay();
    return SBThrd.ThrdRslt ? true : false;
}

bool sb::lvprpr(bool iudata)
{
    SBThrd.ThrdType = Lvprpr;
    SBThrd.ThrdBool = iudata;
    SBThrd.start();
    SBThrd.thrdelay();
    return SBThrd.ThrdRslt ? true : false;
}

void sb::run()
{
    if(SBThrd.ThrdKill) SBThrd.ThrdKill = false;
    if(! SBThrd.ThrdDbg.isEmpty()) SBThrd.ThrdDbg.clear();

    switch(ThrdType) {
    case Remove:
        switch(stype(ThrdStr[0])) {
        case Isdir:
            ThrdRslt = recrmdir(ThrdStr[0]) ? true : false;
            break;
        default:
            ThrdRslt = QFile::remove(ThrdStr[0]) ? true : false;
        }

        break;
    case Copy:
        QFile(ThrdStr[0]).copy(ThrdStr[1]);
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

        ThrdRslt = (mnt_context_mount(mcxt) == 0) ? true : false;
        mnt_free_context(mcxt);
        break;
    }
    case Umount:
    {
        struct libmnt_context *ucxt(mnt_new_context());
        mnt_context_set_target(ucxt, ThrdStr[0].toStdString().c_str());
        mnt_context_enable_force(ucxt, true);
        mnt_context_enable_lazy(ucxt, true);
        ThrdRslt = (mnt_context_umount(ucxt) == 0) ? true : false;
        mnt_free_context(ucxt);
        break;
    }
    case Dinfo:
        ThrdLng = devsize(ThrdStr[0]);

        if(ThrdLng >= 1048576)
        {
            blkid_probe pr(blkid_new_probe_from_filename(ThrdStr[0].toStdString().c_str()));
            blkid_do_probe(pr);

            if(blkid_probe_lookup_value(pr, "UUID", &FSUUID, NULL) == -1)
            {
                if(! QStr(FSUUID).isEmpty()) FSUUID = "";
            }
            else if(blkid_probe_lookup_value(pr, "TYPE", &FSType, NULL) == -1 && QStr(FSType) != "?")
                FSType = "?";

            blkid_free_probe(pr);
        }

        break;
    case Dsize:
        ThrdLng = devsize(ThrdStr[0]);
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

bool sb::recrmdir(QStr path, bool slimit)
{
    if(ThrdKill) return false;
    DIR *dir(opendir(path.toStdString().c_str()));
    struct dirent *ent;

    while((ent = readdir(dir)) != NULL)
    {
        if(! like(ent->d_name, QSL() << "_._" << "_.._"))
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
                    if(slimit)
                        if(QFile(fpath).size() > 8000000) continue;
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
                if(slimit)
                    if(QFile(fpath).size() > 8000000) continue;
            default:
                QFile::remove(fpath);
            }
        }
    }

    closedir(dir);
    return QDir().rmdir(path) ? true : slimit ? true : false;
}

QStr sb::rodir(QStr path, bool hidden)
{
    sbdir(path, path.length(), hidden);
    QStr cdlst(odlst);
    odlst.clear();
    return cdlst;
}

void sb::sbdir(QStr path, uchar oplen, bool hidden)
{
    QStr prepath(odlst.isEmpty() ? NULL : right(path, - short((oplen == 1) ? 1 : oplen + 1)) + '/');
    DIR *dir(opendir(path.toStdString().c_str()));
    struct dirent *ent;

    while((ent = readdir(dir)) != NULL)
    {
        if(! like(ent->d_name, QSL() << "_._" << "_.._"))
        {
            if(! hidden || QStr(ent->d_name).startsWith('.'))
            {
                switch(ent->d_type) {
                case DT_LNK:
                    odlst.append(QStr::number(Islink) % '_' % prepath % QStr(ent->d_name) % '\n');
                    break;
                case DT_DIR:
                    odlst.append(QStr::number(Isdir) % '_' % prepath % QStr(ent->d_name) % '\n');
                    sbdir(path % '/' % QStr(ent->d_name), oplen, false);
                    break;
                case DT_REG:
                    odlst.append(QStr::number(Isfile) % '_' % prepath % QStr(ent->d_name) % '\n');
                    break;
                case DT_UNKNOWN:
                    QStr fpath(path % '/' % QStr(ent->d_name));

                    switch(stype(fpath)) {
                    case Islink:
                        odlst.append(QStr::number(Islink) % '_' % prepath % QStr(ent->d_name) % '\n');
                        break;
                    case Isdir:
                        odlst.append(QStr::number(Isdir) % '_' % prepath % QStr(ent->d_name) % '\n');
                        sbdir(fpath, oplen, false);
                        break;
                    case Isfile:
                        odlst.append(QStr::number(Isfile) % '_' % prepath % QStr(ent->d_name) % '\n');
                    }
                }
            }
            else
                continue;
        }

        if(ThrdKill) return;
    }

    closedir(dir);
}

QSL sb::odir(QStr path, bool hidden)
{
    QSL dlst;
    DIR *dir(opendir(path.toStdString().c_str()));
    struct dirent *ent;

    while((ent = readdir(dir)) != NULL)
    {
        if(! like(ent->d_name, QSL() << "_._" << "_.._"))
            if(! hidden || QStr(ent->d_name).startsWith('.')) dlst.append(ent->d_name);

        if(ThrdKill) break;
    }

    closedir(dir);
    return dlst;
}

bool sb::thrdcrtrpoint(QStr &sdir, QStr &pname)
{
    QStr home1itms, home2itms, home3itms, home4itms, home5itms, rootitms, binitms, bootitms, etcitms, libitms, lib32itms, lib64itms, optitms, sbinitms, selinuxitms, srvitms, usritms, varitms;
    if(isdir("/bin")) binitms = rodir("/bin");
    if(ThrdKill) return false;
    if(isdir("/boot")) bootitms = rodir("/boot");
    if(ThrdKill) return false;
    if(isdir("/etc")) etcitms = rodir("/etc");
    if(ThrdKill) return false;
    if(isdir("/lib")) libitms = rodir("/lib");
    if(ThrdKill) return false;
    if(isdir("/lib32")) lib32itms = rodir("/lib32");
    if(ThrdKill) return false;
    if(isdir("/lib64")) lib64itms = rodir("/lib64");
    if(ThrdKill) return false;
    if(isdir("/opt")) optitms = rodir("/opt");
    if(ThrdKill) return false;
    if(isdir("/sbin")) sbinitms = rodir("/sbin");
    if(ThrdKill) return false;
    if(isdir("/selinux")) selinuxitms = rodir("/selinux");
    if(ThrdKill) return false;
    if(isdir("/srv")) srvitms = rodir("/srv");
    if(ThrdKill) return false;
    if(isdir("/usr")) usritms = rodir("/usr");
    if(ThrdKill) return false;
    if(isdir("/var")) varitms = rodir("/var");
    if(ThrdKill) return false;
    if(isdir("/root")) rootitms = rodir("/root", true);
    if(ThrdKill) return false;
    QSL usrs;
    ulong anum(0);
    QFile file("/etc/passwd");
    file.open(QIODevice::ReadOnly);
    QTS in(&file);

    while(! in.atEnd())
    {
        QStr usr(in.readLine());

        if(usr.contains(":/home/"))
        {
            usr = left(usr, instr(usr, ":") -1);

            if(isdir("/home/" % usr))
            {
                usrs.append(usr);

                switch(usrs.count()) {
                case 1:
                    home1itms = rodir("/home/" % usr, true);
                    break;
                case 2:
                    home2itms = rodir("/home/" % usr, true);
                    break;
                case 3:
                    home3itms = rodir("/home/" % usr, true);
                    break;
                case 4:
                    home4itms = rodir("/home/" % usr, true);
                    break;
                case 5:
                    home5itms = rodir("/home/" % usr, true);
                    break;
                default:
                    anum += rodir("/home/" % usr, true).count('\n');
                }
            }
        }

        if(ThrdKill) return false;
    }

    file.close();
    anum += home1itms.count('\n') + home2itms.count('\n') + home3itms.count('\n') + home4itms.count('\n') + home5itms.count('\n') + rootitms.count('\n') + binitms.count('\n') + bootitms.count('\n') + etcitms.count('\n') + libitms.count('\n') + lib32itms.count('\n') + lib64itms.count('\n') + optitms.count('\n') + sbinitms.count('\n') + selinuxitms.count('\n') + srvitms.count('\n') + usritms.count('\n') + varitms.count('\n');
    Progress = 0;
    QStr trgt(sdir % '/' % pname);
    if(! QDir().mkdir(trgt)) return false;
    QSL elist(QSL() << ".sbuserdata" << ".cache/gvfs" << ".local/share/Trash/files/" << ".local/share/Trash/info/" << ".Xauthority" << ".ICEauthority");
    file.setFileName("/etc/systemback.excludes");
    file.open(QIODevice::ReadOnly);

    while(! in.atEnd())
    {
        QStr cline(in.readLine());
        if(cline.startsWith('.')) elist.append(cline);
        if(ThrdKill) return false;
    }

    file.close();
    QStr *cditms;
    ulong cnum(0);
    uchar cperc;
    QSL rplst, dlst(QDir(sdir).entryList(QDir::Dirs | QDir::NoDotAndDotDot));

    for(uchar a(0) ; a < dlst.count() ; ++a)
    {
        QStr item(dlst.at(a));
        if(like(item, QSL() << "_S01_*" << "_S02_*" << "_S03_*" << "_S04_*" << "_S05_*" << "_S06_*" << "_S07_*" << "_S08_*" << "_S09_*" << "_S10_*" << "_H01_*" << "_H02_*" << "_H03_*" << "_H04_*" << "_H05_*")) rplst.append(item);
        if(ThrdKill) return false;
    }

    dlst.clear();

    if(isdir("/home"))
    {
        if(! QDir().mkdir(trgt % "/home")) return false;

        for(uchar a(0) ; a < usrs.count() ; ++a)
        {
            QStr usr(usrs.at(a)), homeitms;
            if(! QDir().mkdir(trgt % "/home/" % usr)) return false;

            switch(a) {
            case 0:
                cditms = &home1itms;
                break;
            case 1:
                cditms = &home2itms;
                break;
            case 2:
                cditms = &home3itms;
                break;
            case 3:
                cditms = &home4itms;
                break;
            case 4:
                cditms = &home5itms;
                break;
            default:
                homeitms = rodir("/home/" % usr, true);
                cditms = &homeitms;
            }

            in.setString(cditms, QIODevice::ReadOnly);

            while(! in.atEnd())
            {
                QStr line(in.readLine()), item(right(line, - instr(line, "_")));
                ++cnum;
                cperc = (cnum * 100 + 50) / anum;
                if(Progress < cperc) Progress = cperc;
                ThrdDbg = "@/home/" % usr % '/' % item;

                if(! like(item, QSL() << "_lost+found_" << "_lost+found/*" << "*/lost+found_" << "*/lost+found/*" << "_Systemback_" << "_Systemback/*" << "*/Systemback_" << "*/Systemback/*" << "*~_" << "*~/*") && ! exclcheck(elist, item) && exist("/home/" % usr % '/' % item))
                {
                    switch(left(line, instr(line, "_") - 1).toShort()) {
                    case Islink:
                        if(! cplink("/home/" % usr % '/' % item, trgt % "/home/" % usr % '/' % item)) return false;
                        break;
                    case Isdir:
                        if(! QDir().mkdir(trgt % "/home/" % usr % '/' % item)) return false;
                        break;
                    case Isfile:
                        if(QFile("/home/" % usr % '/' % item).size() <= 8000000)
                        {
                            for(uchar b(0) ; b < rplst.count() ; ++b)
                            {
                                QStr cpname(rplst.at(b));

                                if(stype(sdir % '/' % cpname % "/home/" % usr % '/' % item) == Isfile)
                                {
                                    if(fcomp("/home/" % usr % '/' % item, sdir % '/' % cpname % "/home/" % usr % '/' % item) == 2)
                                    {
                                        if(link(QStr(sdir % '/' % cpname % "/home/" % usr % '/' % item).toStdString().c_str(), QStr(trgt % "/home/" % usr % '/' % item).toStdString().c_str()) == -1) return false;
                                        goto nitem_1;
                                    }
                                }
                            }

                            if(! cpfile("/home/" % usr % '/' % item, trgt % "/home/" % usr % '/' % item)) return false;
                        }
                    }
                }

            nitem_1:;
                if(ThrdKill) return false;
            }

            in.setString(cditms, QIODevice::ReadOnly);

            while(! in.atEnd())
            {
                QStr line(in.readLine()), item(right(line, - instr(line, "_")));
                ThrdDbg = "@/home/" % usr % '/' % item;

                if(left(line, instr(line, "_") - 1).toShort() == Isdir)
                    if(exist(trgt % "/home/" % usr % '/' % item))
                        if(! cpertime("/home/" % usr % '/' % item, trgt % "/home/" % usr % '/' % item)) return false;

                if(ThrdKill) return false;
            }

            if(! cpertime("/home/" % usr, trgt % "/home/" % usr)) return false;
            cditms->clear();
        }

        if(! cpertime("/home", trgt % "/home")) return false;
    }

    if(isdir("/root"))
    {
        if(! QDir().mkdir(trgt % "/root")) return false;
        in.setString(&rootitms, QIODevice::ReadOnly);

        while(! in.atEnd())
        {
            QStr line(in.readLine()), item(right(line, - instr(line, "_")));
            ++cnum;
            cperc = (cnum * 100 + 50) / anum;
            if(Progress < cperc) Progress = cperc;
            ThrdDbg = "@/root/" % item;

            if(! like(item, QSL() << "_lost+found_" << "_lost+found/*" << "*/lost+found_" << "*/lost+found/*" << "_Systemback_" << "_Systemback/*" << "*/Systemback_" << "*/Systemback/*" << "*~_" << "*~/*") && ! exclcheck(elist, item) && exist("/root/" % item))
            {
                switch(left(line, instr(line, "_") - 1).toShort()) {
                case Islink:
                    if(! cplink("/root/" % item, trgt % "/root/" % item)) return false;
                    break;
                case Isdir:
                    if(! QDir().mkdir(trgt % "/root/" % item)) return false;
                    break;
                case Isfile:
                    if(QFile("/root/" % item).size() <= 8000000)
                    {
                        for(uchar b(0) ; b < rplst.count() ; ++b)
                        {
                            QStr cpname(rplst.at(b));

                            if(stype(sdir % '/' % cpname % "/root/" % item) == Isfile)
                            {
                                if(fcomp("/root/" % item, sdir % '/' % cpname % "/root/" % item) == 2)
                                {
                                    if(link(QStr(sdir % '/' % cpname % "/root/" % item).toStdString().c_str(), QStr(trgt % "/root/" % item).toStdString().c_str()) == -1) return false;
                                    goto nitem_2;
                                }
                            }
                        }

                        if(! cpfile("/root/" % item, trgt % "/root/" % item)) return false;
                    }
                }
            }

        nitem_2:;
            if(ThrdKill) return false;
        }

        in.setString(&rootitms, QIODevice::ReadOnly);

        while(! in.atEnd())
        {
            QStr line(in.readLine()), item(right(line, - instr(line, "_")));
            ThrdDbg = "@/root/" % item;

            if(left(line, instr(line, "_") - 1).toShort() == Isdir)
                if(exist(trgt % "/root/" % item))
                    if(! cpertime("/root/" % item, trgt % "/root/" % item)) return false;

            if(ThrdKill) return false;
        }

        rootitms.clear();
        if(! cpertime("/root", trgt % "/root")) return false;
    }

    dlst = QDir("/").entryList(QDir::Files);

    for(uchar a(0) ; a < dlst.count() ; ++a)
    {
        QStr item(dlst.at(a));

        if(like(item, QSL() << "_initrd.img*" << "_vmlinuz*"))
            if(islink('/' % item))
                if(! cplink('/' % item, trgt % '/' % item)) return false;

        if(ThrdKill) return false;
    }

    dlst = QSL() << "/cdrom" << "/dev" << "/mnt" << "/proc" << "/run" << "/sys" << "/tmp";

    for(uchar a(0) ; a < dlst.count() ; ++a)
    {
        QStr cdir(dlst.at(a));

        if(isdir(cdir))
            if(! cpdir(cdir, trgt % cdir)) return false;

        if(ThrdKill) return false;
    }

    elist = QSL() << "/etc/mtab" << "/var/.sblvtmp" << "/var/cache/fontconfig/" << "/var/lib/dpkg/lock" << "/var/lib/udisks/mtab" << "/var/lib/ureadahead/" << "/var/log/" << "/var/run/" << "/var/tmp/";
    dlst = QSL() << "/bin" << "/boot" << "/etc" << "/lib" << "/lib32" << "/lib64" << "/opt" << "/sbin" << "/selinux" << "/srv" << "/usr" << "/var";

    for(uchar a(0) ; a < dlst.count() ; ++a)
    {
        QStr cdir(dlst.at(a));

        if(isdir(cdir))
        {
            if(! QDir().mkdir(trgt % cdir)) return false;

            switch(a) {
            case 0:
                cditms = &binitms;
                break;
            case 1:
                cditms = &bootitms;
                break;
            case 2:
                cditms = &etcitms;
                break;
            case 3:
                cditms = &libitms;
                break;
            case 4:
                cditms = &lib32itms;
                break;
            case 5:
                cditms = &lib64itms;
                break;
            case 6:
                cditms = &optitms;
                break;
            case 7:
                cditms = &sbinitms;
                break;
            case 8:
                cditms = &selinuxitms;
                break;
            case 9:
                cditms = &srvitms;
                break;
            case 10:
                cditms = &usritms;
                break;
            default:
                cditms = &varitms;
            }

            in.setString(cditms, QIODevice::ReadOnly);

            while(! in.atEnd())
            {
                QStr line(in.readLine()), item(right(line, - instr(line, "_")));
                ++cnum;
                cperc = (cnum * 100 + 50) / anum;
                if(Progress < cperc) Progress = cperc;
                ThrdDbg = '@' % cdir % '/' % item;

                if(! (QStr(cdir % '/' % item).startsWith("/var/cache/apt/") && (item.endsWith(".bin") || item.contains(".bin."))) && ! (QStr(cdir % '/' % item).startsWith("/var/cache/apt/archives/") && item.endsWith(".deb")) && ! like(item, QSL() << "_lost+found_" << "_lost+found/*" << "*/lost+found_" << "*/lost+found/*" << "_Systemback_" << "_Systemback/*" << "*/Systemback_" << "*/Systemback/*" << "*.dpkg-old_" << "*~_" << "*~/*") && ! exclcheck(elist, QStr(cdir % '/' % item)))
                {
                    switch(left(line, instr(line, "_") - 1).toShort()) {
                    case Islink:
                        if(! cplink(cdir % '/' % item, trgt % cdir % '/' % item)) return false;
                        break;
                    case Isdir:
                        if(! QDir().mkdir(trgt % cdir % '/' % item)) return false;
                        break;
                    case Isfile:
                        for(uchar b(0) ; b < rplst.count() ; ++b)
                        {
                            QStr cpname(rplst.at(b));

                            if(stype(sdir % '/' % cpname % cdir % '/' % item) == Isfile)
                            {
                                if(fcomp(cdir % '/' % item, sdir % '/' % cpname % cdir % '/' % item) == 2)
                                {
                                    if(link(QStr(sdir % '/' % cpname % cdir % '/' % item).toStdString().c_str(), QStr(trgt % cdir % '/' % item).toStdString().c_str()) == -1) return false;
                                    goto nitem_3;
                                }
                            }
                        }

                        if(! cpfile(cdir % '/' % item, trgt % cdir % '/' % item)) return false;
                    }
                }

            nitem_3:;
                if(ThrdKill) return false;
            }

            in.setString(cditms, QIODevice::ReadOnly);

            while(! in.atEnd())
            {
                QStr line(in.readLine()), item(right(line, - instr(line, "_")));
                ThrdDbg = '@' % cdir % '/' % item;

                if(left(line, instr(line, "_") - 1).toShort() == Isdir)
                    if(exist(trgt % cdir % '/' % item))
                        if(! cpertime(cdir % '/' % item, trgt % cdir % '/' % item)) return false;

                if(ThrdKill) return false;
            }

            if(! cpertime(cdir, trgt % cdir)) return false;
            cditms->clear();
        }
    }

    if(isdir("/media"))
    {
        if(! QDir().mkdir(trgt % "/media")) return false;

        if(isfile("/etc/fstab"))
        {
            dlst = QDir("/media").entryList(QDir::Dirs | QDir::NoDotAndDotDot);
            file.setFileName("/etc/fstab");
            file.open(QIODevice::ReadOnly);
            in.setDevice(&file);

            for(uchar a(0) ; a < dlst.count() ; ++a)
            {
                QStr item(dlst.at(a));
                if(a > 0) file.open(QIODevice::ReadOnly);

                while(! in.atEnd())
                {
                    QStr cline(replace(in.readLine(), "\t", " "));

                    if(! cline.startsWith("#") && like(replace(cline, "\\040", " "), QSL() << "* /media/" % item % " *" << "* /media/" % item % "/*"))
                    {
                        QStr fdir;
                        QSL cdlst(QStr(mid(cline, instr(cline, "/media/") + 7, instr(cline, " ", instr(cline, "/media/")) - instr(cline, "/media/") - 7)).split('/'));

                        for(uchar b(0) ; b < cdlst.count() ; ++b)
                        {
                            QStr cdname(cdlst.at(b));

                            if(! cdname.isEmpty())
                            {
                                fdir.append('/' % replace(cdname, "\\040", " "));

                                if(! isdir(trgt % "/media" % fdir))
                                    if(! cpdir("/media" % fdir, trgt % "/media" % fdir)) return false;
                            }
                        }
                    }

                    if(ThrdKill) return false;
                }

                file.close();
            }
        }

        if(! cpertime("/media", trgt % "/media")) return false;
    }

    if(isdir("/var/log"))
    {
        QStr logitms(rodir("/var/log"));
        in.setString(&logitms, QIODevice::ReadOnly);

        while(! in.atEnd())
        {
            QStr line(in.readLine()), item(right(line, - instr(line, "_")));
            ThrdDbg = "@/var/log/" % item;

            switch(left(line, instr(line, "_") - 1).toShort()) {
            case Isdir:
                if(! cpdir("/var/log/" % item, trgt % "/var/log/" % item)) return false;
                break;
            case Isfile:
                if(! like(item, QSL() << "*.0_" << "*.1_" << "*.gz_" << "*.old_"))
                {
                    crtfile(trgt % "/var/log/" % item, NULL);
                    if(! cpertime("/var/log/" % item, trgt % "/var/log/" % item)) return false;
                }
            }

            if(ThrdKill) return false;
        }

        if(! cpertime("/var/log", trgt % "/var/log")) return false;
        if(! cpertime("/var", trgt % "/var")) return false;
    }

    ThrdDbg.clear();
    return true;
}

bool sb::fspchk(QString &dir)
{
    return (dfree(dir.isEmpty() ? "/" : dir) > 10485760) ? true : false;
}

bool sb::thrdsrestore(uchar &mthd, QStr &usr, QStr &srcdir, QStr &trgt, bool &sfstab)
{
    QSL usrs;
    QStr home1itms, home2itms, home3itms, home4itms, home5itms, rootitms, binitms, bootitms, etcitms, libitms, lib32itms, lib64itms, optitms, sbinitms, selinuxitms, srvitms, usritms, varitms;
    ulong anum(0);

    if(mthd != 2)
    {
        if(! ilike(mthd, QSIL() << 4 << 6))
        {
            if(isdir(srcdir % "/root")) rootitms = rodir(srcdir % "/root", true);

            if(isdir(srcdir % "/home"))
            {
                QSL dlst(QDir(srcdir % "/home").entryList(QDir::Dirs | QDir::NoDotAndDotDot));

                for(uchar a(0) ; a < dlst.count() ; ++a)
                {
                    QStr usr(dlst.at(a));
                    usrs.append(usr);

                    switch(usrs.count()) {
                    case 1:
                        home1itms = rodir(srcdir % "/home/" % usr, true);
                        break;
                    case 2:
                        home2itms = rodir(srcdir % "/home/" % usr, true);
                        break;
                    case 3:
                        home3itms = rodir(srcdir % "/home/" % usr, true);
                        break;
                    case 4:
                        home4itms = rodir(srcdir % "/home/" % usr, true);
                        break;
                    case 5:
                        home5itms = rodir(srcdir % "/home/" % usr, true);
                        break;
                    default:
                        anum += rodir(srcdir % "/home/" % usr, true).count('\n');
                    }

                    if(ThrdKill) return false;
                }
            }
        }
        else if(usr == "root")
        {
            if(isdir(srcdir % "/root")) rootitms = rodir(srcdir % "/root", true);
        }
        else if(isdir(srcdir % "/home/" % usr))
        {
            usrs.append(usr);
            home1itms = rodir(srcdir % "/home/" % usr, true);
        }

        anum += home1itms.count('\n') + home2itms.count('\n') + home3itms.count('\n') + home4itms.count('\n') + home5itms.count('\n') + rootitms.count('\n');
    }

    QStr *cditms;
    ulong cnum(0);
    uchar cperc;

    if(mthd < 3)
    {
        if(isdir(srcdir % "/bin")) binitms = rodir(srcdir % "/bin");
        if(ThrdKill) return false;
        if(isdir(srcdir % "/boot")) bootitms = rodir(srcdir % "/boot");
        if(ThrdKill) return false;
        if(isdir(srcdir % "/etc")) etcitms = rodir(srcdir % "/etc");
        if(ThrdKill) return false;
        if(isdir(srcdir % "/lib")) libitms = rodir(srcdir % "/lib");
        if(ThrdKill) return false;
        if(isdir(srcdir % "/lib32")) lib32itms = rodir(srcdir % "/lib32");
        if(ThrdKill) return false;
        if(isdir(srcdir % "/lib64")) lib64itms = rodir(srcdir % "/lib64");
        if(ThrdKill) return false;
        if(isdir(srcdir % "/opt")) optitms = rodir(srcdir % "/opt");
        if(ThrdKill) return false;
        if(isdir(srcdir % "/sbin")) sbinitms = rodir(srcdir % "/sbin");
        if(ThrdKill) return false;
        if(isdir(srcdir % "/selinux")) selinuxitms = rodir(srcdir % "/selinux");
        if(ThrdKill) return false;
        if(isdir(srcdir % "/srv")) srvitms = rodir(srcdir % "/srv");
        if(ThrdKill) return false;
        if(isdir(srcdir % "/usr")) usritms = rodir(srcdir % "/usr");
        if(ThrdKill) return false;
        if(isdir(srcdir % "/var")) varitms = rodir(srcdir % "/var");
        if(ThrdKill) return false;
        anum += binitms.count('\n') + bootitms.count('\n') + etcitms.count('\n') + libitms.count('\n') + lib32itms.count('\n') + lib64itms.count('\n') + optitms.count('\n') + sbinitms.count('\n') + selinuxitms.count('\n') + srvitms.count('\n') + usritms.count('\n') + varitms.count('\n');
        Progress = 0;
        QSL dlst(QDir(trgt.isEmpty() ? "/" : trgt).entryList(QDir::Files));

        for(uchar a(0) ; a < dlst.count() ; ++a)
        {
            QStr item(dlst.at(a));

            if(like(item, QSL() << "_initrd.img*" << "_vmlinuz*"))
            {
                if(islink(trgt % '/' % item))
                    if(exist(srcdir % '/' % item))
                        if(lcomp(trgt % '/' % item, srcdir % '/' % item)) continue;

                QFile::remove(trgt % '/' % item);
            }

            if(ThrdKill) return false;
        }

        dlst = QDir(srcdir).entryList(QDir::Files);

        for(uchar a(0) ; a < dlst.count() ; ++a)
        {
            QStr item(dlst.at(a));

            if(like(item, QSL() << "_initrd.img*" << "_vmlinuz*"))
                if(! exist(trgt % '/' % item)) cplink(srcdir % '/' % item, trgt % '/' % item);

            if(ThrdKill) return false;
        }

        dlst = QSL() << "/cdrom" << "/dev" << "/home" << "/mnt" << "/root" << "/proc" << "/run" << "/sys" << "/tmp";

        for(uchar a(0) ; a < dlst.count() ; ++a)
        {
            QStr cdir(dlst.at(a));

            if(isdir(srcdir % cdir))
            {
                if(isdir(trgt % cdir))
                    cpertime(srcdir % cdir, trgt % cdir);
                else
                {
                    if(exist(trgt % cdir)) QFile::remove(trgt % cdir);

                    if(! cpdir(srcdir % cdir, trgt % cdir))
                        if(! fspchk(trgt)) return false;
                }
            }
            else if(exist(trgt % cdir))
                stype(trgt % cdir) == Isdir ? recrmdir(trgt % cdir) : QFile::remove(trgt % cdir);

            if(ThrdKill) return false;
        }

        QSL elist;
        if(trgt.isEmpty()) elist = QSL() << "/etc/mtab" << "/var/cache/fontconfig/" << "/var/lib/dpkg/lock" << "/var/lib/udisks/mtab" << "/var/run/" << "/var/tmp/";
        if(trgt.isEmpty() || (isfile("/mnt/etc/sudoers.d/99_systemback") && isfile("/mnt/etc/sudoers.d/99_sbscheduler") && isfile("/mnt/etc/xdg/autostart/sbschedule.desktop") && isfile("/mnt/etc/xdg/autostart/sbschedule-kde.desktop") && isfile("/mnt/usr/bin/systemback") && isfile("/mnt/usr/lib/systemback/libsystemback.so.1.0.0") && isfile("/mnt/usr/lib/systemback/sbscheduler") && isfile("/mnt/usr/lib/systemback/sbsysupgrade")&& isdir("/mnt/usr/share/systemback/lang") && isfile("/mnt/usr/share/systemback/efi.tar.gz") && isfile("/mnt/usr/share/systemback/sbstart") && isfile("/mnt/usr/share/systemback/splash.png") && isfile("/mnt/var/lib/dpkg/info/systemback.list") && isfile("/mnt/var/lib/dpkg/info/systemback.md5sums"))) elist.append(QSL() << "/etc/sudoers.d/99_sbscheduler" << "/etc/sudoers.d/99_systemback" << "/etc/systemback*" << "/etc/xdg/autostart/sbschedule*" << "/usr/bin/systemback*" << "/usr/lib/systemback/" << "/usr/share/systemback/" << "/var/lib/dpkg/info/systemback*");
        if(sfstab) elist.append("/etc/fstab");
        dlst = QSL() << "/bin" << "/boot" << "/etc" << "/lib" << "/lib32" << "/lib64" << "/opt" << "/sbin" << "/selinux" << "/srv" << "/usr" << "/var";

        for(uchar a(0) ; a < dlst.count() ; ++a)
        {
            QStr cdir(dlst.at(a));

            if(isdir(srcdir % cdir))
            {
                if(isdir(trgt % cdir))
                {
                    QSL sdlst(odir(trgt % cdir));

                    for(ushort b(0) ; b < sdlst.count() ; ++b)
                    {
                        QStr item(sdlst.at(b));

                        if(! like(item, QSL() << "_lost+found_" << "_Systemback_") && ! exclcheck(elist, QStr(cdir % '/' % item)))
                            if(! exist(srcdir % cdir % '/' % item)) (stype(trgt % cdir % '/' % item) == Isdir) ? recrmdir(trgt % cdir % '/' % item) : QFile::remove(trgt % cdir % '/' % item);

                        if(ThrdKill) return false;
                    }
                }
                else
                {
                    if(exist(trgt % cdir)) QFile::remove(trgt % cdir);

                    if(! QDir().mkdir(trgt % cdir))
                        if(! fspchk(trgt)) return false;
                }

                switch(a) {
                case 0:
                    cditms = &binitms;
                    break;
                case 1:
                    cditms = &bootitms;
                    break;
                case 2:
                    cditms = &etcitms;
                    break;
                case 3:
                    cditms = &libitms;
                    break;
                case 4:
                    cditms = &lib32itms;
                    break;
                case 5:
                    cditms = &lib64itms;
                    break;
                case 6:
                    cditms = &optitms;
                    break;
                case 7:
                    cditms = &sbinitms;
                    break;
                case 8:
                    cditms = &selinuxitms;
                    break;
                case 9:
                    cditms = &srvitms;
                    break;
                case 10:
                    cditms = &usritms;
                    break;
                default:
                    cditms = &varitms;
                }

                QTS in(cditms, QIODevice::ReadOnly);

                while(! in.atEnd())
                {
                    QStr line(in.readLine()), item(right(line, - instr(line, "_")));
                    ++cnum;
                    cperc = (cnum * 100 + 50) / anum;
                    if(Progress < cperc) Progress = cperc;

                    if(! like(item, QSL() << "_lost+found_" << "_lost+found/*" << "*/lost+found_" << "*/lost+found/*" << "_Systemback_" << "_Systemback/*" << "*/Systemback_" << "*/Systemback/*") && ! exclcheck(elist, QStr(cdir % '/' % item)))
                    {
                        switch(left(line, instr(line, "_") - 1).toShort()) {
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

                            if(! cplink(srcdir % cdir % '/' % item, trgt % cdir % '/' % item))
                                if(! fspchk(trgt)) return false;

                            break;
                        case Isdir:
                            switch(stype(trgt % cdir % '/' % item)) {
                            case Isdir:
                            {
                                QSL sdlst(odir(trgt % cdir % '/' % item));

                                for(ushort b(0) ; b < sdlst.count() ; ++b)
                                {
                                    QStr sitem(sdlst.at(b));

                                    if(! like(sitem, QSL() << "_lost+found_" << "_Systemback_") && ! exclcheck(elist, QStr(cdir % '/' % item % '/' % sitem)))
                                        if(! exist(srcdir % cdir % '/' % item % '/' % sitem)) (stype(trgt % cdir % '/' % item % '/' % sitem) == Isdir) ? recrmdir(trgt % cdir % '/' % item % '/' % sitem) : QFile::remove(trgt % cdir % '/' % item % '/' % sitem);

                                    if(ThrdKill) return false;
                                }

                                goto nitem_1;
                            }
                            case Islink:
                            case Isfile:
                                QFile::remove(trgt % cdir % '/' % item);
                            }

                            if(! QDir().mkdir(trgt % cdir % '/' % item))
                                if(! fspchk(trgt)) return false;

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

                            if(! cpfile(srcdir % cdir % '/' % item, trgt % cdir % '/' % item))
                                if(! fspchk(trgt)) return false;
                        }
                    }

                nitem_1:;
                    if(ThrdKill) return false;
                }

                in.setString(cditms, QIODevice::ReadOnly);

                while(! in.atEnd())
                {
                    QStr line(in.readLine()), item(right(line, - instr(line, "_")));

                    if(left(line, instr(line, "_") - 1).toShort() == Isdir)
                        if(exist(trgt % cdir % '/' % item))
                            cpertime(srcdir % cdir % '/' % item, trgt % cdir % '/' % item);

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
            {
                dlst = QDir(trgt % "/media").entryList(QDir::Dirs | QDir::NoDotAndDotDot);

                for(uchar a(0) ; a < dlst.count() ; ++a)
                {
                    QStr item(dlst.at(a));
                    if(! exist(srcdir % "/media/" % item) && ! mcheck(trgt % "/media/" % item % '/')) recrmdir(trgt % "/media/" % item);
                    if(ThrdKill) return false;
                }
            }
            else
            {
                if(exist(trgt % "/media")) QFile::remove(trgt % "/media");

                if(! QDir().mkdir(trgt % "/media"))
                    if(! fspchk(trgt)) return false;
            }

            QStr mediaitms(rodir(srcdir % "/media"));
            QTS in(&mediaitms, QIODevice::ReadOnly);

            while(! in.atEnd())
            {
                QStr line(in.readLine()), item(right(line, - instr(line, "_")));

                if(exist(trgt % "/media/" % item))
                {
                    if(stype(trgt % "/media/" % item) != Isdir)
                    {
                        QFile::remove(trgt % "/media/" % item);

                        if(! QDir().mkdir(trgt % "/media/" % item))
                            if(! fspchk(trgt)) return false;
                    }

                    cpertime(srcdir % "/media/" % item, trgt % "/media/" % item);
                }
                else
                    if(! cpdir(srcdir % "/media/" % item, trgt % "/media/" % item))
                        if(! fspchk(trgt)) return false;

                if(ThrdKill) return false;
            }

            cpertime(srcdir % "/media", trgt % "/media");
        }
        else if(exist(trgt % "/media"))
            stype(trgt % "/media") == Isdir ? recrmdir(trgt % "/media") : QFile::remove(trgt % "/media");

        if(srcdir == "/.systembacklivepoint" && isdir("/.systembacklivepoint/.systemback"))
        {
            QStr sbitms(rodir(srcdir % "/.systemback"));
            QTS in(&sbitms, QIODevice::ReadOnly);

            while(! in.atEnd())
            {
                QStr line(in.readLine()), item(right(line, - instr(line, "_")));

                switch(left(line, instr(line, "_") - 1).toShort()) {
                case Islink:
                    if(item != "etc/fstab" || ! sfstab)
                        if(! cplink(srcdir % "/.systemback/" % item, trgt % '/' % item))
                            if(! fspchk(trgt)) return false;

                    break;
                case Isfile:
                    if(item != "etc/fstab" || ! sfstab)
                        if(! cpfile(srcdir % "/.systemback/" % item, trgt % '/' % item))
                            if(! fspchk(trgt)) return false;
                }

                if(ThrdKill) return false;
            }

            in.setString(&sbitms, QIODevice::ReadOnly);

            while(! in.atEnd())
            {
                QStr line(in.readLine()), item(right(line, - instr(line, "_")));
                if(left(line, instr(line, "_") - 1).toShort() == Isdir) cpertime(srcdir % "/.systemback/" % item, trgt % '/' % item);
                if(ThrdKill) return false;
            }
        }
    }
    else
        Progress = 0;

    if(mthd != 2)
    {
        QSL elist(QSL() << ".cache/gvfs" << ".gvfs" << ".local/share/Trash/files/" << ".local/share/Trash/info/" << ".Xauthority" << ".ICEauthority");
        QFile file(srcdir % "/etc/systemback.excludes");
        file.open(QIODevice::ReadOnly);
        QTS in(&file);

        while(! in.atEnd())
        {
            QStr cline(in.readLine());
            if(cline.startsWith('.')) elist.append(cline);
            if(ThrdKill) return false;
        }

        file.close();
        bool skppd;

        if(! ilike(mthd, QSIL() << 4 << 6) || usr == "root")
        {
            if(! ilike(mthd, QSIL() << 3 << 4))
            {
                QSL sdlst(odir(trgt % "/root", true));

                for(ushort a(0) ; a < sdlst.count() ; ++a)
                {
                    QStr item(sdlst.at(a));

                    if(! item.endsWith('~') && ! exclcheck(elist, item))
                    {
                        if(! exist(srcdir % "/root/" % item))
                        {
                            switch(stype(trgt % "/root/" % item)) {
                            case Isdir:
                                recrmdir(trgt % "/root/" % item, true);
                                break;
                            case Isfile:
                                if(QFile(trgt % "/root/" % item).size() > 8000000) continue;
                            case Islink:
                                QFile::remove(trgt % "/root/" % item);
                            }
                        }
                    }

                    if(ThrdKill) return false;
                }
            }

            in.setString(&rootitms, QIODevice::ReadOnly);

            while(! in.atEnd())
            {
                QStr line(in.readLine()), item(right(line, - instr(line, "_")));
                ++cnum;
                cperc = (cnum * 100 + 50) / anum;
                if(Progress < cperc) Progress = cperc;

                if(! like(item, QSL() << "_lost+found_" << "_lost+found/*" << "*/lost+found_" << "*/lost+found/*" << "_Systemback_" << "_Systemback/*" << "*/Systemback_" << "*/Systemback/*" << "*~_" << "*~/*") && ! exclcheck(elist, item))
                {
                    switch(left(line, instr(line, "_") - 1).toShort()) {
                    case Islink:
                        switch(stype(trgt % "/root/" % item)) {
                        case Islink:
                            if(lcomp(trgt % "/root/" % item, srcdir % "/root/" % item)) goto nitem_2;
                        case Isfile:
                            QFile::remove(trgt % "/root/" % item);
                            break;
                        case Isdir:
                            recrmdir(trgt % "/root/" % item);
                        }

                        if(! cplink(srcdir % "/root/" % item, trgt % "/root/" % item))
                            if(! fspchk(trgt)) return false;

                        break;
                    case Isdir:
                        switch(stype(trgt % "/root/" % item)) {
                        case Isdir:
                        {
                            if(! ilike(mthd, QSIL() << 3 << 4))
                            {
                                QSL sdlst(odir(trgt % "/root/" % item));

                                for(ushort a(0) ; a < sdlst.count() ; ++a)
                                {
                                    QStr sitem(sdlst.at(a));

                                    if(! like(sitem, QSL() << "_lost+found_" << "_Systemback_" << "*~_") && ! exclcheck(elist, QStr(item % '/' % sitem)) && ! exist(srcdir % "/root/" % item % '/' % sitem))
                                    {
                                        switch(stype(trgt % "/root/" % item % '/' % sitem)) {
                                        case Isdir:
                                            recrmdir(trgt % "/root/" % item % '/' % sitem, true);
                                            break;
                                        case Isfile:
                                            if(QFile(trgt % "/root/" % item % '/' % sitem).size() > 8000000) continue;
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

                        if(! QDir().mkdir(trgt % "/root/" % item))
                            if(! fspchk(trgt)) return false;

                        break;
                    case Isfile:
                        skppd = (QFile(srcdir % "/root/" % item).size() >= 8000000) ? true : false;

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

                        if(! skppd)
                            if(! cpfile(srcdir % "/root/" % item, trgt % "/root/" % item))
                                if(! fspchk(trgt)) return false;
                    }
                }

            nitem_2:;
                if(ThrdKill) return false;
            }

            in.setString(&rootitms, QIODevice::ReadOnly);

            while(! in.atEnd())
            {
                QStr line(in.readLine()), item(right(line, - instr(line, "_")));

                if(left(line, instr(line, "_") - 1).toShort() == Isdir)
                    if(exist(trgt % "/root/" % item))
                        cpertime(srcdir % "/root/" % item, trgt % "/root/" % item);

                if(ThrdKill) return false;
            }

            rootitms.clear();
        }

        for(uchar a(0) ; a < usrs.count() ; ++a)
        {
            QStr usr(usrs.at(a));

            if(isdir(trgt % "/home/" % usr))
            {
                if(! ilike(mthd, QSIL() << 3 << 4))
                {
                    QSL sdlst(odir(trgt % "/home/" % usr, true));

                    for(ushort b(0) ; b < sdlst.count() ; ++b)
                    {
                        QStr item(sdlst.at(b));

                        if(! item.endsWith('~') && ! exclcheck(elist, item) && ! exist(srcdir % "/home/" % usr % '/' % item))
                        {
                            switch(stype(trgt % "/home/" % usr % '/' % item)) {
                            case Isdir:
                                recrmdir(trgt % "/home/" % usr % '/' % item, true);
                                break;
                            case Isfile:
                                if(QFile(trgt % "/home/" % usr % '/' % item).size() > 8000000) continue;
                            case Islink:
                                QFile::remove(trgt % "/home/" % usr % '/' % item);
                            }
                        }

                        if(ThrdKill) return false;
                    }
                }
            }
            else
            {
                if(exist(trgt % "/home/" % usr)) QFile::remove(trgt % "/home/" % usr);

                if(! QDir().mkdir(trgt % "/home/" % usr))
                    if(! fspchk(trgt)) return false;
            }

            QStr homeitms;

            switch(a) {
            case 0:
                cditms = &home1itms;
                break;
            case 1:
                cditms = &home2itms;
                break;
            case 2:
                cditms = &home3itms;
                break;
            case 3:
                cditms = &home4itms;
                break;
            case 4:
                cditms = &home5itms;
                break;
            default:
                homeitms = rodir(srcdir % "/home/" % usr, true);
                cditms = &homeitms;
            }

            in.setString(cditms, QIODevice::ReadOnly);

            while(! in.atEnd())
            {
                QStr line(in.readLine()), item(right(line, - instr(line, "_")));
                ++cnum;
                cperc = (cnum * 100 + 50) / anum;
                if(Progress < cperc) Progress = cperc;

                if(! like(item, QSL() << "_lost+found_" << "_lost+found/*" << "*/lost+found_" << "*/lost+found/*" << "_Systemback_" << "_Systemback/*" << "*/Systemback_" << "*/Systemback/*" << "*~_" << "*~/*") && ! exclcheck(elist, item))
                {
                    switch(left(line, instr(line, "_") - 1).toShort()) {
                    case Islink:
                        switch(stype(trgt % "/home/" % usr % '/' % item)) {
                        case Islink:
                            if(lcomp(trgt % "/home/" % usr % '/' % item, srcdir % "/home/" % usr % '/' % item)) goto nitem_3;
                        case Isfile:
                            QFile::remove(trgt % "/home/" % usr % '/' % item);
                            break;
                        case Isdir:
                            recrmdir(trgt % "/home/" % usr % '/' % item);
                        }

                        if(! cplink(srcdir % "/home/" % usr % '/' % item, trgt % "/home/" % usr % '/' % item))
                            if(! fspchk(trgt)) return false;

                        break;
                    case Isdir:
                        switch(stype(trgt % "/home/" % usr % '/' % item)) {
                        case Isdir:
                        {
                            if(! ilike(mthd, QSIL() << 3 << 4))
                            {
                                QSL sdlst(odir(trgt % "/home/" % usr % '/' % item));

                                for(ushort b(0) ; b < sdlst.count() ; ++b)
                                {
                                    QStr sitem(sdlst.at(b));

                                    if(! like(sitem, QSL() << "_lost+found_" << "_Systemback_" << "*~_") && ! exclcheck(elist, QStr(item % '/' % sitem)) && ! exist(srcdir % "/home/" % usr % '/' % item % '/' % sitem))
                                    {
                                        switch(stype(trgt % "/home/" % usr % '/' % item % '/' % sitem)) {
                                        case Isdir:
                                            recrmdir(trgt % "/home/" % usr % '/' % item % '/' % sitem, true);
                                            break;
                                        case Isfile:
                                            if(QFile(trgt % "/home/" % usr % '/' % item % '/' % sitem).size() > 8000000) continue;
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

                        if(! QDir().mkdir(trgt % "/home/" % usr % '/' % item))
                            if(! fspchk(trgt)) return false;

                        break;
                    case Isfile:
                        skppd = (QFile(srcdir % "/home/" % usr % '/' % item).size() >= 8000000) ? true : false;

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

                        if(! skppd)
                            if(! cpfile(srcdir % "/home/" % usr % '/' % item, trgt % "/home/" % usr % '/' % item))
                                if(! fspchk(trgt)) return false;
                    }
                }

            nitem_3:;
                if(ThrdKill) return false;
            }

            in.setString(cditms, QIODevice::ReadOnly);

            while(! in.atEnd())
            {
                QStr line(in.readLine()), item(right(line, - instr(line, "_")));

                if(left(line, instr(line, "_") - 1).toShort() == Isdir)
                    if(exist(trgt % "/home/" % usr % '/' % item))
                        cpertime(srcdir % "/home/" % usr % '/' % item, trgt % "/home/" % usr % '/' % item);

                if(ThrdKill) return false;
            }

            cditms->clear();
            cpertime(srcdir % "/home/" % usr, trgt % "/home/" % usr);
        }
    }

    return true;
}

bool sb::thrdscopy(uchar &mthd, QStr &usr, QStr &srcdir)
{
    QSL usrs;
    QStr home1itms, home2itms, home3itms, home4itms, home5itms, rootitms, binitms, bootitms, etcitms, libitms, lib32itms, lib64itms, optitms, sbinitms, selinuxitms, srvitms, usritms, varitms;
    ulong anum(0);
    if(isdir(srcdir % "/bin")) binitms = rodir(srcdir % "/bin");
    if(ThrdKill) return false;
    if(isdir(srcdir % "/boot")) bootitms = rodir(srcdir % "/boot");
    if(ThrdKill) return false;
    if(isdir(srcdir % "/etc")) etcitms = rodir(srcdir % "/etc");
    if(ThrdKill) return false;
    if(isdir(srcdir % "/lib")) libitms = rodir(srcdir % "/lib");
    if(ThrdKill) return false;
    if(isdir(srcdir % "/lib32")) lib32itms = rodir(srcdir % "/lib32");
    if(ThrdKill) return false;
    if(isdir(srcdir % "/lib64")) lib64itms = rodir(srcdir % "/lib64");
    if(ThrdKill) return false;
    if(isdir(srcdir % "/opt")) optitms = rodir(srcdir % "/opt");
    if(ThrdKill) return false;
    if(isdir(srcdir % "/sbin")) sbinitms = rodir(srcdir % "/sbin");
    if(ThrdKill) return false;
    if(isdir(srcdir % "/selinux")) selinuxitms = rodir(srcdir % "/selinux");
    if(ThrdKill) return false;
    if(isdir(srcdir % "/srv")) srvitms = rodir(srcdir % "/srv");
    if(ThrdKill) return false;
    if(isdir(srcdir % "/usr")) usritms = rodir(srcdir % "/usr");
    if(ThrdKill) return false;
    if(isdir(srcdir % "/var")) varitms = rodir(srcdir % "/var");
    if(ThrdKill) return false;
    if(isdir(srcdir % "/root")) rootitms = (mthd == 4) ? rodir(srcdir % "/etc/skel") : rodir(srcdir % "/root", true);
    if(ThrdKill) return false;
    anum += binitms.count('\n') + bootitms.count('\n') + etcitms.count('\n') + libitms.count('\n') + lib32itms.count('\n') + lib64itms.count('\n') + optitms.count('\n') + sbinitms.count('\n') + selinuxitms.count('\n') + srvitms.count('\n') + usritms.count('\n') + varitms.count('\n') + rootitms.count('\n');

    if(mthd != 0)
    {
        if(isdir(srcdir % "/home"))
        {
            if(ilike(mthd, QSIL() << 1 << 2))
            {
                if(srcdir.isEmpty())
                {
                    QFile file("/etc/passwd");
                    file.open(QIODevice::ReadOnly);
                    QTS in(&file);

                    while(! in.atEnd())
                    {
                        QStr usr(in.readLine());

                        if(usr.contains(":/home/"))
                        {
                            usr = left(usr, instr(usr, ":") -1);
                            if(isdir("/home/" % usr)) usrs.append(usr);
                        }
                    }

                    file.close();
                }
                else
                {
                    QSL dlst(QDir(srcdir % "/home").entryList(QDir::Dirs | QDir::NoDotAndDotDot));
                    for(uchar a(0) ; a < dlst.count() ; ++a) usrs.append(dlst.at(a));
                }
            }

            if(ThrdKill) return false;

            if(mthd == 1)
            {
                for(uchar a(0) ; a < usrs.count() ; ++a)
                {
                    QStr usr(usrs.at(a));

                    switch(a) {
                    case 0:
                        home1itms = rodir(srcdir % "/home/" % usr);
                        break;
                    case 1:
                        home2itms = rodir(srcdir % "/home/" % usr);
                        break;
                    case 2:
                        home3itms = rodir(srcdir % "/home/" % usr);
                        break;
                    case 3:
                        home4itms = rodir(srcdir % "/home/" % usr);
                        break;
                    case 4:
                        home5itms = rodir(srcdir % "/home/" % usr);
                        break;
                    default:
                        anum += rodir(srcdir % "/home/" % usr).count('\n');
                    }

                    if(ThrdKill) return false;
                }
            }
            else if(mthd == 2)
            {
                for(uchar a(0) ; a < usrs.count() ; ++a)
                {
                    QStr usr(usrs.at(a));

                    switch(a) {
                    case 0:
                        home1itms = rodir(srcdir % "/home/" % usr, true);
                        break;
                    case 1:
                        home2itms = rodir(srcdir % "/home/" % usr, true);
                        break;
                    case 2:
                        home3itms = rodir(srcdir % "/home/" % usr, true);
                        break;
                    case 3:
                        home4itms = rodir(srcdir % "/home/" % usr, true);
                        break;
                    case 4:
                        home5itms = rodir(srcdir % "/home/" % usr, true);
                        break;
                    default:
                        anum += rodir(srcdir % "/home/" % usr, true).count('\n');
                    }

                    if(ThrdKill) return false;
                }
            }
            else if(isdir(srcdir % "/home/" % usr))
            {
                usrs.append(usr);

                switch(mthd) {
                case 3:
                    home1itms = rodir(srcdir % "/home/" % usr, true);
                    break;
                case 4:
                    home1itms = rootitms;
                }
            }
        }

        anum += home1itms.count('\n') + home2itms.count('\n') + home3itms.count('\n') + home4itms.count('\n') + home5itms.count('\n');
    }

    Progress = 0;
    QStr *cditms, macid;
    ulong cnum(0);
    uchar cperc;
    QSL elist(QSL() << ".cache/gvfs" << ".gvfs" << ".local/share/Trash/files/" << ".local/share/Trash/info/" << ".Xauthority" << ".ICEauthority");
    QFile file(srcdir % "/etc/systemback.excludes");
    file.open(QIODevice::ReadOnly);
    QTS in(&file);

    while(! in.atEnd())
    {
        QStr cline(in.readLine());
        if(cline.startsWith('.')) elist.append(cline);
        if(ThrdKill) return false;
    }

    file.close();

    if(mthd > 2)
    {
        QStr mid;

        if(isfile(srcdir % "/var/lib/dbus/machine-id"))
            mid = "/var/lib/dbus/machine-id";
        else if(isfile(srcdir % "/etc/machine-id"))
            mid = "/etc/machine-id";

        if(! mid.isEmpty())
        {
            file.setFileName(srcdir % mid);
            file.open(QIODevice::ReadOnly);
            QStr out(in.readLine());
            if(out.length() == 32) macid = out;
            file.close();
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

        if(mthd != 0)
        {
            for(uchar a(0) ; a < usrs.count() ; ++a)
            {
                QStr usr(usrs.at(a));

                if(! isdir("/.sbsystemcopy/home/" % usr))
                {
                    if(exist("/.sbsystemcopy/home/" % usr)) QFile::remove("/.sbsystemcopy/home/" % usr);
                    if(! cpdir(srcdir % "/home/" % usr, "/.sbsystemcopy/home/" % usr)) return false;
                }

                QStr homeitms;

                switch(a) {
                case 0:
                    cditms = &home1itms;
                    break;
                case 1:
                    cditms = &home2itms;
                    break;
                case 2:
                    cditms = &home3itms;
                    break;
                case 3:
                    cditms = &home4itms;
                    break;
                case 4:
                    cditms = &home5itms;
                    break;
                default:
                    homeitms = (mthd == 1) ? rodir(srcdir % "/home/" % usr) : rodir(srcdir % "/home/" % usr, true);
                    cditms = &homeitms;
                }

                in.setString(cditms, QIODevice::ReadOnly);

                while(! in.atEnd())
                {
                    QStr line(in.readLine()), item(right(line, - instr(line, "_")));
                    ++cnum;
                    cperc = (cnum * 100 + 50) / anum;
                    if(Progress < cperc) Progress = cperc;
                    ThrdDbg = (mthd == 4) ? QStr("@/etc/skel/" % item) : QStr("@/home/" % usr % '/' % item);

                    if(mthd == 4 || (! like(item, QSL() << "_lost+found_" << "_lost+found/*" << "*/lost+found_" << "*/lost+found/*" << "_Systemback_" << "_Systemback/*" << "*/Systemback_" << "*/Systemback/*" << "*~_" << "*~/*") && ! exclcheck(elist, item) && (macid.isEmpty() || ! item.contains(macid))))
                    {
                        switch(left(line, instr(line, "_") - 1).toShort()) {
                        case Islink:
                            switch(stype("/.sbsystemcopy/home/" % usr % '/' % item)) {
                            case Islink:
                                if((mthd == 4) ? ! lcomp("/.sbsystemcopy/home/" % usr % '/' % item, srcdir % "/etc/skel/" % item) : ! lcomp("/.sbsystemcopy/home/" % usr % '/' % item, srcdir % "/home/" % usr % '/' % item)) goto nitem_1;
                            case Isfile:
                                QFile::remove("/.sbsystemcopy/home/" % usr % '/' % item);
                                break;
                            case Isdir:
                                recrmdir("/.sbsystemcopy/home/" % usr % '/' % item);
                            }

                            if((mthd == 4) ? cplink(srcdir % "/etc/skel/" % item, "/.sbsystemcopy/home/" % usr % '/' % item) : ! cplink(srcdir % "/home/" % usr % '/' % item, "/.sbsystemcopy/home/" % usr % '/' % item)) return false;
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
                            skppd = (QFile(srcdir % "/home/" % usr % '/' % item).size() >= 8000000) ? true : false;

                            switch(stype("/.sbsystemcopy/home/" % usr % '/' % item)) {
                            case Isfile:
                                if(mthd == 4)
                                {
                                    switch(fcomp("/.sbsystemcopy/home/" % usr % '/' % item, srcdir % "/etc/skel/" % item)) {
                                    case 1:
                                        if(! cpertime(srcdir % "/etc/skel/" % item, "/.sbsystemcopy/home/" % usr % '/' % item)) return false;
                                    case 2:
                                        goto nitem_1;
                                    }
                                }
                                else
                                {
                                    switch(fcomp("/.sbsystemcopy/home/" % usr % '/' % item, srcdir % "/home/" % usr % '/' % item)) {
                                    case 1:
                                        if(! cpertime(srcdir % "/home/" % usr % '/' % item, "/.sbsystemcopy/home/" % usr % '/' % item)) return false;
                                    case 2:
                                        goto nitem_1;
                                    }

                                    if(mthd > 1 && skppd) goto nitem_1;
                                }
                            case Islink:
                                QFile::remove("/.sbsystemcopy/home/" % usr % '/' % item);
                                break;
                            case Isdir:
                                recrmdir("/.sbsystemcopy/home/" % usr % '/' % item);
                            }

                            if(mthd == 4)
                            {
                                if(! cpfile(srcdir % "/etc/skel/" % item, "/.sbsystemcopy/home/" % usr % '/' % item)) return false;
                            }
                            else if(mthd == 1 || ! skppd)
                                if(! cpfile(srcdir % "/home/" % usr % '/' % item, "/.sbsystemcopy/home/" % usr % '/' % item)) return false;
                        }
                    }

                nitem_1:;
                    if(ThrdKill) return false;
                }

                in.setString(cditms, QIODevice::ReadOnly);

                while(! in.atEnd())
                {
                    QStr line(in.readLine()), item(right(line, - instr(line, "_")));
                    ThrdDbg = (mthd == 4) ? QStr("@/etc/skel/" % item) : QStr("@/home/" % usr % '/' % item);

                    if(left(line, instr(line, "_") - 1).toShort() == Isdir)
                        if(exist("/.sbsystemcopy/home/" % usr % '/' % item))
                            if((mthd == 4) ? ! cpertime(srcdir % "/etc/skel/" % item, "/.sbsystemcopy/home/" % usr % '/' % item) : ! cpertime(srcdir % "/home/" % usr % '/' % item, "/.sbsystemcopy/home/" % usr % '/' % item)) return false;

                    if(ThrdKill) return false;
                }

                cditms->clear();

                if(mthd > 1)
                {
                    if(isfile(srcdir % "/home/" % usr % "/.config/user-dirs.dirs"))
                    {
                        QFile file(srcdir % "/home/" % usr % "/.config/user-dirs.dirs");
                        file.open(QIODevice::ReadOnly);
                        QTS in(&file);

                        while(! in.atEnd())
                        {
                            QStr cline(in.readLine()), dir;

                            if(! cline.startsWith('#') && cline.contains("$HOME"))
                            {
                                dir = left(right(cline, - instr(cline, "/")), -1);
                                if(isdir(srcdir % "/home/" % usr % '/' % dir)) cpdir(srcdir % "/home/" % usr % '/' % dir, "/.sbsystemcopy/home/" % usr % '/' % dir);
                            }

                            if(ThrdKill) return false;
                        }

                        file.close();
                    }
                }

                if(! cpertime(srcdir % "/home/" % usr, "/.sbsystemcopy/home/" % usr)) return false;
            }
        }

        if(! cpertime(srcdir % "/home", "/.sbsystemcopy/home")) return false;
    }

    if(isdir(srcdir % "/root"))
    {
        if(! isdir("/.sbsystemcopy/root") && ! QDir().mkdir("/.sbsystemcopy/root"))
        {
            QFile::rename("/.sbsystemcopy/root", "/.sbsystemcopy/root_" % rndstr());
            if(! QDir().mkdir("/.sbsystemcopy/root")) return false;
        }

        in.setString(&rootitms, QIODevice::ReadOnly);

        while(! in.atEnd())
        {
            QStr line(in.readLine()), item(right(line, - instr(line, "_")));
            ++cnum;
            cperc = (cnum * 100 + 50) / anum;
            if(Progress < cperc) Progress = cperc;
            ThrdDbg = (mthd == 4) ? QStr("@/etc/skel/" % item) : QStr("@/root/" % item);

            if(mthd == 4 || (! like(item, QSL() << "_lost+found_" << "_lost+found/*" << "*/lost+found_" << "*/lost+found/*" << "_Systemback_" << "_Systemback/*" << "*/Systemback_" << "*/Systemback/*" << "*~_" << "*~/*") && ! exclcheck(elist, item) && (macid.isEmpty() || ! item.contains(macid))))
            {
                switch(left(line, instr(line, "_") - 1).toShort()) {
                case Islink:
                    switch(stype("/.sbsystemcopy/root/" % item)) {
                    case Islink:
                        if((mthd == 4) ? ! lcomp("/.sbsystemcopy/root/" % item, srcdir % "/etc/skel/" % item) : ! lcomp("/.sbsystemcopy/root/" % item, srcdir % "/root/" % item)) goto nitem_2;
                    case Isfile:
                        QFile::remove("/.sbsystemcopy/root/" % item);
                        break;
                    case Isdir:
                        recrmdir("/.sbsystemcopy/root/" % item);
                    }

                    if((mthd == 4) ? cplink(srcdir % "/etc/skel/" % item, "/.sbsystemcopy/root/" % item) : ! cplink(srcdir % "/root/" % item, "/.sbsystemcopy/root/" % item)) return false;
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
                    skppd = (QFile(srcdir % "/root/" % item).size() >= 8000000) ? true : false;

                    switch(stype("/.sbsystemcopy/root/" % item)) {
                    case Isfile:
                        if(mthd == 4)
                        {
                            switch(fcomp("/.sbsystemcopy/root/" % item, srcdir % "/etc/skel/" % item)) {
                            case 1:
                                if(! cpertime(srcdir % "/etc/skel/" % item, "/.sbsystemcopy/root/" % item)) return false;
                            case 2:
                                goto nitem_2;
                            }
                        }
                        else
                        {
                            switch(fcomp("/.sbsystemcopy/root/" % item, srcdir % "/root/" % item)) {
                            case 1:
                                if(! cpertime(srcdir % "/root/" % item, "/.sbsystemcopy/root/" % item)) return false;
                            case 2:
                                goto nitem_2;
                            }

                            if(mthd > 1 && skppd) goto nitem_2;
                        }
                    case Islink:
                        QFile::remove("/.sbsystemcopy/root/" % item);
                        break;
                    case Isdir:
                        recrmdir("/.sbsystemcopy/root/" % item);
                    }

                    if(mthd == 4)
                    {
                        if(! cpfile(srcdir % "/etc/skel/" % item, "/.sbsystemcopy/root/" % item)) return false;
                    }
                    else if(mthd == 1 || ! skppd)
                        if(! cpfile(srcdir % "/root/" % item, "/.sbsystemcopy/root/" % item)) return false;
                }
            }

        nitem_2:;
            if(ThrdKill) return false;
        }

        in.setString(&rootitms, QIODevice::ReadOnly);

        while(! in.atEnd())
        {
            QStr line(in.readLine()), item(right(line, - instr(line, "_")));
            ThrdDbg = (mthd == 4) ? QStr("@/etc/skel/" % item) : QStr("@/root/" % item);

            if(left(line, instr(line, "_") - 1).toShort() == Isdir)
                if(exist("/.sbsystemcopy/root/" % item))
                    if((mthd == 4) ? ! cpertime(srcdir % "/etc/skel/" % item, "/.sbsystemcopy/root/" % item) : ! cpertime(srcdir % "/root/" % item, "/.sbsystemcopy/root/" % item)) return false;

            if(ThrdKill) return false;
        }

        if(! cpertime(srcdir % "/root", "/.sbsystemcopy/root")) return false;
        rootitms.clear();
    }

    QSL dlst(QDir("/.sbsystemcopy").entryList(QDir::Files));

    for(uchar a(0) ; a < dlst.count() ; ++a)
    {
        QStr item(dlst.at(a));

        if(like(item, QSL() << "_initrd.img*" << "_vmlinuz*"))
        {
            if(islink("/.sbsystemcopy/" % item))
                if(islink(srcdir % '/' % item))
                    if(lcomp("/.sbsystemcopy/" % item, srcdir % '/' % item)) continue;

            if(! QFile::remove("/.sbsystemcopy/" % item)) return false;
        }

        if(ThrdKill) return false;
    }

    dlst = QDir(srcdir.isEmpty() ? "/" : srcdir).entryList(QDir::Files);

    for(uchar a(0) ; a < dlst.count() ; ++a)
    {
        QStr item(dlst.at(a));

        if(like(item, QSL() << "_initrd.img*" << "_vmlinuz*"))
            if(islink(srcdir % '/' % item))
                if(! exist("/.sbsystemcopy/" % item))
                    if(! cplink(srcdir % '/' % item, "/.sbsystemcopy/" % item)) return false;

        if(ThrdKill) return false;
    }

    dlst = QSL() << "/cdrom" << "/dev" << "/home" << "/mnt" << "/root" << "/proc" << "/run" << "/sys" << "/tmp";

    for(uchar a(0) ; a < dlst.count() ; ++a)
    {
        QStr cdir(dlst.at(a));

        if(isdir(srcdir % cdir))
        {
            if(isdir("/.sbsystemcopy" % cdir))
            {
                if(! cpertime(srcdir % cdir, "/.sbsystemcopy" % cdir)) return false;
            }
            else
            {
                if(exist("/.sbsystemcopy" % cdir)) QFile::remove("/.sbsystemcopy" % cdir);
                if(! cpdir(srcdir % cdir, "/.sbsystemcopy" % cdir)) return false;
            }
        }
        else if(exist("/.sbsystemcopy" % cdir))
            stype("/.sbsystemcopy" % cdir) == Isdir ? recrmdir("/.sbsystemcopy" % cdir) : QFile::remove("/.sbsystemcopy" % cdir);

        if(ThrdKill) return false;
    }

    elist = QSL() << "/etc/mtab" << "/var/cache/fontconfig/" << "/var/lib/dpkg/lock" << "/var/lib/udisks/mtab" << "/var/log/" << "/var/run/" << "/var/tmp/";
    if(mthd > 2) elist.append(QSL() << "/etc/machine-id" << "/etc/systemback.conf" << "/etc/systemback.excludes" << "/var/lib/dbus/machine-id");

    if(srcdir == "/.systembacklivepoint")
        if(fload("/proc/cmdline").contains("noxconf")) elist.append("/etc/X11/xorg.conf");

    dlst = QSL() << "/bin" << "/boot" << "/etc" << "/lib" << "/lib32" << "/lib64" << "/opt" << "/sbin" << "/selinux" << "/srv" << "/usr" << "/var";

    for(uchar a(0) ; a < dlst.count() ; ++a)
    {
        QStr cdir(dlst.at(a));

        if(isdir(srcdir % cdir))
        {
            if(isdir("/.sbsystemcopy" % cdir))
            {
                QSL sdlst(odir("/.sbsystemcopy" % cdir));

                for(ushort b(0) ; b < sdlst.count() ; ++b)
                {
                    QStr item(sdlst.at(b));

                    if(! like(item, QSL() << "_lost+found_" << "_Systemback_"))
                        if(! exist(srcdir % cdir % '/' % item)) (stype("/.sbsystemcopy" % cdir % '/' % item) == Isdir) ? recrmdir("/.sbsystemcopy" % cdir % '/' % item) : QFile::remove("/.sbsystemcopy" % cdir % '/' % item);

                    if(ThrdKill) return false;
                }
            }
            else
            {
                if(exist("/.sbsystemcopy" % cdir)) QFile::remove("/.sbsystemcopy" % cdir);
                if(! QDir().mkdir("/.sbsystemcopy" % cdir)) return false;
            }

            switch(a) {
            case 0:
                cditms = &binitms;
                break;
            case 1:
                cditms = &bootitms;
                break;
            case 2:
                cditms = &etcitms;
                break;
            case 3:
                cditms = &libitms;
                break;
            case 4:
                cditms = &lib32itms;
                break;
            case 5:
                cditms = &lib64itms;
                break;
            case 6:
                cditms = &optitms;
                break;
            case 7:
                cditms = &sbinitms;
                break;
            case 8:
                cditms = &selinuxitms;
                break;
            case 9:
                cditms = &srvitms;
                break;
            case 10:
                cditms = &usritms;
                break;
            default:
                cditms = &varitms;
            }

            QTS in(cditms, QIODevice::ReadOnly);

            while(! in.atEnd())
            {
                QStr line(in.readLine()), item(right(line, - instr(line, "_")));
                ++cnum;
                cperc = (cnum * 100 + 50) / anum;
                if(Progress < cperc) Progress = cperc;
                ThrdDbg = '@' % cdir % '/' % item;

                if(! like(item, QSL() << "_lost+found_" << "_lost+found/*" << "*/lost+found_" << "*/lost+found/*" << "_Systemback_" << "_Systemback/*" << "*/Systemback_" << "*/Systemback/*") && ! exclcheck(elist, QStr(cdir % '/' % item)) && (macid.isEmpty() || ! item.contains(macid)) && (mthd < 3 || ! (QStr(cdir % '/' % item).startsWith("/etc/udev/rules.d") && item.contains("-persistent-"))))
                {
                    switch(left(line, instr(line, "_") - 1).toShort()) {
                    case Islink:
                        switch(stype("/.sbsystemcopy" % cdir % '/' % item)) {
                        case Islink:
                            if(lcomp("/.sbsystemcopy" % cdir % '/' % item, srcdir % cdir % '/' % item)) goto nitem_3;
                        case Isfile:
                            QFile::remove("/.sbsystemcopy" % cdir % '/' % item);
                            break;
                        case Isdir:
                            recrmdir("/.sbsystemcopy" % cdir % '/' % item);
                        }

                        if(! cplink(srcdir % cdir % '/' % item, "/.sbsystemcopy" % cdir % '/' % item)) return false;
                        break;
                    case Isdir:
                        switch(stype("/.sbsystemcopy" % cdir % '/' % item)) {
                        case Isdir:
                        {
                            QSL sdlst(odir("/.sbsystemcopy" % cdir % '/' % item));

                            for(ushort b(0) ; b < sdlst.count() ; ++b)
                            {
                                QStr sitem(sdlst.at(b));

                                if(! like(sitem, QSL() << "_lost+found_" << "_Systemback_"))
                                    if(! exist(srcdir % cdir % '/' % item % '/' % sitem) || cdir % '/' % item % '/' % sitem == "/etc/X11/xorg.conf") (stype("/.sbsystemcopy" % cdir % '/' % item % '/' % sitem) == Isdir) ? recrmdir("/.sbsystemcopy" % cdir % '/' % item % '/' % sitem) : QFile::remove("/.sbsystemcopy" % cdir % '/' % item % '/' % sitem);

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
                                if(! cpertime(srcdir % cdir % '/' % item, "/.sbsystemcopy" % cdir % '/' % item)) return false;
                            case 2:
                                goto nitem_3;
                            }

                            break;
                        case Islink:
                            QFile::remove("/.sbsystemcopy" % cdir % '/' % item);
                        case Isdir:
                            recrmdir("/.sbsystemcopy" % cdir % '/' % item);
                        }

                        if(! cpfile(srcdir % cdir % '/' % item, "/.sbsystemcopy" % cdir % '/' % item)) return false;
                    }
                }

            nitem_3:;
                if(ThrdKill) return false;
            }

            in.setString(cditms, QIODevice::ReadOnly);

            while(! in.atEnd())
            {
                QStr line(in.readLine()), item(right(line, - instr(line, "_")));
                ThrdDbg = '@' % cdir % '/' % item;

                if(left(line, instr(line, "_") - 1).toShort() == Isdir)
                    if(exist("/.sbsystemcopy" % cdir % '/' % item))
                        if(! cpertime(srcdir % cdir % '/' % item, "/.sbsystemcopy" % cdir % '/' % item)) return false;

                if(ThrdKill) return false;
            }

            if(! cpertime(srcdir % cdir, "/.sbsystemcopy" % cdir)) return false;
            cditms->clear();
        }
        else if(exist("/.sbsystemcopy" % cdir))
            stype("/.sbsystemcopy" % cdir) == Isdir ? recrmdir("/.sbsystemcopy" % cdir) : QFile::remove("/.sbsystemcopy" % cdir);
    }

    if(isdir(srcdir % "/media"))
    {
        if(isdir("/.sbsystemcopy/media"))
        {
            dlst = QDir("/media").entryList(QDir::Dirs | QDir::NoDotAndDotDot);

            for(uchar a(0) ; a < dlst.count() ; ++a)
            {
                QStr item(dlst.at(a));
                if(! exist(srcdir % "/media/" % item) && ! mcheck("/.sbsystemcopy/media/" % item % '/')) recrmdir("/.sbsystemcopy/media/" % item);
                if(ThrdKill) return false;
            }
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
                file.setFileName("/etc/fstab");
                file.open(QIODevice::ReadOnly);
                in.setDevice(&file);

                for(uchar a(0) ; a < dlst.count() ; ++a)
                {
                    QStr item(dlst.at(a));
                    if(a > 0) file.open(QIODevice::ReadOnly);

                    while(! in.atEnd())
                    {
                        QStr cline(replace(in.readLine(), "\t", " "));

                        if(! cline.startsWith("#") && like(replace(cline, "\\040", " "), QSL() << "* /media/" % item % " *" << "* /media/" % item % "/*"))
                        {
                            QStr fdir;
                            QSL cdlst(QStr(mid(cline, instr(cline, "/media/") + 7, instr(cline, " ", instr(cline, "/media/")) - instr(cline, "/media/") - 7)).split('/'));

                            for(uchar b(0) ; b < cdlst.count() ; ++b)
                            {
                                QStr cdname(cdlst.at(b));

                                if(! cdname.isEmpty())
                                {
                                    fdir.append('/' % replace(cdname, "\\040", " "));

                                    if(! isdir("/.sbsystemcopy/media" % fdir))
                                        if(! cpdir("/media" % fdir, "/.sbsystemcopy/media" % fdir)) return false;
                                }
                            }
                        }

                        if(ThrdKill) return false;
                    }

                    file.close();
                }
            }
        }
        else
        {
            QStr mediaitms(rodir(srcdir % "/media"));
            QTS in(&mediaitms, QIODevice::ReadOnly);

            while(! in.atEnd())
            {
                QStr line(in.readLine()), item(right(line, - instr(line, "_")));
                ThrdDbg = "@/media/" % item;

                if(exist("/.sbsystemcopy/media/" % item))
                {
                    if(stype("/.sbsystemcopy/media/" % item) != Isdir)
                    {
                        QFile::remove("/.sbsystemcopy/media/" % item);
                        if(! QDir().mkdir("/.sbsystemcopy/media/" % item)) return false;
                    }

                    if(! cpertime(srcdir % "/media/" % item, "/.sbsystemcopy/media/" % item)) return false;
                }
                else
                    if(! cpdir(srcdir % "/media/" % item, "/.sbsystemcopy/media/" % item)) return false;

                if(ThrdKill) return false;
            }
        }

        if(! cpertime(srcdir % "/media", "/.sbsystemcopy/media")) return false;
    }
    else if(exist("/.sbsystemcopy/media"))
        stype("/.sbsystemcopy/media") == Isdir ? recrmdir("/.sbsystemcopy/media") : QFile::remove("/.sbsystemcopy/media");

    if(isdir(srcdir % "/var/log"))
    {
        QStr logitms(rodir(srcdir % "/var/log"));
        in.setString(&logitms, QIODevice::ReadOnly);

        while(! in.atEnd())
        {
            QStr line(in.readLine()), item(right(line, - instr(line, "_")));
            ThrdDbg = "@/var/log/" % item;

            switch(left(line, instr(line, "_") - 1).toShort()) {
            case Isdir:
                if(! cpdir(srcdir % "/var/log/" % item, "/.sbsystemcopy/var/log/" % item)) return false;
                break;
            case Isfile:
                if(! like(item, QSL() << "*.0_" << "*.1_" << "*.gz_" << "*.old_"))
                {
                    crtfile("/.sbsystemcopy/var/log/" % item, NULL);
                    if(! cpertime("/var/log/" % item, "/.sbsystemcopy/var/log/" % item)) return false;
                }
            }

            if(ThrdKill) return false;
        }

        if(! cpertime(srcdir % "/var/log", "/.sbsystemcopy/var/log")) return false;
        if(! cpertime(srcdir % "/var", "/.sbsystemcopy/var")) return false;
    }

    if(srcdir == "/.systembacklivepoint" && isdir("/.systembacklivepoint/.systemback"))
    {
        QStr sbitms(rodir(srcdir % "/.systemback"));
        QTS in(&sbitms, QIODevice::ReadOnly);

        while(! in.atEnd())
        {
            QStr line(in.readLine()), item(right(line, - instr(line, "_")));
            ThrdDbg = "@/.systemback/" % item;

            switch(left(line, instr(line, "_") - 1).toShort()) {
            case Islink:
                if(item != "etc/fstab")
                    if(! cplink(srcdir % "/.systemback/" % item, "/.sbsystemcopy/" % item)) return false;

                break;
            case Isfile:
                if(item != "etc/fstab")
                    if(! cpfile(srcdir % "/.systemback/" % item, "/.sbsystemcopy/" % item)) return false;
            }

            if(ThrdKill) return false;
        }

        in.setString(&sbitms, QIODevice::ReadOnly);

        while(! in.atEnd())
        {
            QStr line(in.readLine()), item(right(line, - instr(line, "_")));
            ThrdDbg = "@/.systemback/" % item;

            if(left(line, instr(line, "_") - 1).toShort() == Isdir)
                if(! cpertime(srcdir % "/.systemback/" % item, "/.sbsystemcopy/" % item)) return false;

            if(ThrdKill) return false;
        }
    }

    ThrdDbg.clear();
    return true;
}

bool sb::thrdlvprpr(bool &iudata)
{
    if(ThrdLng > 0) ThrdLng = 0;
    QStr sitms;
    QSL dlst(QSL() << "/bin" << "/boot" << "/etc" << "/lib" << "/lib32" << "/lib64" << "/opt" << "/sbin" << "/selinux" << "/usr" << "/initrd.img" << "/initrd.img.old" << "/vmlinuz" << "/vmlinuz.old");

    for(uchar a(0) ; a < dlst.count() ; ++a)
    {
        QStr item(dlst.at(a));

        if(isdir(item))
        {
            sitms.append(rodir(item));
            ++ThrdLng;
        }

        if(ThrdKill) return false;
    }

    ThrdLng += sitms.count('\n');
    sitms.clear();
    if(! QDir().mkdir(sdir[2] % "/.sblivesystemcreate/.systemback")) return false;
    if(! QDir().mkdir(sdir[2] % "/.sblivesystemcreate/.systemback/etc")) return false;

    if(isdir("/etc/udev"))
    {
        if(! QDir().mkdir(sdir[2] % "/.sblivesystemcreate/.systemback/etc/udev")) return false;

        if(isdir("/etc/udev/rules.d"))
        {
            if(! QDir().mkdir(sdir[2] % "/.sblivesystemcreate/.systemback/etc/udev/rules.d")) return false;

            if(isfile("/etc/udev/rules.d/70-persistent-cd.rules"))
                if(! cpfile("/etc/udev/rules.d/70-persistent-cd.rules", sdir[2] % "/.sblivesystemcreate/.systemback/etc/udev/rules.d/70-persistent-cd.rules")) return false;

            if(isfile("/etc/udev/rules.d/70-persistent-net.rules"))
                if(! cpfile("/etc/udev/rules.d/70-persistent-net.rules", sdir[2] % "/.sblivesystemcreate/.systemback/etc/udev/rules.d/70-persistent-net.rules")) return false;

            if(! cpertime("/etc/udev/rules.d", sdir[2] % "/.sblivesystemcreate/.systemback/etc/udev/rules.d")) return false;
        }

        if(! cpertime("/etc/udev", sdir[2] % "/.sblivesystemcreate/.systemback/etc/udev")) return false;
    }

    if(isfile("/etc/fstab"))
        if(! cpfile("/etc/fstab", sdir[2] % "/.sblivesystemcreate/.systemback/etc/fstab")) return false;

    if(! cpertime("/etc", sdir[2] % "/.sblivesystemcreate/.systemback/etc")) return false;
    if(exist("/.sblvtmp")) stype("/.sblvtmp") == Isdir ? recrmdir("/.sblvtmp") : QFile::remove("/.sblvtmp");
    if(! QDir().mkdir("/.sblvtmp")) return false;
    dlst = QSL() << "/cdrom" << "/dev" << "/mnt" << "/proc" << "/run" << "/srv" << "/sys" << "/tmp";

    for(uchar a(0) ; a < dlst.count() ; ++a)
    {
        QStr cdir(dlst.at(a));

        if(isdir(cdir))
        {
            if(! cpdir(cdir, "/.sblvtmp" % cdir)) return false;
            ++ThrdLng;
        }
    }

    if(ThrdKill) return false;

    if(! isdir("/media"))
    {
        if(! QDir().mkdir("/media")) return false;
    }
    else if(exist("/media/.sblvtmp"))
       stype("/media/.sblvtmp") == Isdir ? recrmdir("/media/.sblvtmp") : QFile::remove("/media/.sblvtmp");

    if(! QDir().mkdir("/media/.sblvtmp")) return false;
    if(! QDir().mkdir("/media/.sblvtmp/media")) return false;
    ++ThrdLng;
    if(ThrdKill) return false;

    if(isfile("/etc/fstab"))
    {
        QSL dlst(QDir("/media").entryList(QDir::Dirs | QDir::NoDotAndDotDot));
        QFile file("/etc/fstab");
        file.open(QIODevice::ReadOnly);
        QTS in(&file);

        for(uchar a(0) ; a < dlst.count() ; ++a)
        {
            QStr item(dlst.at(a));
            if(a > 0) file.open(QIODevice::ReadOnly);

            while(! in.atEnd())
            {
                QStr cline(replace(in.readLine(), "\t", " "));

                if(! cline.startsWith("#") && like(replace(cline, "\\040", " "), QSL() << "* /media/" % item % " *" << "* /media/" % item % "/*"))
                {
                    QStr fdir;
                    QSL cdlst(QStr(mid(cline, instr(cline, "/media/") + 7, instr(cline, " ", instr(cline, "/media/")) - instr(cline, "/media/") - 7)).split('/'));

                    for(uchar b(0) ; b < cdlst.count() ; ++b)
                    {
                        QStr cdname(cdlst.at(b));

                        if(! cdname.isEmpty())
                        {
                            fdir.append('/' % replace(cdname, "\\040", " "));

                            if(! isdir("/media/.sblvtmp/media" % fdir))
                            {
                                if(! cpdir("/media" % fdir, "/media/.sblvtmp/media" % fdir)) return false;
                                ++ThrdLng;
                            }
                        }
                    }
                }

                if(ThrdKill) return false;
            }

            file.close();
        }
    }

    if(exist("/var/.sblvtmp")) stype("/var/.sblvtmp") == Isdir ? recrmdir("/var/.sblvtmp") : QFile::remove("/var/.sblvtmp");
    if(ThrdKill) return false;
    QStr varitms(rodir("/var"));
    QTS in(&varitms, QIODevice::ReadOnly);
    if(ThrdKill) return false;
    if(! QDir().mkdir("/var/.sblvtmp")) return false;
    if(! QDir().mkdir("/var/.sblvtmp/var")) return false;
    ++ThrdLng;
    QSL elist(QSL() << "cache/fontconfig/" << "lib/dpkg/lock" << "lib/udisks/mtab" << "lib/ureadahead/" << "log/" << "run/" << "tmp/");

    while(! in.atEnd())
    {
        QStr line(in.readLine()), item(right(line, - instr(line, "_")));
        ThrdDbg = "@/var/" % item;

        if(! (item.startsWith("cache/apt/") && (item.endsWith(".bin") || item.contains(".bin."))) && ! (item.startsWith("cache/apt/archives/") && item.endsWith(".deb")) && ! like(item, QSL() << "_lost+found_" << "_lost+found/*" << "*/lost+found_" << "*/lost+found/*" << "_Systemback_" << "_Systemback/*" << "*/Systemback_" << "*/Systemback/*" << "*.dpkg-old_" << "*~_" << "*~/*") && ! exclcheck(elist, item))
        {
            switch(left(line, instr(line, "_") - 1).toShort()) {
            case Islink:
                if(! cplink("/var/" % item, "/var/.sblvtmp/var/" % item)) return false;
                ++ThrdLng;
                break;
            case Isdir:
                if(! QDir().mkdir("/var/.sblvtmp/var/" % item)) return false;
                ++ThrdLng;
                break;
            case Isfile:
                if(issmfs("/var/.sblvtmp", "/var/" % item))
                    if(link(QStr("/var/" % item).toStdString().c_str(), QStr("/var/.sblvtmp/var/" % item).toStdString().c_str()) == -1) return false;

                ++ThrdLng;
            }
        }
        else if(item.startsWith("log"))
        {
            switch(left(line, instr(line, "_") - 1).toShort()) {
            case Isdir:
                if(! QDir().mkdir("/var/.sblvtmp/var/" % item)) return false;
                ++ThrdLng;
                break;
            case Isfile:
                if(! like(item, QSL() << "*.0_" << "*.1_" << "*.gz_" << "*.old_"))
                {
                    crtfile("/var/.sblvtmp/var/" % item, NULL);
                    if(! cpertime("/var/" % item, "/var/.sblvtmp/var/" % item)) return false;
                    ++ThrdLng;
                }
            }
        }

        if(ThrdKill) return false;
    }

    in.setString(&varitms, QIODevice::ReadOnly);

    while(! in.atEnd())
    {
        QStr line(in.readLine()), item(right(line, - instr(line, "_")));
        ThrdDbg = "@/var/" % item;

        if(left(line, instr(line, "_") - 1).toShort() == Isdir)
            if(exist("/var/.sblvtmp/var/" % item))
                if(! cpertime("/var/" % item, "/var/.sblvtmp/var/" % item)) return false;

        if(ThrdKill) return false;
    }

    if(! cpertime("/var", "/var/.sblvtmp/var")) return false;
    QSL usrs;
    QFile file("/etc/passwd");
    file.open(QIODevice::ReadOnly);
    in.setDevice(&file);

    while(! in.atEnd())
    {
        QStr usr(in.readLine());

        if(usr.contains(":/home/"))
        {
            usr = left(usr, instr(usr, ":") -1);
            if(isdir("/home/" % usr)) usrs.append(usr);
        }
    }

    file.close();
    if(ThrdKill) return false;
    bool uhl(true);

    if(dfree("/home") > 104857600 && dfree("/root") > 104857600)
    {
        for(uchar a(0) ; a < usrs.count() ; ++a)
        {
            QStr usr(usrs.at(a));

            if(! issmfs("/home", "/home/" % usr))
            {
                uhl = false;
                break;
            }

            if(ThrdKill) return false;
        }
    }
    else
        uhl = false;

    if(uhl)
    {
        if(exist("/home/.sbuserdata")) stype("/home/.sbuserdata") == Isdir ? recrmdir("/home/.sbuserdata") : QFile::remove("/home/.sbuserdata");
        if(! QDir().mkdir("/home/.sbuserdata")) return false;
        if(! QDir().mkdir("/home/.sbuserdata/home")) return false;
    }
    else
    {
        if(! QDir().mkdir(sdir[2] % "/.sblivesystemcreate/userdata")) return false;
        if(! QDir().mkdir(sdir[2] % "/.sblivesystemcreate/userdata/home")) return false;
    }

    ++ThrdLng;
    if(ThrdKill) return false;
    elist = QSL() << ".sbuserdata" << ".cache/gvfs" << ".local/share/Trash/files/" << ".local/share/Trash/info/" << ".Xauthority" << ".ICEauthority";
    file.setFileName("/etc/systemback.excludes");
    file.open(QIODevice::ReadOnly);

    while(! in.atEnd())
    {
        QStr cline(in.readLine());
        elist.append(cline);
        if(ThrdKill) return false;
    }

    file.close();

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
        ++ThrdLng;
        if(ThrdKill) return false;
        QStr rootitms;
        rootitms = iudata ? rodir("/root") : rodir("/root", true);
        in.setString(&rootitms, QIODevice::ReadOnly);

        while(! in.atEnd())
        {
            QStr line(in.readLine()), item(right(line, - instr(line, "_")));
            ThrdDbg = "@/root/" % item;

            if(! like(item, QSL() << "_lost+found_" << "_lost+found/*" << "*/lost+found_" << "*/lost+found/*" << "_Systemback_" << "_Systemback/*" << "*/Systemback_" << "*/Systemback/*" << "*~_" << "*~/*") && ! exclcheck(elist, item))
            {
                if(exist("/root/" % item))
                {
                    switch(left(line, instr(line, "_") - 1).toShort()) {
                    case Islink:
                        if(! cplink("/root/" % item, usdir % "/root/" % item)) return false;
                        ++ThrdLng;
                        break;
                    case Isdir:
                        if(! QDir().mkdir(usdir % "/root/" % item)) return false;
                        ++ThrdLng;
                        break;
                    case Isfile:
                        if(iudata || QFile("/root/" % item).size() <= 8000000)
                        {
                            if(uhl)
                            {
                                if(link(QStr("/root/" % item).toStdString().c_str(), QStr(usdir % "/root/" % item).toStdString().c_str()) == -1) return false;
                            }
                            else if(! cpfile("/root/" % item, usdir % "/root/" % item))
                                return false;

                            ++ThrdLng;
                        }
                    }
                }
            }

            if(ThrdKill) return false;
        }

        in.setString(&rootitms, QIODevice::ReadOnly);

        while(! in.atEnd())
        {
            QStr line(in.readLine()), item(right(line, - instr(line, "_")));
            ThrdDbg = "@/root/" % item;

            if(left(line, instr(line, "_") - 1).toShort() == Isdir)
                if(exist(usdir % "/root/" % item))
                    if(! cpertime("/root/" % item, usdir % "/root/" % item)) return false;

            if(ThrdKill) return false;
        }

        if(! cpertime("/root", usdir % "/root")) return false;
    }

    for(uchar a(0) ; a < usrs.count() ; ++a)
    {
        QStr udir(usrs.at(a)), usdir;
        usdir = uhl ? "/home/.sbuserdata/home" : QStr(sdir[2] % "/.sblivesystemcreate/userdata/home");
        if(! QDir().mkdir(usdir % '/' % udir)) return false;
        ++ThrdLng;
        QStr useritms;
        useritms = iudata ? rodir("/home/" % udir) : rodir("/home/" % udir, true);
        in.setString(&useritms, QIODevice::ReadOnly);

        while(! in.atEnd())
        {
            QStr line(in.readLine()), item(right(line, - instr(line, "_")));
            ThrdDbg = "@/home/" % udir % '/' % item;

            if(! like(item, QSL() << "_lost+found_" << "_lost+found/*" << "*/lost+found_" << "*/lost+found/*" << "_Systemback_" << "_Systemback/*" << "*/Systemback_" << "*/Systemback/*" << "*~_" << "*~/*") && ! exclcheck(elist, item) && exist("/home/" % udir % '/' % item))
            {
                switch(left(line, instr(line, "_") - 1).toShort()) {
                case Islink:
                    if(! cplink("/home/" % udir % '/' % item, usdir % '/' % udir % '/' % item)) return false;
                    ++ThrdLng;
                    break;
                case Isdir:
                    if(! QDir().mkdir(usdir % '/' % udir % '/' % item)) return false;
                    ++ThrdLng;
                    break;
                case Isfile:
                    if(iudata || QFile("/home/" % udir % '/' % item).size() <= 8000000)
                    {
                        if(uhl)
                        {
                            if(link(QStr("/home/" % udir % '/' % item).toStdString().c_str(), QStr(usdir % '/' % udir % '/' % item).toStdString().c_str()) == -1) return false;
                        }
                        else if(! cpfile("/home/" % udir % '/' % item, usdir % '/' % udir % '/' % item))
                            return false;

                        ++ThrdLng;
                    }
                }
            }

            if(ThrdKill) return false;
        }

        in.setString(&useritms, QIODevice::ReadOnly);

        while(! in.atEnd())
        {
            QStr line(in.readLine()), item(right(line, - instr(line, "_")));
            ThrdDbg = "@/home/" % udir % '/' % item;

            if(left(line, instr(line, "_") - 1).toShort() == Isdir)
                if(exist(usdir % '/' % udir % '/' % item))
                    if(! cpertime("/home/" % udir % '/' % item, usdir % '/' % udir % '/' % item)) return false;

            if(ThrdKill) return false;
        }

        if(! iudata && isfile("/home/" % udir % "/.config/user-dirs.dirs"))
        {
            file.setFileName("/home/" % udir % "/.config/user-dirs.dirs");
            file.open(QIODevice::ReadOnly);
            in.setDevice(&file);

            while(! in.atEnd())
            {
                QStr cline(in.readLine()), dir;

                if(! cline.startsWith('#') && cline.contains("$HOME"))
                {
                    dir = left(right(cline, - instr(cline, "/")), -1);

                    if(isdir("/home/" % udir % '/' % dir))
                    {
                        if(! cpdir("/home/" % udir % '/' % dir, usdir % '/' % udir % '/' % dir)) return false;
                        ++ThrdLng;
                    }
                }

                if(ThrdKill) return false;
            }

            file.close();
        }

        if(! cpertime("/home/" % udir, usdir % '/' % udir)) return false;
    }

    ThrdDbg.clear();
    return true;
}
