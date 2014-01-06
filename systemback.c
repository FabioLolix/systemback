/********************************************************************

 Systemback shared library

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

#include <sys/stat.h>
#include <sys/types.h>
#include <utime.h>

int cpertime(const char *sourceitem, const char *newitem)
{

  struct stat sistat;
    if(stat(sourceitem, &sistat) == -1)
      return -1;

  struct stat nistat;
    if(stat(newitem, &nistat) == -1)
      return -1;

  if(sistat.st_mode != nistat.st_mode)
    if(chmod(newitem, sistat.st_mode) == -1)
      return -1;

  if(sistat.st_uid != nistat.st_uid || sistat.st_gid != nistat.st_gid)
    if(chown(newitem, sistat.st_uid, sistat.st_gid) == -1)
      return -1;

  if(sistat.st_atime != nistat.st_atime || sistat.st_mtime != nistat.st_mtime)
  {
    struct utimbuf sitimes;
      sitimes.actime = sistat.st_atime;
      sitimes.modtime = sistat.st_mtime;

    if(utime(newitem, &sitimes) == -1)
      return -1;
    else
      return 0;
  }
  else
    return 0;

}

int cpdir(const char *sourcedir, const char *newdir)
{

  struct stat sdstat;
    if(stat(sourcedir, &sdstat) == -1)
      return -1;

  if(mkdir(newdir, sdstat.st_mode) == -1)
    return -1;

  struct stat ndstat;
    if(stat(newdir, &ndstat) == -1)
      return -1;

  if(sdstat.st_mode != ndstat.st_mode)
    if(chmod(newdir, sdstat.st_mode) == -1)
      return -1;

  if(sdstat.st_uid != ndstat.st_uid || sdstat.st_gid != ndstat.st_gid)
    if(chown(newdir, sdstat.st_uid, sdstat.st_gid) == -1)
      return -1;

  struct utimbuf sdtimes;
    sdtimes.actime = sdstat.st_atime;
    sdtimes.modtime = sdstat.st_mtime;

  if(utime(newdir, &sdtimes) == -1)
    return -1;
  else
    return 0;

}
