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

#include "sblib_global.hpp"
#include "sbtypedef.hpp"
#include <QThread>

class SHARED_EXPORT_IMPORT sb : public QThread
{
public:
    explicit sb(QThread *parent = nullptr);

    static sb SBThrd;
    static QStr ThrdStr[3], ThrdDbg, sdir[3], schdle[7], pnames[15], trn[2];
    static ullong ThrdLng[2];
    static cuchar Notexist, Isfile, Isdir, Islink, Isblock, Unknow, Read, Write, Exec, Sblock, Dpkglock, Schdlrlock, Norm, All, Mixed, Remove, Copy, Sync, Mount, Umount, Readprttns, Readlvprttns, Ruuid, Setpflag, Mkptable, Mkpart, Crtrpoint, Srestore, Scopy, Lvprpr;
    static uchar pnumber;
    static schar Progress;
    static bool ExecKill, ThrdKill;

    static QStr mid(cQStr &txt, cushort &start, cushort &len);
    static QStr right(cQStr &txt, cshort &len);
    static QStr left(cQStr &txt, cshort &len);
    static QStr rndstr(cuchar &vlen = 10);
    static QStr ruuid(cQStr &part);
    static QStr rlink(cQStr &path);
    static QStr fload(cQStr &path);
    static QStr getarch();
    static QStr ckname();
    static ullong devsize(cQStr &dev);
    static ullong dfree(cQStr &path);
    static ushort rinstr(cQStr &txt, cQStr &stxt, cushort &start = 0);
    static ushort instr(cQStr &txt, cQStr &stxt, cushort &start = 1);
    static uchar exec(cQStr &cmd, cQStr &envv = NULL, cbool &silent = false, cbool &bckgrnd = false);
    static uchar fcomp(cQStr &file1, cQStr &file2);
    static uchar stype(cQStr &path);
    static uchar exec(cQSL &cmds);
    static bool srestore(cuchar &mthd, cQStr &usr, cQStr &srcdir, cQStr &trgt, cbool &sfstab = false);
    static bool mkpart(cQStr &dev, cullong &start = 0, cullong &len = 0);
    static bool mount(cQStr &dev, cQStr &mpoint, cQStr &moptns = NULL);
    static bool like(cQStr &txt, cQSL &lst, cuchar &mode = Norm);
    static bool scopy(cuchar &mthd, cQStr &usr, cQStr &srcdir);
    static bool execsrch(cQStr &fname, cQStr &ppath = NULL);
    static bool pisrng(cQStr &pname, ushort *pid = nullptr);
    static bool cpertime(cQStr &srcitem, cQStr &newitem);
    static bool access(cQStr &path, cuchar &mode = Read);
    static bool crtfile(cQStr &path, cQStr &txt = NULL);
    static bool cpfile(cQStr &srcfile, cQStr &newfile);
    static bool cplink(cQStr &srclink, cQStr &newlink);
    static bool cpdir(cQStr &srcddir, cQStr &newdir);
    static bool crtrpoint(cQStr &sdir, cQStr &pname);
    static bool exclcheck(cQSL &elist, cQStr &item);
    static bool setpflag(cQStr &part, cQStr &flag);
    static bool issmfs(cQStr &item1,cQStr &item2);
    static bool lcomp(cQStr &link1, cQStr &link2);
    static bool ilike(cshort &num, cQSIL &lst);
    static bool lvprpr(cbool &iudata);
    static bool mkptable(cQStr &dev);
    static bool islnxfs(cQStr &path);
    static bool remove(cQStr &path);
    static bool mcheck(cQStr &item);
    static bool umount(cQStr &dev);
    static bool exist(cQStr &path);
    static bool lock(cuchar &type);
    static bool isnum(cQStr &txt);
    static bool ickernel();
    static bool efiprob();
    static void readlvprttns(QSL &strlst);
    static void readprttns(QSL &strlst);
    static void delay(cushort &msec);
    static void unlock(cuchar &type);
    static void print(cQStr &txt);
    static void error(cQStr &txt);
    static void pupgrade();
    static void supgrade();
    static void cfgwrite();
    static void thrdelay();
    static void xrestart();
    static void cfgread();
    static void fssync();

protected:
    void run();

private:
    static QSL *ThrdSlst;
    static int sblock[3];
    static cuchar *ThrdType, *ThrdChr;
    static cbool *ThrdBool;
    static bool ThrdRslt;

    static bool rodir(QStr &str, cQStr &path, cbool &hidden = false, cuchar &oplen = 0);
    static bool odir(QSL &strlst, cQStr &path, cbool &hidden = false);
    bool thrdsrestore(cuchar &mthd, cQStr &usr, cQStr &srcdir, cQStr &trgt, cbool &sfstab);
    bool thrdscopy(cuchar &mthd, cQStr &usr, cQStr &srcdir);
    bool recrmdir(cQStr &path, cbool &slimit = false);
    bool thrdcrtrpoint(cQStr &sdir, cQStr &pname);
    bool thrdlvprpr(cbool &iudata);
    bool fspchk(cQStr &dir);
};

#endif // SBLIB_HPP
