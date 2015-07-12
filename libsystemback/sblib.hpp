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

#ifndef SBLIB_HPP
#define SBLIB_HPP
#define fnln __attribute__((always_inline))

#include "sblib_global.hpp"
#include "bstr.hpp"
#include <QCoreApplication>
#include <QStringBuilder>
#include <QTemporaryFile>
#include <QFileInfo>
#include <QThread>
#include <sys/statvfs.h>
#include <sys/stat.h>
#include <unistd.h>

class SHARED_EXPORT_IMPORT sb : public QThread
{
    Q_DECLARE_TR_FUNCTIONS(systemback)

public:
    enum { Remove = 0, Copy = 1, Sync = 2, Mount = 3, Umount = 4, Readprttns = 5, Readlvdevs = 6, Ruuid = 7, Setpflag = 8, Mkptable = 9, Mkpart = 10, Delpart = 11, Crtrpoint = 12, Srestore = 13, Scopy = 14, Lvprpr = 15,
           MSDOS = 0, GPT = 1, Clear = 2, Primary = 3, Extended = 4, Logical = 5, Freespace = 6, Emptyspace = 7,
           Notexist = 0, Isfile = 1, Isdir = 2, Islink = 3, Isblock = 4, Unknown = 5,
           Noflag = 0, Silent = 1, Bckgrnd = 2, Prgrss = 4, Wait = 8,
           Sblock = 0, Dpkglock = 1, Schdlrlock = 2,
           Crtdir = 0, Rmfile = 1, Crthlnk = 2,
           False = 0, True = 1, Empty = 2,
           Read = 0, Write = 1, Exec = 2,
           Norm = 0, All = 1, Mixed = 2 };

    static sb SBThrd;
    static QStr ThrdStr[3], sdir[3], schdlr[2], pnames[15], lang, style, wsclng;
    static ullong ThrdLng[2];
    static uchar pnumber, ismpnt, schdle[6], waot, incrmtl, xzcmpr, autoiso, ecache;
    static schar Progress;
    static bool ExecKill, ThrdKill;

    static QStr mid(cQStr &txt, ushort start, ushort len);
    static QStr fload(cQStr &path, bool ascnt);
    static QStr right(cQStr &txt, short len);
    static QStr left(cQStr &txt, short len);
    static QStr gdetect(cQStr rdir = "/");
    static QStr rndstr(uchar vlen = 10);
    static QStr ruuid(cQStr &part);
    static QStr appver();
    static QBA fload(cQStr &path);
    template<typename T> static ullong dfree(const T &path);
    static fnln ullong fsize(cQStr &path);
    static fnln ushort instr(cQStr &txt, cQStr &stxt, ushort start = 1);
    static fnln ushort rinstr(cQStr &txt, cQStr &stxt);
    static uchar exec(cQStr &cmd, uchar flag = Noflag, cQStr &envv = nullptr);
    template<typename T> static uchar stype(const T &path);
    static uchar exec(cQSL &cmds);
    static bool srestore(uchar mthd, cQStr &usr, cQStr &srcdir, cQStr &trgt, bool sfstab = false);
    template<typename T1, typename T2> static bool issmfs(const T1 &item1, const T2 &item2);
    static bool mkpart(cQStr &dev, ullong start = 0, ullong len = 0, uchar type = Primary);
    static bool mount(cQStr &dev, cQStr &mpoint, cQStr &moptns = nullptr);
    template<typename T> static fnln bool crtdir(const T &path);
    template<typename T> static fnln bool rmfile(const T &file);
    static bool like(cQStr &txt, cQSL &lst, uchar mode = Norm);
    static bool execsrch(cQStr &fname, cQStr &ppath = nullptr);
    static bool cfgwrite(cQStr &file = "/etc/systemback.conf");
    static bool scopy(uchar mthd, cQStr &usr, cQStr &srcdir);
    static bool mkptable(cQStr &dev, cQStr &type = "msdos");
    static bool crtfile(cQStr &path, cQStr &txt = nullptr);
    static bool like(int num, cSIL &lst, bool all = false);
    template<typename T> static bool exist(const T &path);
    static bool access(cQStr &path, uchar mode = Read);
    static bool copy(cQStr &srcfile, cQStr &newfile);
    static bool setpflag(cQStr &part, cQStr &flags);
    static bool rename(cQStr &opath, cQStr &npath);
    static fnln bool islink(cQStr &path);
    static fnln bool isfile(cQStr &path);
    static fnln bool isdir(cQStr &path);
    static bool crtrpoint(cQStr &pname);
    static bool islnxfs(cQStr &path);
    static bool remove(cQStr &path);
    static bool mcheck(cQStr &item);
    static bool lvprpr(bool iudata);
    static bool fopen(QFile &file);
    static bool umount(cQStr &dev);
    static bool error(cQStr &txt);
    static bool isnum(cQStr &txt);
    static bool lock(uchar type);
    static void readprttns(QSL &strlst);
    static void readlvdevs(QSL &strlst);
    static void delpart(cQStr &part);
    static void unlock(uchar type);
    static void delay(ushort msec);
    static void print(cQStr &txt);
    static void supgrade();
    static void pupgrade();
    static void thrdelay();
    static void cfgread();
    static void fssync();
    static void ldtltr();

protected:
    void run();

private:
    sb();
    ~sb();

    static QTrn *SBtr;
    static QSL *ThrdSlst;
    static int sblock[3];
    static uchar ThrdType, ThrdChr;
    static bool ThrdBool, ThrdRslt;

    static QStr rlink(cQStr &path, ushort blen);
    static uchar fcomp(cQStr &file1, cQStr &file2);
    template<typename T1, typename T2> static fnln bool crthlnk(const T1 &srclnk, const T2 &newlnk);
    static bool rodir(QBA &ba, QUCL &ucl, cQStr &path, bool hidden = false, uchar oplen = 0);
    static bool cpertime(cQStr &srcitem, cQStr &newitem, bool skel = false);
    static bool cpfile(cQStr &srcfile, cQStr &newfile, bool skel = false);
    static bool cerr(uchar type, cQStr &str1, cQStr &str2 = nullptr);
    static bool odir(QBAL &balst, cQStr &path, bool hidden = false);
    static bool rodir(QUCL &ucl, cQStr &path, uchar oplen = 0);
    static bool rodir(QBA &ba, cQStr &path, uchar oplen = 0);
    static bool cplink(cQStr &srclink, cQStr &newlink);
    static bool cpdir(cQStr &srcdir, cQStr &newdir);
    static bool exclcheck(cQSL &elist, cQStr &item);
    static bool lcomp(cQStr &link1, cQStr &link2);
    bool thrdsrestore(uchar mthd, cQStr &usr, cQStr &srcdir, cQStr &trgt, bool sfstab);
    bool thrdscopy(uchar mthd, cQStr &usr, cQStr &srcdir);
    bool recrmdir(cQStr &path, bool slimit = false);
    bool thrdcrtrpoint(cQStr &trgt);
    bool thrdlvprpr(bool iudata);
};

inline QStr sb::left(cQStr &txt, short len)
{
    return txt.length() > qAbs(len) ? txt.left(len > 0 ? len : txt.length() + len) : len > 0 ? txt : nullptr;
}

inline QStr sb::right(cQStr &txt, short len)
{
    return txt.length() > qAbs(len) ? txt.right(len > 0 ? len : txt.length() + len) : len > 0 ? txt : nullptr;
}

inline QStr sb::mid(cQStr &txt, ushort start, ushort len)
{
    return txt.length() >= start ? txt.length() - start + 1 > len ? txt.mid(start - 1, len) : txt.right(txt.length() - start + 1) : nullptr;
}

inline ushort sb::instr(cQStr &txt, cQStr &stxt, ushort start)
{
    return txt.indexOf(stxt, start - 1) + 1;
}

inline ushort sb::rinstr(cQStr &txt, cQStr &stxt)
{
    return txt.lastIndexOf(stxt) + 1;
}

inline bool sb::like(int num, cSIL &lst, bool all)
{
    for(int val : lst)
        if(all ? num != val : num == val) return ! all;

    return all;
}

template<typename T> inline bool sb::exist(const T &path)
{
    struct stat istat;
    return lstat(bstr(path), &istat) == 0;
}

inline bool sb::islink(cQStr &path)
{
    return QFileInfo(path).isSymLink();
}

inline bool sb::isfile(cQStr &path)
{
    return QFileInfo(path).isFile();
}

inline bool sb::isdir(cQStr &path)
{
    return QFileInfo(path).isDir();
}

template<typename T> inline uchar sb::stype(const T &path)
{
    struct stat istat;
    if(lstat(bstr(path), &istat) == -1) return Notexist;

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
        return Unknown;
    }
}

inline ullong sb::fsize(cQStr &path)
{
    return QFileInfo(path).size();
}

template<typename T> ullong sb::dfree(const T &path)
{
    struct statvfs dstat;
    return statvfs(bstr(path), &dstat) == -1 ? 0 : dstat.f_bavail * dstat.f_bsize;
}

template<typename T1, typename T2> inline bool sb::issmfs(const T1 &item1, const T2 &item2)
{
    struct stat istat[2];
    return ! like(-1, {stat(bstr(item1), &istat[0]), stat(bstr(item2), &istat[1])}) && istat[0].st_dev == istat[1].st_dev;
}

template<typename T> inline bool sb::crtdir(const T &path)
{
    return mkdir(bstr(path), 0755) == 0 ? true : cerr(Crtdir, path);
}

template<typename T> inline bool sb::rmfile(const T &file)
{
    return unlink(bstr(file)) == 0 ? true : cerr(Rmfile, file);
}

inline bool sb::isnum(cQStr &txt)
{
    for(uchar a(0) ; a < txt.length() ; ++a)
        if(! txt.at(a).isDigit()) return false;

    return ! txt.isEmpty();
}

#endif
