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

#ifndef SBLIB_HPP
#define SBLIB_HPP
#define _FILE_OFFSET_BITS 64

#include "sblib_global.hpp"
#include "sbtypedef.hpp"
#include <QFileInfo>
#include <QThread>
#include <sys/stat.h>

class SHARED_EXPORT_IMPORT sb : public QThread
{
public:
    explicit sb(QThread *parent = nullptr);

    static sb SBThrd;
    static QStr ThrdStr[3], ThrdDbg, sdir[3], schdle[7], pnames[15], trn[2];
    static ullong ThrdLng[2];
    static cuchar Remove{ 0 }, Copy{ 1 }, Sync{ 2 }, Mount{ 3 }, Umount{ 4 },
        Readprttns{ 5 }, Readlvprttns{ 6 }, Ruuid{ 7 }, Setpflag{ 8 },
        Mkptable{ 9 }, Mkpart{ 10 }, Delpart{ 11 }, Crtrpoint{ 12 },
        Srestore{ 13 }, Scopy{ 14 }, Lvprpr{ 15 }, MSDOS{ 0 }, GPT{ 1 },
        Clear{ 2 }, Primary{ 3 }, Extended{ 4 }, Logical{ 5 }, Freespace{ 6 },
        Emptyspace{ 7 }, Notexist{ 0 }, Isfile{ 1 }, Isdir{ 2 }, Islink{ 3 },
        Isblock{ 4 }, Unknow{ 5 }, Read{ 0 }, Write{ 1 }, Exec{ 2 },
        Sblock{ 0 }, Dpkglock{ 1 }, Schdlrlock{ 2 }, Norm{ 0 }, All{ 1 },
        Mixed{ 2 };
    static uchar pnumber;
    static schar Progress;
    static bool ExecKill, ThrdKill;

    static QStr mid(cQStr &txt, ushort start, ushort len);
    static QStr right(cQStr &txt, short len);
    static QStr left(cQStr &txt, short len);
    static QStr rndstr(cuchar vlen = 10);
    static QStr ruuid(cQStr &part);
    static QStr fload(cQStr &path);
    static QStr getarch();
    static QStr ckname();
    static ullong dfree(cQStr &path);
    static ullong fsize(cQStr &path);
    static ushort instr(cQStr &txt, cQStr &stxt, ushort start = 1);
    static ushort rinstr(cQStr &txt, cQStr &stxt);
    static uchar exec(cQStr &cmd, cQStr &envv = NULL, bool silent = false,
                      bool bckgrnd = false);
    static uchar stype(cQStr &path);
    static uchar exec(cQSL &cmds);
    static bool srestore(cuchar mthd, cQStr &usr, cQStr &srcdir, cQStr &trgt,
                         bool sfstab = false);
    static bool mkpart(cQStr &dev, ullong start = 0, ullong len = 0,
                       uchar type = Primary);
    static bool mount(cQStr &dev, cQStr &mpoint, cQStr &moptns = NULL);
    static bool like(cQStr &txt, cQSL &lst, cuchar mode = Norm);
    static bool scopy(cuchar mthd, cQStr &usr, cQStr &srcdir);
    static bool execsrch(cQStr &fname, cQStr &ppath = NULL);
    static bool pisrng(cQStr &pname, ushort *pid = nullptr);
    static bool mkptable(cQStr &dev, cQStr &type = "msdos");
    static bool access(cQStr &path, cuchar mode = Read);
    static bool crtfile(cQStr &path, cQStr &txt = NULL);
    static bool copy(cQStr &srcfile, cQStr &newfile);
    static bool crtrpoint(cQStr &sdir, cQStr &pname);
    static bool setpflag(cQStr &part, cQStr &flag);
    static bool ilike(short num, cQSIL &lst);
    static bool islnxfs(cQStr &path);
    static bool islink(cQStr &path);
    static bool isfile(cQStr &path);
    static bool remove(cQStr &path);
    static bool mcheck(cQStr &item);
    static bool lvprpr(bool iudata);
    static bool umount(cQStr &dev);
    static bool exist(cQStr &path);
    static bool isdir(cQStr &path);
    static bool lock(cuchar type);
    static bool ickernel();
    static bool efiprob();
    static void readlvprttns(QSL &strlst);
    static void readprttns(QSL &strlst);
    static void unlock(cuchar type);
    static void delay(ushort msec);
    static void print(cQStr &txt);
    static void error(cQStr &txt);
    static void pupgrade();
    static void supgrade();
    static void cfgwrite();
    static void thrdelay();
    static void xrestart();
    static void cfgread();
    static void fssync();

    static void delpart(cQStr &part);

protected:
    void run();

private:
    static QSL *ThrdSlst;
    static int sblock[3];
    static uchar ThrdType, ThrdChr;
    static bool ThrdBool, ThrdRslt;

    static QStr rlink(cQStr &path, ushort blen);
    static ullong psalign(ullong start, ushort ssize);
    static ullong pealign(ullong end, ushort ssize);
    static ullong devsize(cQStr &dev);
    static uchar fcomp(cQStr &file1, cQStr &file2);
    static bool rodir(QStr &str, cQStr &path, bool hidden = false,
                      cuchar oplen = 0);
    static bool odir(QSL &strlst, cQStr &path, bool hidden = false);
    static bool cpertime(cQStr &srcitem, cQStr &newitem);
    static bool cplink(cQStr &srclink, cQStr &newlink);
    static bool cpfile(cQStr &srcfile, cQStr &newfile);
    static bool cpdir(cQStr &srcddir, cQStr &newdir);
    static bool exclcheck(cQSL &elist, cQStr &item);
    static bool issmfs(cQStr &item1, cQStr &item2);
    static bool lcomp(cQStr &link1, cQStr &link2);
    static bool isnum(cQStr &txt);
    bool thrdsrestore(cuchar mthd, cQStr &usr, cQStr &srcdir, cQStr &trgt,
                      bool sfstab);
    bool thrdscopy(cuchar mthd, cQStr &usr, cQStr &srcdir);
    bool recrmdir(cQStr &path, bool slimit = false);
    bool thrdcrtrpoint(cQStr &sdir, cQStr &pname);
    bool thrdlvprpr(bool iudata);
    bool fspchk(cQStr &dir);
};

inline QStr sb::left(cQStr &txt, short len)
{
    return txt.length() > qAbs(len)
               ? txt.left(len > 0 ? len : txt.length() + len)
               : len > 0 ? txt : NULL;
}

inline QStr sb::right(cQStr &txt, short len)
{
    return txt.length() > qAbs(len)
               ? txt.right(len > 0 ? len : txt.length() + len)
               : len > 0 ? txt : NULL;
}

inline QStr sb::mid(cQStr &txt, ushort start, ushort len)
{
    return txt.length() >= start ? txt.length() - start + 1 > len
        ? txt.mid(start - 1, len)
        : txt.right(txt.length() - start + 1)
        : NULL;
}

inline ushort sb::instr(cQStr &txt, cQStr &stxt, ushort start)
{
    return txt.indexOf(stxt, start - 1) + 1;
}

inline ushort sb::rinstr(cQStr &txt, cQStr &stxt)
{
    return txt.lastIndexOf(stxt) + 1;
}

inline bool sb::like(cQStr &txt, cQSL &lst, cuchar mode)
{
    switch (mode) {
    case Norm:
        for (cQStr &stxt : lst)
            if (stxt.startsWith('*')) {
                if (stxt.endsWith('*')) {
                    if (txt.contains(stxt.mid(1, stxt.length() - 2)))
                        return true;
                } else if (txt.endsWith(stxt.mid(1, stxt.length() - 2)))
                    return true;
            } else if (stxt.endsWith('*')) {
                if (txt.startsWith(stxt.mid(1, stxt.length() - 2)))
                    return true;
            } else if (txt == stxt.mid(1, stxt.length() - 2))
                return true;

        return false;
    case All:
        for (cQStr &stxt : lst)
            if (stxt.startsWith('*')) {
                if (stxt.endsWith('*')) {
                    if (!txt.contains(stxt.mid(1, stxt.length() - 2)))
                        return false;
                } else if (!txt.endsWith(stxt.mid(1, stxt.length() - 2)))
                    return false;
            } else if (stxt.endsWith('*')) {
                if (!txt.startsWith(stxt.mid(1, stxt.length() - 2)))
                    return false;
            } else if (txt != stxt.mid(1, stxt.length() - 2))
                return false;

        return true;
    case Mixed: {
        QSL alst, nlst;

        for (cQStr &stxt : lst)
            switch (stxt.at(0).toLatin1()) {
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

inline bool sb::ilike(short num, cQSIL &lst)
{
    for (short val : lst)
        if (num == val)
            return true;

    return false;
}

inline bool sb::exist(cQStr &path)
{
    struct stat istat;
    return lstat(path.toStdString().c_str(), &istat) == 0;
}

inline bool sb::islink(cQStr &path) { return QFileInfo(path).isSymLink(); }
inline bool sb::isfile(cQStr &path) { return QFileInfo(path).isFile(); }
inline bool sb::isdir(cQStr &path) { return QFileInfo(path).isDir(); }

inline uchar sb::stype(cQStr &path)
{
    struct stat istat;
    if (lstat(path.toStdString().c_str(), &istat) == -1)
        return Notexist;

    switch (istat.st_mode & S_IFMT) {
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

inline ullong sb::fsize(cQStr &path) { return QFileInfo(path).size(); }

inline bool sb::access(cQStr &path, cuchar mode)
{
    switch (mode) {
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

#endif // SBLIB_HPP
