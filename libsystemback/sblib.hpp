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
#define QTS QTextStream
#define QSL QStringList
#define QSIL QList<short>
#define QStr QString
#define isfile(path) QFileInfo(path).isFile()
#define isdir(path) QFileInfo(path).isDir()
#define islink(path) QFileInfo(path).isSymLink()

#include "sblib_global.hpp"
#include <QStringList>
#include <QThread>

class SHARED_EXPORT_IMPORT sb : public QThread
{
public:
    explicit sb(QThread *parent = 0);

    static sb SBThrd;
    static QStr ThrdStr[3], ThrdDbg, sdir[3], schdle[7], pnames[15], trn[2];
    static quint64 ThrdLng[2];
    static const uchar Notexist = 0, Isfile = 1, Isdir = 2, Islink = 3, Isblock = 4, Unknow = 5, Read = 0, Write = 1, Exec = 2, Sblock = 0, Dpkglock = 1, Schdlrlock = 2, Norm = 0, All = 1, Mixed = 2, Remove = 0, Copy = 1, Sync = 2, Mount = 3, Umount = 4, Readprttns = 5, Readlvprttns = 6, Ruuid = 7, Setpflag = 8, Mkptable = 9, Mkpart = 10, Crtrpoint = 11, Srestore = 12, Scopy = 13, Lvprpr = 14;
    static uchar pnumber;
    static char Progress;
    static bool ExecKill, ThrdKill;

    static QSL readlvprttns();
    static QSL readprttns();
    static QStr mid(QStr txt, ushort start, ushort len);
    static QStr right(QStr txt, short len);
    static QStr left(QStr txt, short len);
    static QStr rndstr(uchar vlen = 10);
    static QStr ruuid(QStr part);
    static QStr rlink(QStr path);
    static QStr fload(QStr path);
    static QStr getarch();
    static quint64 devsize(QStr dev);
    static quint64 dfree(QStr path);
    static quint64 fsize(QStr path);
    static ushort rinstr(QStr txt, QStr stxt, ushort start = 0);
    static ushort instr(QStr txt, QStr stxt, ushort start = 1);
    static uchar exec(QStr cmd, QStr envv = NULL, bool silent = false, bool bckgrnd = false);
    static uchar fcomp(QStr file1, QStr file2);
    static uchar stype(QStr path);
    static uchar exec(QSL cmds);
    static bool srestore(uchar mthd, QStr usr, QStr srcdir, QStr trgt, bool sfstab = false);
    static bool mkpart(QStr dev, quint64 start = 0, quint64 len = 0);
    static bool mount(QStr dev, QStr mpoint, QStr moptns = NULL);
    static bool like(QStr txt, QSL lst, uchar mode = Norm);
    static bool scopy(uchar mthd, QStr usr, QStr srcdir);
    static bool execsrch(QStr fname, QStr ppath = NULL);
    static bool cpertime(QStr srcitem, QStr newitem);
    static bool access(QStr path, uchar mode = Read);
    static bool crtfile(QStr path, QStr txt = NULL);
    static bool cpfile(QStr srcfile, QStr newfile);
    static bool cplink(QStr srclink, QStr newlink);
    static bool cpdir(QStr srcddir, QStr newdir);
    static bool crtrpoint(QStr sdir, QStr pname);
    static bool exclcheck(QSL elist, QStr item);
    static bool setpflag(QStr part, QStr flag);
    static bool issmfs(QStr item1, QStr item2);
    static bool lcomp(QStr link1, QStr link2);
    static bool ilike(short num, QSIL lst);
    static bool lvprpr(bool iudata);
    static bool mkptable(QStr dev);
    static bool islnxfs(QStr path);
    static bool pisrng(QStr pname);
    static bool mcheck(QStr item);
    static bool remove(QStr path);
    static bool umount(QStr dev);
    static bool lock(uchar type);
    static bool exist(QStr path);
    static bool isnum(QStr txt);
    static void unlock(uchar type);
    static void delay(ushort msec);
    static void print(QStr txt);
    static void error(QStr txt);
    static void pupgrade();
    static void supgrade();
    static void cfgwrite();
    static void thrdelay();
    static void cfgread();
    static void fssync();

protected:
    void run();

private:
    static QSL ThrdSlst;
    static QStr odlst;
    static char sblock, dpkglock, schdlrlock;
    static uchar ThrdType, ThrdChr;
    static bool ThrdBool, ThrdRslt;

    static QStr rodir(QStr path, bool hidden = false);
    static void sbdir(QStr path, uchar oplen, bool hidden = false);
    QSL odir(QStr path, bool hidden = false);
    bool thrdsrestore(uchar &mthd, QStr &usr, QStr &srcdir, QStr &trgt, bool &sfstab);
    bool thrdscopy(uchar &mthd, QStr &usr, QStr &srcdir);
    bool recrmdir(QStr path, bool slimit = false);
    bool thrdcrtrpoint(QStr &sdir, QStr &pname);
    bool thrdlvprpr(bool &iudata);
    bool fspchk(QStr &dir);
};

#endif // SBLIB_HPP
