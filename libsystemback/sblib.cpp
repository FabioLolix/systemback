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
#include <sys/stat.h>
#include <sys/time.h>
#include <linux/fs.h>
#include <unistd.h>
#include <dirent.h>
#include <signal.h>
#include <utime.h>
#include <fcntl.h>

sb sb::SBThrd;
QSL sb::ThrdSlst;
QStr sb::ThrdStr[3], sb::ThrdDbg, sb::sdir[3], sb::schdle[7], sb::pnames[15], sb::trn[2], sb::odlst;
ullong sb::ThrdLng[2] = {0, 0};
int sb::sblock[3];
uchar sb::ThrdType, sb::ThrdChr, sb::pnumber(0);
char sb::Progress(-1);
bool sb::ExecKill(true), sb::ThrdKill(true), sb::ThrdBool, sb::ThrdRslt;

sb::sb(QThread *parent) : QThread(parent)
{
    setenv("PATH", "/usr/lib/systemback:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin", 1);
    umask(0);
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

    do {
        msleep(10);
        qApp->processEvents();
    } while(ushort(time.elapsed()) < msec);
}

QStr sb::left(QStr txt, short len)
{
    ushort plen(abs(len));
    if(len < 0) return txt.length() >= plen ? txt.left(txt.length() - plen) : NULL;
    return txt.length() >= plen ? txt.left(len) : txt;
}

QStr sb::right(QStr txt, short len)
{
    ushort plen(abs(len));
    if(len < 0) return txt.length() >= plen ? txt.right(txt.length() - plen) : NULL;
    return txt.length() >= plen ? txt.right(len) : txt;
}

QStr sb::mid(QStr txt, ushort start, ushort len)
{
    if(txt.length() >= start) return txt.length() - start + 1 >= len ? txt.mid(start - 1, len) : right(txt, - start + 1);
    return NULL;
}

bool sb::like(QStr txt, QSL lst, uchar mode)
{
    switch(mode) {
    case Norm:
        for(uchar a(0) ; a < lst.count() ; ++a)
        {
            QStr stxt(lst.at(a));

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
        }

        return false;
    case All:
        for(uchar a(0) ; a < lst.count() ; ++a)
        {
            QStr stxt(lst.at(a));

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
        }

        return true;
    case Mixed:
    {
        QSL alst, nlst;

        for(uchar a(0) ; a < lst.count() ; ++a)
        {
            QStr stxt(lst.at(a));

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
        }

        return like(txt, alst, All) && like(txt, nlst) ? true : false;
    }
    default:
        return false;
    }
}

bool sb::ilike(short num, QSIL lst)
{
    for(uchar a(0) ; a < lst.count() ; ++a)
        if(num == lst.at(a)) return true;

    return false;
}

ushort sb::instr(QStr txt, QStr stxt, ushort start)
{
    return txt.length() + 1 >= stxt.length() + start ? txt.indexOf(stxt, start - 1) + 1 : 0;
}

ushort sb::rinstr(QStr txt, QStr stxt, ushort start)
{
    return start == 0 ? txt.lastIndexOf(stxt) + 1 : start >= stxt.length() ? txt.left(start).lastIndexOf(stxt) + 1 : 0;
}

bool sb::isnum(QStr txt)
{
    for(uchar a(0) ; a < txt.length() ; ++a)
        if(! txt.at(a).isDigit()) return false;

    return txt.isEmpty() ? false : true;
}

QStr sb::rndstr(uchar vlen)
{
    QStr val, chrs("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz./"), chr;
    uchar clen(vlen == 16 ? 64 : 62);
    srand(time(NULL));

    do {
        chr = chrs.mid(rand() % clen, 1);
        if(! val.endsWith(chr)) val.append(chr);
    } while(val.length() < vlen);

    return val;
}

bool sb::exist(QStr path)
{
    struct stat istat;
    return lstat(path.toStdString().c_str(), &istat) == 0;
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
    struct stat istat;
    if(lstat(path.toStdString().c_str(), &istat) == -1) return Notexist;

    switch(istat.st_mode & S_IFMT) {
    case S_IFREG:
        return Isfile;
    case S_IFDIR:
        return Isdir;
    case S_IFLNK:
        return Islink;
    case S_IFBLK:
        return Isblock;
    default:
        return Unknow;
    }
}

QStr sb::ruuid(QStr part)
{
    ThrdType = Ruuid;
    ThrdStr[0] = part;
    SBThrd.start();
    thrdelay();
    return ThrdStr[1];
}

QStr sb::rlink(QStr path)
{
    struct stat istat;
    if(lstat(path.toStdString().c_str(), &istat) == -1) return NULL;
    char rpath[istat.st_size];
    rpath[readlink(path.toStdString().c_str(), rpath, sizeof rpath)] = '\0';
    return rpath;
}

QStr sb::fload(QStr path)
{
    QFile file(path);
    if(! file.open(QIODevice::ReadOnly)) return NULL;
    return file.readAll();
}

bool sb::crtfile(QStr path, QStr txt)
{
    if(! ilike(stype(path), QSIL() << Notexist << Isfile) || ! isdir(path.left(path.lastIndexOf('/')))) return false;
    QFile file(path);
    if(! file.open(QFile::WriteOnly | QFile::Truncate) || file.write(txt.toLocal8Bit()) == -1) return false;
    file.flush();
    return true;
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

ullong sb::dfree(QStr path)
{
    struct statvfs dstat;
    return statvfs(path.toStdString().c_str(), &dstat) == -1 ? 0 : dstat.f_bavail * dstat.f_bsize;
}

ullong sb::fsize(QStr path)
{
    return QFileInfo(path).size();
}

bool sb::remove(QStr path)
{
    ThrdType = Remove;
    ThrdStr[0] = path;
    SBThrd.start();
    thrdelay();
    return ThrdRslt;
}

bool sb::islnxfs(QStr dirpath)
{
    QStr fpath(dirpath % '/' % rndstr() % "_sbdirtestfile");
    if(! crtfile(fpath) || chmod(fpath.toStdString().c_str(), 0776) == -1) return false;
    struct stat fstat;
    bool err(stat(fpath.toStdString().c_str(), &fstat) == -1);
    QFile::remove(fpath);
    return err ? false : fstat.st_mode == 33278;
}

bool sb::ickernel()
{
    QStr ckernel(ckname());

    if(isfile("/lib/modules/" % ckernel % "/modules.order"))
    {
        QFile file("/lib/modules/" % ckernel % "/modules.order");

        if(file.open(QIODevice::ReadOnly))
            while(! file.atEnd())
                if(like(file.readLine().trimmed(), QSL() << "*aufs.ko_" << "*overlayfs.ko_" << "*unionfs.ko_")) return true;
    }

    if(isfile("/lib/modules/" % ckernel % "/modules.builtin"))
    {
        QFile file("/lib/modules/" % ckernel % "/modules.builtin");

        if(file.open(QIODevice::ReadOnly))
            while(! file.atEnd())
                if(like(file.readLine().trimmed(), QSL() << "*aufs.ko_" << "*overlayfs.ko_" << "*unionfs.ko_")) return true;
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
                    if(cline.endsWith("efivars.ko") && isfile("/lib/modules/" % ckernel % '/' % cline) && exec("modprobe efivars", NULL, true) == 0 && isdir("/sys/firmware/efi")) return true;
                }
        }
    }

    return false;
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

                    for(uchar a(0) ; a < vals.count() ; ++a)
                        switch(a) {
                        case 0:
                            schdle[1] = vals.at(0);
                            break;
                        case 1:
                            schdle[2] = vals.at(1);
                            break;
                        case 2:
                            schdle[3] = vals.at(2);
                            break;
                        case 3:
                            schdle[4] = vals.at(3);
                        }
                }
                else if(cline.startsWith("pointsnumber="))
                    pnumber = cval.toShort();
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

    if(sdir[2].isEmpty())
    {
        sdir[2] = "/home";
        if(! cfgupdt) cfgupdt = true;
    }

    if(! like(schdle[0], QSL() << "_on_" << "_off_"))
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
    else if(schdle[1].toShort() > 7 || schdle[2].toShort() > 23 || schdle[3].toShort() > 59 || schdle[4].toShort() < 10 || schdle[4].toShort() > 99)
    {
        schdle[1] = '1';
        schdle[2] = schdle[3] = '0';
        schdle[4] = "10";
        if(! cfgupdt) cfgupdt = true;
    }
    else if(schdle[1].toShort() == 0 && schdle[2].toShort() == 0 && schdle[3].toShort() < 30)
    {
        schdle[3] = "30";
        if(! cfgupdt) cfgupdt = true;
    }

    if(! like(schdle[5], QSL() << "_on_" << "_off_"))
    {
        schdle[5] = "off";
        if(! cfgupdt) cfgupdt = true;
    }

    if(! like(schdle[6], QSL() << "_topleft_" << "_topright_" << "_center_" << "_bottomleft_" << "_bottomright_"))
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

void sb::cfgwrite()
{
    crtfile("/etc/systemback.conf", "storagedir=" % sdir[0] % "\nliveworkdir=" % sdir[2] % "\npointsnumber=" % QStr::number(pnumber) % "\ntimer=" % schdle[0] % "\nschedule=" % schdle[1] % ":" % schdle[2] % ":" % schdle[3] % ":" % schdle[4] % "\nsilentmode=" % schdle[5] % "\nwindowposition=" % schdle[6] % '\n');
}

void sb::xrestart()
{
    ushort pid;
    if(pisrng("Xorg", pid)) kill(pid, SIGTERM);
}

bool sb::setpflag(QStr partition, QStr flag)
{
    if(partition.length() < 9) return false;
    ThrdType = Setpflag;
    ThrdStr[0] = partition;
    ThrdStr[1] = flag;
    SBThrd.start();
    thrdelay();
    return ThrdRslt;
}

bool sb::mkptable(QStr dev)
{
    if(dev.length() > 9) return false;
    ThrdType = Mkptable;
    ThrdStr[0] = dev;
    SBThrd.start();
    thrdelay();
    return ThrdRslt;
}

bool sb::mkpart(QStr dev, ullong start, ullong len)
{
    if(dev.length() > 9) return false;
    ThrdType = Mkpart;
    ThrdStr[0] = dev;
    ThrdLng[0] = start;
    ThrdLng[1] = len;
    SBThrd.start();
    thrdelay();
    return ThrdRslt;
}

bool sb::cpertime(QStr sourcepath, QStr destpath)
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

bool sb::cpfile(QStr srcfile, QStr newfile)
{
    struct stat fstat;
    if(stat(srcfile.toStdString().c_str(), &fstat) == -1) return false;

    if(SBThrd.isRunning())
    {
        int src, dst;
        if((src = open(srcfile.toStdString().c_str(), O_RDONLY | O_NOATIME)) == -1) return false;
        bool err;

        if(! (err = (dst = open(newfile.toStdString().c_str(), O_WRONLY | O_CREAT, fstat.st_mode)) == -1))
        {
            if(fstat.st_size > 0)
            {
                llong size(0);

                do {
                    llong csize(size);
                    if((size += sendfile(dst, src, NULL, fstat.st_size - size)) == csize) err = true;
                } while(! err && size < fstat.st_size);
            }

            close(dst);
        }

        close(src);
        if(err) return false;
    }
    else
    {
        ThrdType = Copy;
        ThrdStr[0] = srcfile;
        ThrdStr[1] = newfile;
        ThrdLng[0] = fstat.st_size;
        ThrdLng[1] = fstat.st_mode;
        SBThrd.start();
        thrdelay();
        if(! ThrdRslt) return false;
    }

    if(fstat.st_uid + fstat.st_gid > 0 && (chown(newfile.toStdString().c_str(), fstat.st_uid, fstat.st_gid) == -1 || (fstat.st_mode != (fstat.st_mode & ~(S_ISUID | S_ISGID)) && chmod(newfile.toStdString().c_str(), fstat.st_mode) == -1))) return false;
    struct utimbuf sftimes;
    sftimes.actime = fstat.st_atim.tv_sec;
    sftimes.modtime = fstat.st_mtim.tv_sec;
    return utime(newfile.toStdString().c_str(), &sftimes) == 0;
}

bool sb::lcomp(QStr link1, QStr link2)
{
    QStr lnk(rlink(link1));
    if(lnk != rlink(link2) || lnk.isEmpty()) return false;
    struct stat istat[2];
    if(ilike(-1, QSIL() << lstat(link1.toStdString().c_str(), &istat[0]) << lstat(link2.toStdString().c_str(), &istat[1]))) return false;
    if(istat[0].st_mtim.tv_sec != istat[1].st_mtim.tv_sec) return false;
    return true;
}

bool sb::cplink(QStr srclink, QStr newlink)
{
    if(! QFile::link(rlink(srclink), newlink)) return false;
    struct stat sistat;
    if(lstat(srclink.toStdString().c_str(), &sistat) == -1) return false;
    struct timeval sitimes[2];
    sitimes[0].tv_sec = sistat.st_atim.tv_sec;
    sitimes[1].tv_sec = sistat.st_mtim.tv_sec;
    sitimes[0].tv_usec = sitimes[1].tv_usec = 0;
    return lutimes(newlink.toStdString().c_str(), sitimes) == 0;
}

bool sb::cpdir(QStr srcdir, QStr newdir)
{
    struct stat dstat;
    if(stat(srcdir.toStdString().c_str(), &dstat) == -1 || mkdir(newdir.toStdString().c_str(), dstat.st_mode) == -1) return false;
    if(dstat.st_uid + dstat.st_gid > 0 && (chown(newdir.toStdString().c_str(), dstat.st_uid, dstat.st_gid) == -1 || (dstat.st_mode != (dstat.st_mode & ~(S_ISUID | S_ISGID)) && chmod(newdir.toStdString().c_str(), dstat.st_mode) == -1))) return false;
    struct utimbuf sdtimes;
    sdtimes.actime = dstat.st_atim.tv_sec;
    sdtimes.modtime = dstat.st_mtim.tv_sec;
    return utime(newdir.toStdString().c_str(), &sdtimes) == 0;
}

void sb::fssync()
{
    ThrdType = Sync;
    SBThrd.start();
    thrdelay();
}

ullong sb::devsize(QStr dev)
{
    ullong bsize;
    int odev;
    bool err(false);

    if((odev = open(dev.toStdString().c_str(), O_RDONLY)) != -1)
    {
        if(ioctl(odev, BLKGETSIZE64, &bsize) == -1) err = true;
        close(odev);
    }

    return err ? 0 : bsize;
}

bool sb::mcheck(QStr item)
{
    QStr mnts(fload("/proc/self/mounts"));
    if(item.contains(' ')) item = item.replace(" ", "\\040");

    if(item.startsWith("/dev/"))
        if(item.length() > 8)
        {
            if(QStr('\n' % mnts).contains('\n' % item % ' '))
                return true;
            else
            {
                blkid_probe pr(blkid_new_probe_from_filename(item.toStdString().c_str()));
                blkid_do_probe(pr);
                cchar *uuid("");
                blkid_probe_lookup_value(pr, "UUID", &uuid, NULL);
                blkid_free_probe(pr);
                return QStr(uuid).isEmpty() ? false : QStr('\n' % mnts).contains("\n/dev/disk/by-uuid/" % QStr(uuid) % ' ');
            }
        }
        else
            return QStr('\n' % mnts).contains('\n' % item);
    else if(item.endsWith('/') && item.length() > 1)
        return mnts.contains(' ' % left(item, -1));
    else
        return mnts.contains(' ' % item % ' ');
}

bool sb::mount(QStr dev, QStr mpoint, QStr moptns)
{
    ThrdType = Mount;
    ThrdStr[0] = dev;
    ThrdStr[1] = mpoint;
    ThrdStr[2] = moptns;
    SBThrd.start();
    thrdelay();
    return ThrdRslt;
}

bool sb::umount(QStr dev)
{
    ThrdType = Umount;
    ThrdStr[0] = dev;
    SBThrd.start();
    thrdelay();
    return ThrdRslt;
}

uchar sb::fcomp(QStr file1, QStr file2)
{
    struct stat fstat[2];
    if(ilike(-1, QSIL() << stat(file1.toStdString().c_str(), &fstat[0]) << stat(file2.toStdString().c_str(), &fstat[1]))) return false;
    if(fstat[0].st_size == fstat[1].st_size && fstat[0].st_mtim.tv_sec == fstat[1].st_mtim.tv_sec) return fstat[0].st_mode == fstat[1].st_mode && fstat[0].st_uid == fstat[1].st_uid && fstat[0].st_gid == fstat[1].st_gid ? 2 : 1;
    return 0;
}

bool sb::issmfs(QStr item1, QStr item2)
{
    struct stat istat[2];
    if(ilike(-1, QSIL() << stat(item1.toStdString().c_str(), &istat[0]) << stat(item2.toStdString().c_str(), &istat[1]))) return false;
    return istat[0].st_dev == istat[1].st_dev;
}

bool sb::execsrch(QStr fname, QStr ppath)
{
    QSL plst(QStr(getenv("PATH")).split(':'));
    if(! ppath.isEmpty()) ppath.append('/');

    for(uchar a(0) ; a < plst.count() ; ++a)
        if(isfile(ppath % plst.at(a) % '/' % fname)) return true;

    return false;
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
    else if(like(cmd, QSL() << "_tar -xf*" << "*--no-same-permissions_", All))
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
            cperc = mid(pout, rinstr(pout, "%") - 5, 2).toShort();
            if(Progress < cperc) Progress = cperc;
            break;
        }
        case 3:
            if(ThrdLng[0] == 0)
            {
                QStr itms(rodir(sdir[2] % "/.sblivesystemcreate"));
                QTS in(&itms, QIODevice::ReadOnly);

                while(! in.atEnd())
                {
                    QStr line(in.readLine()), item(right(line, - instr(line, "_")));
                    if(left(line, instr(line, "_") - 1).toShort() == Isfile) ThrdLng[0] += fsize(sdir[2] % "/.sblivesystemcreate/" % item);
                }
            }
            else if(isfile(ThrdStr[0]))
            {
                cperc = (fsize(ThrdStr[0]) * 100 + 50) / ThrdLng[0];
                if(Progress < cperc) Progress = cperc;
            }

            break;
        case 4:
            QStr itms(rodir(ThrdStr[0]));
            QTS in(&itms, QIODevice::ReadOnly);
            ullong size(0);

            while(! in.atEnd())
            {
                QStr line(in.readLine()), item(right(line, - instr(line, "_")));
                if(left(line, instr(line, "_") - 1).toShort() == Isfile) size += fsize(ThrdStr[0] % '/' % item);
            }

            cperc = (size * 100 + 50) / ThrdLng[0];
            if(Progress < cperc) Progress = cperc;
        }
    }

    return proc.exitStatus() == QProcess::CrashExit ? 255 : proc.exitCode();
}

uchar sb::exec(QSL cmds)
{
    for(uchar a(0) ; a < cmds.count() ; ++a)
    {
        uchar rev(exec(cmds.at(a)));
        if(rev > 0) return rev;
    }

    return 0;
}

bool sb::exclcheck(QSL elist, QStr item)
{
    for(ushort a(0) ; a < elist.count() ; ++a)
    {
        QStr excl(elist.at(a));

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
    bool rerun;

    do {
        rerun = false;

        for(uchar a(0) ; a < 15 ; ++a)
            if(! pnames[a].isEmpty()) pnames[a].clear();

        if(isdir(sdir[1]) && access(sdir[1], Write))
        {
            QSL dlst(QDir(sdir[1]).entryList(QDir::Dirs | QDir::NoDotAndDotDot));

            for(uchar a(0) ; a < dlst.count() ; ++a)
            {
                QStr iname(dlst.at(a));

                if(! islink(sdir[1] % '/' % iname) && ! iname.contains(' '))
                {
                    QStr pre(left(iname, 4));

                    if(pre.at(1).isDigit() && pre.at(2).isDigit() && pre.at(3) == '_')
                    {
                        if(pre.at(0) == 'S')
                        {
                            if(pre.at(1) == '0' || mid(pre, 2, 2) == "10") pnames[mid(pre, 2, 2).toShort() - 1] = right(iname, -4);
                        }
                        else if(pre.at(0) == 'H' && pre.at(1) == '0' && like(pre.at(2), QSL() << "_1_" << "_2_" << "_3_" << "_4_" << "_5_"))
                            pnames[9 + mid(pre, 3, 1).toShort()] = right(iname, -4);
                    }
                }
            }

            for(uchar a(14) ; a > 0 ; --a)
                if(a != 10 && ! pnames[a].isEmpty() && pnames[a - 1].isEmpty())
                {
                    QFile::rename(sdir[1] % (a > 10 ? QStr("/H0" % QStr::number(a - 9)) : QStr((a < 9 ? "/S0" : "/S") % QStr::number(a + 1))) % '_' % pnames[a], sdir[1] % (a > 10 ? QStr("/H0" % QStr::number(a - 10)) : "/S0" % QStr::number(a)) % '_' % pnames[a]);
                    if(! rerun) rerun = true;
                }
        }
    } while(rerun);
}

void sb::supgrade()
{
    exec("apt-get update");

    for(;;)
    {
        if(exec(QSL() << "apt-get install -fym --force-yes" << "dpkg --configure -a" << "apt-get dist-upgrade --no-install-recommends -ym --force-yes" << "apt-get autoremove --purge -y") == 0)
        {
            QSL dlst(QDir("/boot").entryList(QDir::Files));
            QStr rklist;

            for(short a(dlst.count() - 1) ; a > -1 ; --a)
            {
                QStr item(dlst.at(a));

                if(item.startsWith("vmlinuz-"))
                {
                    QStr vmlinuz(right(item, -8)), kernel(left(vmlinuz, instr(vmlinuz, "-") - 1)), kver(mid(vmlinuz, kernel.length() + 2, instr(vmlinuz, "-", kernel.length() + 2) - kernel.length() - 2));

                    if(isnum(kver) && vmlinuz.startsWith(kernel % '-' % kver % '-') && ! rklist.contains(kernel % '-' % kver % "-*"))
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

            uchar cproc(rklist.isEmpty() ? 0 : exec("apt-get autoremove --purge " % rklist));

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

                if(! iplist.isEmpty()) exec("dpkg --purge " % iplist);
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

        exec(QSL() << "tput reset" << "tput civis");

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

bool sb::pisrng(QStr pname)
{
    ushort pid;
    return pisrng(pname, pid);
}

bool sb::pisrng(QStr pname, ushort &pid)
{
    DIR *dir(opendir("/proc"));
    struct dirent *ent;

    while((ent = readdir(dir)))
        if(! like(ent->d_name, QSL() << "_._" << "_.._") && ent->d_type == DT_DIR && isnum(QStr(ent->d_name)) && islink("/proc/" % QStr(ent->d_name) % "/exe") && QFile::readLink("/proc/" % QStr(ent->d_name) % "/exe").endsWith('/' % pname))
        {
            pid = QStr(ent->d_name).toShort();
            closedir(dir);
            return true;
        }

    closedir(dir);
    return false;
}

QSL sb::readprttns()
{
    ThrdType = Readprttns;
    SBThrd.start();
    thrdelay();
    return ThrdSlst;
}

QSL sb::readlvprttns()
{
    ThrdType = Readlvprttns;
    SBThrd.start();
    thrdelay();
    return ThrdSlst;
}

bool sb::crtrpoint(QStr sdir, QStr pname)
{
    ThrdType = Crtrpoint;
    ThrdStr[0] = sdir;
    ThrdStr[1] = pname;
    SBThrd.start();
    thrdelay();
    return ThrdRslt;
}

bool sb::srestore(uchar mthd, QStr usr, QStr srcdir, QStr trgt, bool sfstab)
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

bool sb::scopy(uchar mthd, QStr usr, QStr srcdir)
{
    ThrdType = Scopy;
    ThrdChr = mthd;
    ThrdStr[0] = usr;
    ThrdStr[1] = srcdir;
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
    {
        int src, dst;

        if((ThrdRslt = (src = open(ThrdStr[0].toStdString().c_str(), O_RDONLY | O_NOATIME)) != -1))
        {
            if((ThrdRslt = (dst = open(ThrdStr[1].toStdString().c_str(), O_WRONLY | O_CREAT, ThrdLng[1])) != -1))
            {
                if(ThrdLng[0] > 0)
                {
                    ullong size(0);

                    do {
                        ullong csize(size);
                        if((size += sendfile(dst, src, NULL, ThrdLng[0] - size)) == csize) ThrdRslt = false;
                    } while(ThrdRslt && size < ThrdLng[0]);
                }

                close(dst);
            }

            close(src);
        }

        break;
    }
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

        ThrdRslt = (mnt_context_mount(mcxt) == 0);
        mnt_free_context(mcxt);
        break;
    }
    case Umount:
    {
        struct libmnt_context *ucxt(mnt_new_context());
        mnt_context_set_target(ucxt, ThrdStr[0].toStdString().c_str());
        mnt_context_enable_force(ucxt, true);
        mnt_context_enable_lazy(ucxt, true);
        ThrdRslt = (mnt_context_umount(ucxt) == 0);
        mnt_free_context(ucxt);
        break;
    }
    case Readprttns:
    {
        QSL dlst(QDir("/dev").entryList(QDir::System)), devs;

        for(ushort a(0) ; a < dlst.count() ; ++a)
        {
           QStr path("/dev/" % dlst.at(a));

           if(like(path, QSL() << "_/dev/sd*" << "_/dev/hd*"))
           {
               ullong size(devsize(path));

               if(size > 1048576 && size < 109951162777600)
               {
                   blkid_probe pr(blkid_new_probe_from_filename(path.toStdString().c_str()));
                   blkid_do_probe(pr);
                   cchar *uuid(""), *type("?"), *label("");

                   if(blkid_probe_lookup_value(pr, "UUID", &uuid, NULL) == 0)
                   {
                       blkid_probe_lookup_value(pr, "TYPE", &type, NULL);
                       blkid_probe_lookup_value(pr, "LABEL", &label, NULL);
                   }

                   blkid_free_probe(pr);
                   devs.append(path % '\n' % type % '\n' % label % '\n' % uuid % '\n' % QStr::number(size));
               }
           }
        }

        ThrdSlst = devs;
        break;
    }
    case Readlvprttns:
    {
        QSL dlst(QDir("/dev/disk/by-id").entryList(QDir::Files)), devs;

        for(uchar a(0) ; a < dlst.count() ; ++a)
        {
            QStr item(dlst.at(a));

            if(item.startsWith("usb-") && islink("/dev/disk/by-id/" % item))
            {
                QStr path(rlink("/dev/disk/by-id/" % item));

                if(! path.isEmpty())
                {
                    QStr dname(right(path, - rinstr(path, "/")));

                    if(dname.length() == 3 && like(dname, QSL() << "_sd*" << "_hd*"))
                    {
                        ullong size(devsize("/dev/" % dname));
                        if(size < 109951162777600) devs.append("/dev/" % dname % '\n' % mid(item, 5, rinstr(item, "_") - 5).replace('_', ' ') % '\n' % QStr::number(size));
                    }
                }
            }
        }

        devs.sort();
        ThrdSlst = devs;
        break;
    }
    case Ruuid:
    {
        blkid_probe pr(blkid_new_probe_from_filename(ThrdStr[0].toStdString().c_str()));
        blkid_do_probe(pr);
        cchar *uuid("");
        blkid_probe_lookup_value(pr, "UUID", &uuid, NULL);
        blkid_free_probe(pr);
        ThrdStr[1] = uuid;
        break;
    }
    case Setpflag:
        if(stype(ThrdStr[0]) == Isblock && stype(left(ThrdStr[0], 8)) == Isblock)
        {
            PedDevice *dev(ped_device_get(left(ThrdStr[0], 8).toStdString().c_str()));
            PedDisk *dsk(ped_disk_new(dev));
            PedPartition *prt(NULL);
            bool rv(false);

            while(! ThrdKill && (prt = ped_disk_next_partition(dsk, prt)))
                if(QStr(ped_partition_get_path(prt)) == ThrdStr[0])
                {
                    if(ped_partition_set_flag(prt, ped_partition_flag_get_by_name(ThrdStr[1].toStdString().c_str()), 1) == 1 && ped_disk_commit_to_dev(dsk) == 1) rv = true;
                    ped_disk_commit_to_os(dsk);
                }

            ped_disk_destroy(dsk);
            ped_device_destroy(dev);
            ThrdRslt = rv;
        }
        else
            ThrdRslt = false;

        break;
    case Mkptable:
        if(stype(ThrdStr[0]) == Isblock)
        {
            PedDevice *dev(ped_device_get(ThrdStr[0].toStdString().c_str()));
            PedDisk *dsk(ped_disk_new_fresh(dev, ped_disk_type_get("msdos")));
            bool rv(ped_disk_commit_to_dev(dsk) == 1 ? true : false);
            ped_disk_commit_to_os(dsk);
            ped_disk_destroy(dsk);
            ped_device_destroy(dev);
            ThrdRslt = rv;
        }
        else
            ThrdRslt = false;

        break;
    case Mkpart:
        if(stype(ThrdStr[0]) == Isblock)
        {
            PedDevice *dev(ped_device_get(ThrdStr[0].toStdString().c_str()));
            PedDisk *dsk(ped_disk_new(dev));
            PedPartition *prt(NULL);
            bool rv(false);

            while(! ThrdKill && (prt = ped_disk_next_partition(dsk, prt)))
                if(prt->type == PED_PARTITION_FREESPACE && prt->geom.length >= 2048)
                {
                    ullong ends(prt->next->type == PED_PARTITION_METADATA ? prt->next->geom.end : prt->geom.end);
                    PedPartition *crtprt(ped_partition_new(dsk, PED_PARTITION_NORMAL, ped_file_system_type_get("ext2"), ThrdLng[0] == 0 ? prt->geom.start : ThrdLng[0] / dev->sector_size, ThrdLng[1] == 0 || ThrdLng[1] / dev->sector_size > ends ? ends : ThrdLng[0] / dev->sector_size + ThrdLng[1] / dev->sector_size));
                    if(ped_disk_add_partition(dsk, crtprt, ped_constraint_exact(&crtprt->geom)) == 1 && ped_disk_commit_to_dev(dsk) == 1) rv = true;
                    ped_disk_commit_to_os(dsk);
                    break;
                }

            ped_disk_destroy(dsk);
            ped_device_destroy(dev);
            ThrdRslt = rv;
        }
        else
            ThrdRslt = false;

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
    DIR *dir(opendir(path.toStdString().c_str()));
    struct dirent *ent;

    while(! ThrdKill && (ent = readdir(dir)))
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

QStr sb::rodir(QStr path, bool hidden)
{
    sbdir(path, path.length(), hidden);
    QStr cdlst(odlst);
    odlst.clear();
    return cdlst;
}

void sb::sbdir(QStr path, uchar oplen, bool hidden)
{
    QStr prepath(odlst.isEmpty() ? NULL : right(path, - short(oplen == 1 ? 1 : oplen + 1)) + '/');
    DIR *dir(opendir(path.toStdString().c_str()));
    struct dirent *ent;

    while(! ThrdKill && (ent = readdir(dir)))
        if(! like(ent->d_name, QSL() << "_._" << "_.._") && (! hidden || QStr(ent->d_name).startsWith('.')))
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

    closedir(dir);
}

QSL sb::odir(QStr path, bool hidden)
{
    QSL dlst;
    DIR *dir(opendir(path.toStdString().c_str()));
    struct dirent *ent;

    while(! ThrdKill && (ent = readdir(dir)))
        if(! like(ent->d_name, QSL() << "_._" << "_.._") && (! hidden || QStr(ent->d_name).startsWith('.'))) dlst.append(ent->d_name);

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
    uint anum(0);
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
    if(! file.open(QIODevice::ReadOnly)) return false;

    while(! file.atEnd())
    {
        QStr cline(file.readLine().trimmed());
        if(cline.startsWith('.')) elist.append(cline);
        if(ThrdKill) return false;
    }

    file.close();
    QStr *cditms;
    uint cnum(0);
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

            QTS in(cditms, QIODevice::ReadOnly);

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

                                if(stype(sdir % '/' % cpname % "/home/" % usr % '/' % item) == Isfile && fcomp("/home/" % usr % '/' % item, sdir % '/' % cpname % "/home/" % usr % '/' % item) == 2)
                                {
                                    if(link(QStr(sdir % '/' % cpname % "/home/" % usr % '/' % item).toStdString().c_str(), QStr(trgt % "/home/" % usr % '/' % item).toStdString().c_str()) == -1) return false;
                                    goto nitem_1;
                                }

                                if(ThrdKill) return false;
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
                if(left(line, instr(line, "_") - 1).toShort() == Isdir && exist(trgt % "/home/" % usr % '/' % item) && ! cpertime("/home/" % usr % '/' % item, trgt % "/home/" % usr % '/' % item)) return false;
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
        QTS in(&rootitms, QIODevice::ReadOnly);

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

                            if(stype(sdir % '/' % cpname % "/root/" % item) == Isfile && fcomp("/root/" % item, sdir % '/' % cpname % "/root/" % item) == 2)
                            {
                                if(link(QStr(sdir % '/' % cpname % "/root/" % item).toStdString().c_str(), QStr(trgt % "/root/" % item).toStdString().c_str()) == -1) return false;
                                goto nitem_2;
                            }

                            if(ThrdKill) return false;
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
            if(left(line, instr(line, "_") - 1).toShort() == Isdir && exist(trgt % "/root/" % item) && ! cpertime("/root/" % item, trgt % "/root/" % item)) return false;
            if(ThrdKill) return false;
        }

        rootitms.clear();
        if(! cpertime("/root", trgt % "/root")) return false;
    }

    dlst = QDir("/").entryList(QDir::Files);

    for(uchar a(0) ; a < dlst.count() ; ++a)
    {
        QStr item(dlst.at(a));
        if(like(item, QSL() << "_initrd.img*" << "_vmlinuz*") && islink('/' % item) && ! cplink('/' % item, trgt % '/' % item)) return false;
        if(ThrdKill) return false;
    }

    dlst = QSL() << "/cdrom" << "/dev" << "/mnt" << "/proc" << "/run" << "/sys" << "/tmp";

    for(uchar a(0) ; a < dlst.count() ; ++a)
    {
        QStr cdir(dlst.at(a));
        if(isdir(cdir) && ! cpdir(cdir, trgt % cdir)) return false;
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

            QTS in(cditms, QIODevice::ReadOnly);

            while(! in.atEnd())
            {
                QStr line(in.readLine()), item(right(line, - instr(line, "_")));
                ++cnum;
                cperc = (cnum * 100 + 50) / anum;
                if(Progress < cperc) Progress = cperc;
                ThrdDbg = '@' % cdir % '/' % item;

                if(! like(cdir % '/' % item, QSL() << "+_/var/cache/apt/*" << "-*.bin_" << "-*.bin.*", Mixed) && ! like(cdir % '/' % item, QSL() << "_/var/cache/apt/archives/*" << "*.deb_", All) && ! like(item, QSL() << "_lost+found_" << "_lost+found/*" << "*/lost+found_" << "*/lost+found/*" << "_Systemback_" << "_Systemback/*" << "*/Systemback_" << "*/Systemback/*" << "*.dpkg-old_" << "*~_" << "*~/*") && ! exclcheck(elist, QStr(cdir % '/' % item)) && exist(cdir % '/' % item))
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

                            if(stype(sdir % '/' % cpname % cdir % '/' % item) == Isfile && fcomp(cdir % '/' % item, sdir % '/' % cpname % cdir % '/' % item) == 2)
                            {
                                if(link(QStr(sdir % '/' % cpname % cdir % '/' % item).toStdString().c_str(), QStr(trgt % cdir % '/' % item).toStdString().c_str()) == -1) return false;
                                goto nitem_3;
                            }

                            if(ThrdKill) return false;
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
                if(left(line, instr(line, "_") - 1).toShort() == Isdir && exist(trgt % cdir % '/' % item) && ! cpertime(cdir % '/' % item, trgt % cdir % '/' % item)) return false;
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
            if(! file.open(QIODevice::ReadOnly)) return false;

            for(uchar a(0) ; a < dlst.count() ; ++a)
            {
                QStr item(dlst.at(a));
                if(a > 0 && ! file.open(QIODevice::ReadOnly)) return false;

                while(! file.atEnd())
                {
                    QStr cline(file.readLine().trimmed().replace('\t', ' '));

                    if(! cline.startsWith("#") && like(cline.replace("\\040", " "), QSL() << "* /media/" % item % " *" << "* /media/" % item % "/*"))
                    {
                        QStr fdir;
                        QSL cdlst(QStr(mid(cline, instr(cline, "/media/") + 7, instr(cline, " ", instr(cline, "/media/")) - instr(cline, "/media/") - 7)).split('/'));

                        for(uchar b(0) ; b < cdlst.count() ; ++b)
                        {
                            QStr cdname(cdlst.at(b));

                            if(! cdname.isEmpty())
                            {
                                fdir.append('/' % cdname.replace("\\040", " "));
                                if(! isdir(trgt % "/media" % fdir) && ! cpdir("/media" % fdir, trgt % "/media" % fdir)) return false;
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
        QTS in(&logitms, QIODevice::ReadOnly);

        while(! in.atEnd())
        {
            QStr line(in.readLine()), item(right(line, - instr(line, "_")));
            ThrdDbg = "@/var/log/" % item;

            switch(left(line, instr(line, "_") - 1).toShort()) {
            case Isdir:
                if(! cpdir("/var/log/" % item, trgt % "/var/log/" % item)) return false;
                break;
            case Isfile:
                if(! like(item, QSL() << "*.gz_" << "*.old_") && (! item.contains('.') || ! isnum(right(item, - rinstr(item, ".")))))
                {
                    crtfile(trgt % "/var/log/" % item);
                    if(! cpertime("/var/log/" % item, trgt % "/var/log/" % item)) return false;
                }
            }

            if(ThrdKill) return false;
        }

        in.setString(&logitms, QIODevice::ReadOnly);

        while(! in.atEnd())
        {
            QStr line(in.readLine()), item(right(line, - instr(line, "_")));
            ThrdDbg = "@/var/log/" % item;
            if(left(line, instr(line, "_") - 1).toShort() == Isdir && exist(trgt % "/var/log/" % item) && ! cpertime("/var/log/" % item, trgt % "/var/log/" % item)) return false;
            if(ThrdKill) return false;
        }

        if(! cpertime("/var/log", trgt % "/var/log")) return false;
        if(! cpertime("/var", trgt % "/var")) return false;
    }

    ThrdDbg.clear();
    return true;
}

bool sb::fspchk(QStr &dir)
{
    return dfree(dir.isEmpty() ? "/" : dir) > 10485760;
}

bool sb::thrdsrestore(uchar &mthd, QStr &usr, QStr &srcdir, QStr &trgt, bool &sfstab)
{
    QSL usrs;
    QStr home1itms, home2itms, home3itms, home4itms, home5itms, rootitms, binitms, bootitms, etcitms, libitms, lib32itms, lib64itms, optitms, sbinitms, selinuxitms, srvitms, usritms, varitms;
    uint anum(0);

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
            if(ThrdKill) return false;
        }
        else if(isdir(srcdir % "/home/" % usr))
        {
            usrs.append(usr);
            home1itms = rodir(srcdir % "/home/" % usr, true);
            if(ThrdKill) return false;
        }

        anum += home1itms.count('\n') + home2itms.count('\n') + home3itms.count('\n') + home4itms.count('\n') + home5itms.count('\n') + rootitms.count('\n');
    }

    QStr *cditms;
    uint cnum(0);
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
            if(like(item, QSL() << "_initrd.img*" << "_vmlinuz*") && ! islink(trgt % '/' % item) && ! exist(srcdir % '/' % item) && ! lcomp(srcdir % '/' % item, trgt % '/' % item)) QFile::remove(trgt % '/' % item);
            if(ThrdKill) return false;
        }

        dlst = QDir(srcdir).entryList(QDir::Files);

        for(uchar a(0) ; a < dlst.count() ; ++a)
        {
            QStr item(dlst.at(a));
            if(like(item, QSL() << "_initrd.img*" << "_vmlinuz*") && ! exist(trgt % '/' % item)) cplink(srcdir % '/' % item, trgt % '/' % item);
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
                    if(! cpdir(srcdir % cdir, trgt % cdir) && ! fspchk(trgt)) return false;
                }
            }
            else if(exist(trgt % cdir))
                stype(trgt % cdir) == Isdir ? recrmdir(trgt % cdir) : QFile::remove(trgt % cdir);

            if(ThrdKill) return false;
        }

        QSL elist;
        if(trgt.isEmpty()) elist = QSL() << "/etc/mtab" << "/var/cache/fontconfig/" << "/var/lib/dpkg/lock" << "/var/lib/udisks/mtab" << "/var/run/" << "/var/tmp/";
        if(trgt.isEmpty() || (isfile("/mnt/etc/xdg/autostart/sbschedule.desktop") && isfile("/mnt/etc/xdg/autostart/sbschedule-kde.desktop") && isfile("/mnt/usr/bin/systemback") && isfile("/mnt/usr/lib/systemback/libsystemback.so.1.0.0") && isfile("/mnt/usr/lib/systemback/sbscheduler") && isfile("/mnt/usr/lib/systemback/sbsustart") && isfile("/mnt/usr/lib/systemback/sbsysupgrade")&& isdir("/mnt/usr/share/systemback/lang") && isfile("/mnt/usr/share/systemback/efi.tar.gz") && isfile("/mnt/usr/share/systemback/splash.png") && isfile("/mnt/var/lib/dpkg/info/systemback.list") && isfile("/mnt/var/lib/dpkg/info/systemback.md5sums"))) elist.append(QSL() << "/etc/systemback*" << "/etc/xdg/autostart/sbschedule*" << "systemback*" << "/usr/lib/systemback/" << "/usr/share/systemback/" << "/var/lib/dpkg/info/systemback*");
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
                        if(! like(item, QSL() << "_lost+found_" << "_Systemback_") && ! exclcheck(elist, QStr(cdir % '/' % item)) && ! exist(srcdir % cdir % '/' % item)) stype(trgt % cdir % '/' % item) == Isdir ? recrmdir(trgt % cdir % '/' % item) : QFile::remove(trgt % cdir % '/' % item);
                        if(ThrdKill) return false;
                    }
                }
                else
                {
                    if(exist(trgt % cdir)) QFile::remove(trgt % cdir);
                    if(! QDir().mkdir(trgt % cdir) && ! fspchk(trgt)) return false;
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

                            if(! cplink(srcdir % cdir % '/' % item, trgt % cdir % '/' % item) && ! fspchk(trgt)) return false;
                            break;
                        case Isdir:
                            switch(stype(trgt % cdir % '/' % item)) {
                            case Isdir:
                            {
                                QSL sdlst(odir(trgt % cdir % '/' % item));

                                for(ushort b(0) ; b < sdlst.count() ; ++b)
                                {
                                    QStr sitem(sdlst.at(b));
                                    if(! like(sitem, QSL() << "_lost+found_" << "_Systemback_") && ! exclcheck(elist, QStr(cdir % '/' % item % '/' % sitem)) && ! exist(srcdir % cdir % '/' % item % '/' % sitem)) stype(trgt % cdir % '/' % item % '/' % sitem) == Isdir ? recrmdir(trgt % cdir % '/' % item % '/' % sitem) : QFile::remove(trgt % cdir % '/' % item % '/' % sitem);
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

                nitem_1:;
                    if(ThrdKill) return false;
                }

                in.setString(cditms, QIODevice::ReadOnly);

                while(! in.atEnd())
                {
                    QStr line(in.readLine()), item(right(line, - instr(line, "_")));
                    if(left(line, instr(line, "_") - 1).toShort() == Isdir && exist(trgt % cdir % '/' % item)) cpertime(srcdir % cdir % '/' % item, trgt % cdir % '/' % item);
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
                if(! QDir().mkdir(trgt % "/media") && ! fspchk(trgt)) return false;
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
            QStr sbitms(rodir(srcdir % "/.systemback"));
            QTS in(&sbitms, QIODevice::ReadOnly);

            while(! in.atEnd())
            {
                QStr line(in.readLine()), item(right(line, - instr(line, "_")));

                switch(left(line, instr(line, "_") - 1).toShort()) {
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
        if(! file.open(QIODevice::ReadOnly)) return false;

        while(! file.atEnd())
        {
            QStr cline(file.readLine().trimmed());
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
                        skppd = (QFile(srcdir % "/root/" % item).size() > 8000000);

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

            nitem_2:;
                if(ThrdKill) return false;
            }

            in.setString(&rootitms, QIODevice::ReadOnly);

            while(! in.atEnd())
            {
                QStr line(in.readLine()), item(right(line, - instr(line, "_")));
                if(left(line, instr(line, "_") - 1).toShort() == Isdir && exist(trgt % "/root/" % item)) cpertime(srcdir % "/root/" % item, trgt % "/root/" % item);
                if(ThrdKill) return false;
            }

            rootitms.clear();
        }

        for(uchar a(0) ; a < usrs.count() ; ++a)
        {
            QStr usr(usrs.at(a));

            if(! isdir(trgt % "/home/" % usr))
            {
                if(exist(trgt % "/home/" % usr)) QFile::remove(trgt % "/home/" % usr);
                if(! QDir().mkdir(trgt % "/home/" % usr) && ! fspchk(trgt)) return false;
            }
            else if(! ilike(mthd, QSIL() << 3 << 4))
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
                            if(QFile(trgt % "/home/" % usr % '/' % item).size() > 8000000) break;
                        case Islink:
                            QFile::remove(trgt % "/home/" % usr % '/' % item);
                        }
                    }

                    if(ThrdKill) return false;
                }
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

            QTS in(cditms, QIODevice::ReadOnly);

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
                        skppd = (QFile(srcdir % "/home/" % usr % '/' % item).size() > 8000000);

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

            nitem_3:;
                if(ThrdKill) return false;
            }

            in.setString(cditms, QIODevice::ReadOnly);

            while(! in.atEnd())
            {
                QStr line(in.readLine()), item(right(line, - instr(line, "_")));
                if(left(line, instr(line, "_") - 1).toShort() == Isdir && exist(trgt % "/home/" % usr % '/' % item)) cpertime(srcdir % "/home/" % usr % '/' % item, trgt % "/home/" % usr % '/' % item);
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
    if(isdir(srcdir % "/root")) rootitms = mthd == 5 ? rodir(srcdir % "/etc/skel") : rodir(srcdir % "/root", true);
    if(ThrdKill) return false;
    uint anum(binitms.count('\n') + bootitms.count('\n') + etcitms.count('\n') + libitms.count('\n') + lib32itms.count('\n') + lib64itms.count('\n') + optitms.count('\n') + sbinitms.count('\n') + selinuxitms.count('\n') + srvitms.count('\n') + usritms.count('\n') + varitms.count('\n') + rootitms.count('\n'));

    if(mthd > 0)
    {
        if(isdir(srcdir % "/home"))
        {
            if(ilike(mthd, QSIL() << 1 << 2))
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
                    home1itms = rodir(srcdir % "/home/" % usr);
                    break;
                case 5:
                    home1itms = rootitms;
                }

                if(ThrdKill) return false;
            }
        }

        anum += home1itms.count('\n') + home2itms.count('\n') + home3itms.count('\n') + home4itms.count('\n') + home5itms.count('\n');
    }

    Progress = 0;
    QStr *cditms, macid;
    uint cnum(0);
    uchar cperc;
    QSL elist(QSL() << ".cache/gvfs" << ".gvfs" << ".local/share/Trash/files/" << ".local/share/Trash/info/" << ".Xauthority" << ".ICEauthority");
    QFile file(srcdir % "/etc/systemback.excludes");
    if(! file.open(QIODevice::ReadOnly)) return false;

    while(! file.atEnd())
    {
        QStr cline(file.readLine().trimmed());
        if(cline.startsWith('.')) elist.append(cline);
        if(ThrdKill) return false;
    }

    file.close();

    if(mthd > 2)
    {
        QStr mid(isfile(srcdir % "/var/lib/dbus/machine-id") ? "/var/lib/dbus/machine-id" : isfile(srcdir % "/etc/machine-id") ? "/etc/machine-id" : NULL);

        if(! mid.isEmpty())
        {
            file.setFileName(srcdir % mid);
            if(! file.open(QIODevice::ReadOnly)) return false;
            QStr line(file.readLine().trimmed());
            if(line.length() == 32) macid = line;
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

        if(mthd > 0)
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
                    homeitms = mthd == 1 ? rodir(srcdir % "/home/" % usr) : rodir(srcdir % "/home/" % usr, true);
                    cditms = &homeitms;
                }

                QTS in(cditms, QIODevice::ReadOnly);

                while(! in.atEnd())
                {
                    QStr line(in.readLine()), item(right(line, - instr(line, "_")));
                    ++cnum;
                    cperc = (cnum * 100 + 50) / anum;
                    if(Progress < cperc) Progress = cperc;
                    ThrdDbg = (mthd == 5 ? "@/etc/skel/" : QStr("@/home/" % usr % '/')) % item;

                    if((mthd == 5 || (! like(item, QSL() << "_lost+found_" << "_lost+found/*" << "*/lost+found_" << "*/lost+found/*" << "_Systemback_" << "_Systemback/*" << "*/Systemback_" << "*/Systemback/*" << "*~_" << "*~/*") && ! exclcheck(elist, item) && (macid.isEmpty() || ! item.contains(macid)))) && (! srcdir.isEmpty() || exist((mthd == 5 ? "/etc/skel/" : QStr("/home/" % usr % '/')) % item)))
                    {
                        switch(left(line, instr(line, "_") - 1).toShort()) {
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

                            if(! cplink(srcdir % (mthd == 5 ? "/etc/skel/" : QStr("/home/" % usr % '/')) % item, "/.sbsystemcopy/home/" % usr % '/' % item)) return false;
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
                            skppd = ilike(mthd, QSIL() << 2 << 3) ? (QFile(srcdir % "/home/" % usr % '/' % item).size() > 8000000) : false;

                            switch(stype("/.sbsystemcopy/home/" % usr % '/' % item)) {
                            case Isfile:
                                if(mthd == 5)
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
                                if(! cpfile(srcdir % "/etc/skel/" % item, "/.sbsystemcopy/home/" % usr % '/' % item)) return false;
                            }
                            else if(! skppd && ! cpfile(srcdir % "/home/" % usr % '/' % item, "/.sbsystemcopy/home/" % usr % '/' % item))
                                return false;
                        }
                    }

                nitem_1:;
                    if(ThrdKill) return false;
                }

                in.setString(cditms, QIODevice::ReadOnly);

                while(! in.atEnd())
                {
                    QStr line(in.readLine()), item(right(line, - instr(line, "_")));
                    ThrdDbg = (mthd == 5 ? "@/etc/skel/" : QStr("@/home/" % usr % '/')) % item;
                    if(left(line, instr(line, "_") - 1).toShort() == Isdir && exist("/.sbsystemcopy/home/" % usr % '/' % item) && ! cpertime(srcdir % (mthd == 5 ? "/etc/skel/" : QStr("/home/" % usr % '/')) % item, "/.sbsystemcopy/home/" % usr % '/' % item)) return false;
                    if(ThrdKill) return false;
                }

                cditms->clear();

                if(mthd > 1 && isfile(srcdir % "/home/" % usr % "/.config/user-dirs.dirs"))
                {
                    QFile file(srcdir % "/home/" % usr % "/.config/user-dirs.dirs");
                    if(! file.open(QIODevice::ReadOnly)) return false;

                    while(! file.atEnd())
                    {
                        QStr cline(file.readLine().trimmed());

                        if(! cline.startsWith('#') && cline.contains("$HOME"))
                        {
                            QStr dir(left(right(cline, - instr(cline, "/")), -1));
                            if(isdir(srcdir % "/home/" % usr % '/' % dir)) cpdir(srcdir % "/home/" % usr % '/' % dir, "/.sbsystemcopy/home/" % usr % '/' % dir);
                        }

                        if(ThrdKill) return false;
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

        QTS in(&rootitms, QIODevice::ReadOnly);

        while(! in.atEnd())
        {
            QStr line(in.readLine()), item(right(line, - instr(line, "_")));
            ++cnum;
            cperc = (cnum * 100 + 50) / anum;
            if(Progress < cperc) Progress = cperc;
            ThrdDbg = (mthd == 5 ? "@/etc/skel/" : "@/root/") % item;

            if((mthd == 5 || (! like(item, QSL() << "_lost+found_" << "_lost+found/*" << "*/lost+found_" << "*/lost+found/*" << "_Systemback_" << "_Systemback/*" << "*/Systemback_" << "*/Systemback/*" << "*~_" << "*~/*") && ! exclcheck(elist, item) && (macid.isEmpty() || ! item.contains(macid)))) && (! srcdir.isEmpty() || exist((mthd == 5 ? "/etc/skel/" : "/root/") % item)))
            {
                switch(left(line, instr(line, "_") - 1).toShort()) {
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

                    if(! cplink(srcdir % (mthd == 5 ? "/etc/skel/" : "/root/") % item, "/.sbsystemcopy/root/" % item)) return false;
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
                    skppd = ilike(mthd, QSIL() << 2 << 3) ? (QFile(srcdir % "/root/" % item).size() > 8000000) : false;

                    switch(stype("/.sbsystemcopy/root/" % item)) {
                    case Isfile:
                        if(mthd == 5)
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
                        if(! cpfile(srcdir % "/etc/skel/" % item, "/.sbsystemcopy/root/" % item)) return false;
                    }
                    else if(! skppd && ! cpfile(srcdir % "/root/" % item, "/.sbsystemcopy/root/" % item))
                        return false;
                }
            }

        nitem_2:;
            if(ThrdKill) return false;
        }

        in.setString(&rootitms, QIODevice::ReadOnly);

        while(! in.atEnd())
        {
            QStr line(in.readLine()), item(right(line, - instr(line, "_")));
            ThrdDbg = (mthd == 5 ? "@/etc/skel/" : "@/root/") % item;
            if(left(line, instr(line, "_") - 1).toShort() == Isdir && exist("/.sbsystemcopy/root/" % item) && ! cpertime(srcdir % (mthd == 5 ? "/etc/skel/" : "/root/") % item, "/.sbsystemcopy/root/" % item)) return false;
            if(ThrdKill) return false;
        }

        if(! cpertime(srcdir % "/root", "/.sbsystemcopy/root")) return false;
        rootitms.clear();
    }

    QSL dlst(QDir("/.sbsystemcopy").entryList(QDir::Files));

    for(uchar a(0) ; a < dlst.count() ; ++a)
    {
        QStr item(dlst.at(a));
        if(like(item, QSL() << "_initrd.img*" << "_vmlinuz*") && ! islink("/.sbsystemcopy/" % item) && ! islink(srcdir % '/' % item) && ! lcomp(srcdir % '/' % item, "/.sbsystemcopy/" % item) && ! QFile::remove("/.sbsystemcopy/" % item)) return false;
        if(ThrdKill) return false;
    }

    dlst = QDir(srcdir.isEmpty() ? "/" : srcdir).entryList(QDir::Files);

    for(uchar a(0) ; a < dlst.count() ; ++a)
    {
        QStr item(dlst.at(a));
        if(like(item, QSL() << "_initrd.img*" << "_vmlinuz*") && islink(srcdir % '/' % item) && ! exist("/.sbsystemcopy/" % item) && ! cplink(srcdir % '/' % item, "/.sbsystemcopy/" % item)) return false;
        if(ThrdKill) return false;
    }

    dlst = QSL() << "/cdrom" << "/dev" << "/home" << "/mnt" << "/root" << "/proc" << "/run" << "/sys" << "/tmp";

    for(uchar a(0) ; a < dlst.count() ; ++a)
    {
        QStr cdir(dlst.at(a));

        if(isdir(srcdir % cdir))
        {
            if(! isdir("/.sbsystemcopy" % cdir))
            {
                if(exist("/.sbsystemcopy" % cdir)) QFile::remove("/.sbsystemcopy" % cdir);
                if(! cpdir(srcdir % cdir, "/.sbsystemcopy" % cdir)) return false;
            }
            else if(! cpertime(srcdir % cdir, "/.sbsystemcopy" % cdir))
                return false;
        }
        else if(exist("/.sbsystemcopy" % cdir))
            stype("/.sbsystemcopy" % cdir) == Isdir ? recrmdir("/.sbsystemcopy" % cdir) : QFile::remove("/.sbsystemcopy" % cdir);

        if(ThrdKill) return false;
    }

    elist = QSL() << "/etc/mtab" << "/var/cache/fontconfig/" << "/var/lib/dpkg/lock" << "/var/lib/udisks/mtab" << "/var/log" << "/var/run/" << "/var/tmp/";
    if(mthd > 2) elist.append(QSL() << "/etc/machine-id" << "/etc/systemback.conf" << "/etc/systemback.excludes" << "/var/lib/dbus/machine-id");
    if(srcdir == "/.systembacklivepoint" && fload("/proc/cmdline").contains("noxconf")) elist.append("/etc/X11/xorg.conf");
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
                    if(! like(item, QSL() << "_lost+found_" << "_Systemback_") && ! exist(srcdir % cdir % '/' % item)) stype("/.sbsystemcopy" % cdir % '/' % item) == Isdir ? recrmdir("/.sbsystemcopy" % cdir % '/' % item) : QFile::remove("/.sbsystemcopy" % cdir % '/' % item);
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

                if(! like(item, QSL() << "_lost+found_" << "_lost+found/*" << "*/lost+found_" << "*/lost+found/*" << "_Systemback_" << "_Systemback/*" << "*/Systemback_" << "*/Systemback/*") && ! exclcheck(elist, QStr(cdir % '/' % item)) && (macid.isEmpty() || ! item.contains(macid)) && (mthd < 3 || ! like(cdir % '/' % item, QSL() << "_/etc/udev/rules.d*" << "*-persistent-*", All)) && (! srcdir.isEmpty() || exist(cdir % '/' % item)))
                {
                    switch(left(line, instr(line, "_") - 1).toShort()) {
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
                                if(! like(sitem, QSL() << "_lost+found_" << "_Systemback_") && (! exist(srcdir % cdir % '/' % item % '/' % sitem) || cdir % '/' % item % '/' % sitem == "/etc/X11/xorg.conf")) stype("/.sbsystemcopy" % cdir % '/' % item % '/' % sitem) == Isdir ? recrmdir("/.sbsystemcopy" % cdir % '/' % item % '/' % sitem) : QFile::remove("/.sbsystemcopy" % cdir % '/' % item % '/' % sitem);
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
                        case Islink:
                            QFile::remove("/.sbsystemcopy" % cdir % '/' % item);
                            break;
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
                if(left(line, instr(line, "_") - 1).toShort() == Isdir && exist("/.sbsystemcopy" % cdir % '/' % item) && ! cpertime(srcdir % cdir % '/' % item, "/.sbsystemcopy" % cdir % '/' % item)) return false;
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
                if(! file.open(QIODevice::ReadOnly)) return false;

                for(uchar a(0) ; a < dlst.count() ; ++a)
                {
                    QStr item(dlst.at(a));
                    if(a > 0 && ! file.open(QIODevice::ReadOnly)) return false;

                    while(! file.atEnd())
                    {
                        QStr cline(file.readLine().trimmed().replace('\t', ' '));

                        if(! cline.startsWith("#") && like(cline.replace("\\040", " "), QSL() << "* /media/" % item % " *" << "* /media/" % item % "/*"))
                        {
                            QStr fdir;
                            QSL cdlst(QStr(mid(cline, instr(cline, "/media/") + 7, instr(cline, " ", instr(cline, "/media/")) - instr(cline, "/media/") - 7)).split('/'));

                            for(uchar b(0) ; b < cdlst.count() ; ++b)
                            {
                                QStr cdname(cdlst.at(b));

                                if(! cdname.isEmpty())
                                {
                                    fdir.append('/' % cdname.replace("\\040", " "));
                                    if(! isdir("/.sbsystemcopy/media" % fdir) && ! cpdir("/media" % fdir, "/.sbsystemcopy/media" % fdir)) return false;
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
                else if(! cpdir(srcdir % "/media/" % item, "/.sbsystemcopy/media/" % item))
                    return false;

                if(ThrdKill) return false;
            }
        }

        if(! cpertime(srcdir % "/media", "/.sbsystemcopy/media")) return false;
    }
    else if(exist("/.sbsystemcopy/media"))
        stype("/.sbsystemcopy/media") == Isdir ? recrmdir("/.sbsystemcopy/media") : QFile::remove("/.sbsystemcopy/media");

    if(exist("/.sbsystemcopy/var/log")) stype("/.sbsystemcopy/var/log") == Isdir ? recrmdir("/.sbsystemcopy/var/log") : QFile::remove("/.sbsystemcopy/var/log");

    if(isdir(srcdir % "/var/log"))
    {
        if(! QDir().mkdir("/.sbsystemcopy/var/log")) return false;
        QStr logitms(rodir(srcdir % "/var/log"));
        QTS in(&logitms, QIODevice::ReadOnly);

        while(! in.atEnd())
        {
            QStr line(in.readLine()), item(right(line, - instr(line, "_")));
            ThrdDbg = "@/var/log/" % item;

            switch(left(line, instr(line, "_") - 1).toShort()) {
            case Isdir:
                if(! cpdir(srcdir % "/var/log/" % item, "/.sbsystemcopy/var/log/" % item)) return false;
                break;
            case Isfile:
                if(! like(item, QSL() << "*.gz_" << "*.old_") && (! item.contains('.') || ! isnum(right(item, - rinstr(item, ".")))))
                {
                    crtfile("/.sbsystemcopy/var/log/" % item);
                    if(! cpertime(srcdir % "/var/log/" % item, "/.sbsystemcopy/var/log/" % item)) return false;
                }
            }

            if(ThrdKill) return false;
        }

        in.setString(&logitms, QIODevice::ReadOnly);

        while(! in.atEnd())
        {
            QStr line(in.readLine()), item(right(line, - instr(line, "_")));
            ThrdDbg = "@/var/log/" % item;
            if(left(line, instr(line, "_") - 1).toShort() == Isdir && ! cpertime(srcdir % "/var/log/" % item, "/.sbsystemcopy/var/log/" % item)) return false;
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
                if(item != "etc/fstab" && ! cplink("/.systembacklivepoint/.systemback/" % item, "/.sbsystemcopy/" % item)) return false;
                break;
            case Isfile:
                if(item != "etc/fstab" && ! cpfile("/.systembacklivepoint/.systemback/" % item, "/.sbsystemcopy/" % item)) return false;
            }

            if(ThrdKill) return false;
        }

        in.setString(&sbitms, QIODevice::ReadOnly);

        while(! in.atEnd())
        {
            QStr line(in.readLine()), item(right(line, - instr(line, "_")));
            ThrdDbg = "@/.systemback/" % item;
            if(left(line, instr(line, "_") - 1).toShort() == Isdir && ! cpertime("/.systembacklivepoint/.systemback/" % item, "/.sbsystemcopy/" % item)) return false;
            if(ThrdKill) return false;
        }
    }

    ThrdDbg.clear();
    return true;
}

bool sb::thrdlvprpr(bool &iudata)
{
    if(ThrdLng[0] > 0) ThrdLng[0] = 0;
    QStr sitms;
    QSL dlst(QSL() << "/bin" << "/boot" << "/etc" << "/lib" << "/lib32" << "/lib64" << "/opt" << "/sbin" << "/selinux" << "/usr" << "/initrd.img" << "/initrd.img.old" << "/vmlinuz" << "/vmlinuz.old");

    for(uchar a(0) ; a < dlst.count() ; ++a)
    {
        QStr item(dlst.at(a));

        if(isdir(item))
        {
            sitms.append(rodir(item));
            ++ThrdLng[0];
        }

        if(ThrdKill) return false;
    }

    ThrdLng[0] += sitms.count('\n');
    sitms.clear();
    if(! QDir().mkdir(sdir[2] % "/.sblivesystemcreate/.systemback")) return false;
    if(! QDir().mkdir(sdir[2] % "/.sblivesystemcreate/.systemback/etc")) return false;

    if(isdir("/etc/udev"))
    {
        if(! QDir().mkdir(sdir[2] % "/.sblivesystemcreate/.systemback/etc/udev")) return false;

        if(isdir("/etc/udev/rules.d"))
        {
            if(! QDir().mkdir(sdir[2] % "/.sblivesystemcreate/.systemback/etc/udev/rules.d")) return false;
            if(isfile("/etc/udev/rules.d/70-persistent-cd.rules") && ! cpfile("/etc/udev/rules.d/70-persistent-cd.rules", sdir[2] % "/.sblivesystemcreate/.systemback/etc/udev/rules.d/70-persistent-cd.rules")) return false;
            if(isfile("/etc/udev/rules.d/70-persistent-net.rules") && ! cpfile("/etc/udev/rules.d/70-persistent-net.rules", sdir[2] % "/.sblivesystemcreate/.systemback/etc/udev/rules.d/70-persistent-net.rules")) return false;
            if(! cpertime("/etc/udev/rules.d", sdir[2] % "/.sblivesystemcreate/.systemback/etc/udev/rules.d")) return false;
        }

        if(! cpertime("/etc/udev", sdir[2] % "/.sblivesystemcreate/.systemback/etc/udev")) return false;
    }

    if(isfile("/etc/fstab") && ! cpfile("/etc/fstab", sdir[2] % "/.sblivesystemcreate/.systemback/etc/fstab")) return false;
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
            ++ThrdLng[0];
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
    ++ThrdLng[0];
    if(ThrdKill) return false;

    if(isfile("/etc/fstab"))
    {
        QSL dlst(QDir("/media").entryList(QDir::Dirs | QDir::NoDotAndDotDot));
        QFile file("/etc/fstab");
        if(! file.open(QIODevice::ReadOnly)) return false;

        for(uchar a(0) ; a < dlst.count() ; ++a)
        {
            QStr item(dlst.at(a));
            if(a > 0 && ! file.open(QIODevice::ReadOnly)) return false;

            while(! file.atEnd())
            {
                QStr cline(file.readLine().trimmed().replace('\t', ' '));

                if(! cline.startsWith("#") && like(cline.replace("\\040", " "), QSL() << "* /media/" % item % " *" << "* /media/" % item % "/*"))
                {
                    QStr fdir;
                    QSL cdlst(QStr(mid(cline, instr(cline, "/media/") + 7, instr(cline, " ", instr(cline, "/media/")) - instr(cline, "/media/") - 7)).split('/'));

                    for(uchar b(0) ; b < cdlst.count() ; ++b)
                    {
                        QStr cdname(cdlst.at(b));

                        if(! cdname.isEmpty())
                        {
                            fdir.append('/' % cdname.replace("\\040", " "));

                            if(! isdir("/media/.sblvtmp/media" % fdir))
                            {
                                if(! cpdir("/media" % fdir, "/media/.sblvtmp/media" % fdir)) return false;
                                ++ThrdLng[0];
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
    ++ThrdLng[0];
    QSL elist(QSL() << "cache/fontconfig/" << "lib/dpkg/lock" << "lib/udisks/mtab" << "lib/ureadahead/" << "log/" << "run/" << "tmp/");

    while(! in.atEnd())
    {
        QStr line(in.readLine()), item(right(line, - instr(line, "_")));

        if(exist("/var/" % item))
        {
            ThrdDbg = "@/var/" % item;

            if(! like(item, QSL() << "+_cache/apt/*" << "-*.bin_" << "-*.bin.*", Mixed) && ! like(item, QSL() << "_cache/apt/archives/*" << "*.deb_", All) && ! like(item, QSL() << "_lost+found_" << "_lost+found/*" << "*/lost+found_" << "*/lost+found/*" << "_Systemback_" << "_Systemback/*" << "*/Systemback_" << "*/Systemback/*" << "*.dpkg-old_" << "*~_" << "*~/*") && ! exclcheck(elist, item))
            {
                switch(left(line, instr(line, "_") - 1).toShort()) {
                case Islink:
                    if(! cplink("/var/" % item, "/var/.sblvtmp/var/" % item)) return false;
                    ++ThrdLng[0];
                    break;
                case Isdir:
                    if(! QDir().mkdir("/var/.sblvtmp/var/" % item)) return false;
                    ++ThrdLng[0];
                    break;
                case Isfile:
                    if(issmfs("/var/.sblvtmp", "/var/" % item) && link(QStr("/var/" % item).toStdString().c_str(), QStr("/var/.sblvtmp/var/" % item).toStdString().c_str()) == -1) return false;
                    ++ThrdLng[0];
                }
            }
            else if(item.startsWith("log"))
            {
                switch(left(line, instr(line, "_") - 1).toShort()) {
                case Isdir:
                    if(! QDir().mkdir("/var/.sblvtmp/var/" % item)) return false;
                    ++ThrdLng[0];
                    break;
                case Isfile:
                    if(! like(item, QSL() << "*.gz_" << "*.old_") && (! item.contains('.') || ! isnum(right(item, - rinstr(item, ".")))))
                    {
                        crtfile("/var/.sblvtmp/var/" % item);
                        if(! cpertime("/var/" % item, "/var/.sblvtmp/var/" % item)) return false;
                        ++ThrdLng[0];
                    }
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
        if(left(line, instr(line, "_") - 1).toShort() == Isdir && exist("/var/.sblvtmp/var/" % item) && ! cpertime("/var/" % item, "/var/.sblvtmp/var/" % item)) return false;
        if(ThrdKill) return false;
    }

    if(! cpertime("/var", "/var/.sblvtmp/var")) return false;
    QSL usrs;
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

    if(ThrdKill) return false;
    file.close();
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

    ++ThrdLng[0];
    if(ThrdKill) return false;
    elist = QSL() << ".sbuserdata" << ".cache/gvfs" << ".local/share/Trash/files/" << ".local/share/Trash/info/" << ".Xauthority" << ".ICEauthority";
    file.setFileName("/etc/systemback.excludes");
    if(! file.open(QIODevice::ReadOnly)) return false;

    while(! file.atEnd())
    {
        QStr cline(file.readLine().trimmed());
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
        ++ThrdLng[0];
        if(ThrdKill) return false;
        QStr rootitms(iudata ? rodir("/root") : rodir("/root", true));
        in.setString(&rootitms, QIODevice::ReadOnly);

        while(! in.atEnd())
        {
            QStr line(in.readLine()), item(right(line, - instr(line, "_")));
            ThrdDbg = "@/root/" % item;

            if(! like(item, QSL() << "_lost+found_" << "_lost+found/*" << "*/lost+found_" << "*/lost+found/*" << "_Systemback_" << "_Systemback/*" << "*/Systemback_" << "*/Systemback/*" << "*~_" << "*~/*") && ! exclcheck(elist, item) && exist("/root/" % item))
            {
                switch(left(line, instr(line, "_") - 1).toShort()) {
                case Islink:
                    if(! cplink("/root/" % item, usdir % "/root/" % item)) return false;
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
                            if(link(QStr("/root/" % item).toStdString().c_str(), QStr(usdir % "/root/" % item).toStdString().c_str()) == -1) return false;
                        }
                        else if(! cpfile("/root/" % item, usdir % "/root/" % item))
                            return false;

                        ++ThrdLng[0];
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
            if(left(line, instr(line, "_") - 1).toShort() == Isdir && exist(usdir % "/root/" % item) && ! cpertime("/root/" % item, usdir % "/root/" % item)) return false;
            if(ThrdKill) return false;
        }

        if(! cpertime("/root", usdir % "/root")) return false;
    }

    for(uchar a(0) ; a < usrs.count() ; ++a)
    {
        QStr udir(usrs.at(a)), usdir(uhl ? "/home/.sbuserdata/home" : QStr(sdir[2] % "/.sblivesystemcreate/userdata/home"));
        if(! QDir().mkdir(usdir % '/' % udir)) return false;
        ++ThrdLng[0];
        QStr useritms(iudata ? rodir("/home/" % udir) : rodir("/home/" % udir, true));
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
                            if(link(QStr("/home/" % udir % '/' % item).toStdString().c_str(), QStr(usdir % '/' % udir % '/' % item).toStdString().c_str()) == -1) return false;
                        }
                        else if(! cpfile("/home/" % udir % '/' % item, usdir % '/' % udir % '/' % item))
                            return false;

                        ++ThrdLng[0];
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
            if(left(line, instr(line, "_") - 1).toShort() == Isdir && exist(usdir % '/' % udir % '/' % item) && ! cpertime("/home/" % udir % '/' % item, usdir % '/' % udir % '/' % item)) return false;
            if(ThrdKill) return false;
        }

        if(! iudata && isfile("/home/" % udir % "/.config/user-dirs.dirs"))
        {
            file.setFileName("/home/" % udir % "/.config/user-dirs.dirs");
            if(! file.open(QIODevice::ReadOnly)) return false;

            while(! file.atEnd())
            {
                QStr cline(file.readLine().trimmed());

                if(! cline.startsWith('#') && cline.contains("$HOME"))
                {
                    QStr dir(left(right(cline, - instr(cline, "/")), -1));

                    if(isdir("/home/" % udir % '/' % dir))
                    {
                        if(! cpdir("/home/" % udir % '/' % dir, usdir % '/' % udir % '/' % dir)) return false;
                        ++ThrdLng[0];
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
