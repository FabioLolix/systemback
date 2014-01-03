#include <sys/stat.h>
#include <utime.h>

int tsync(const char *sourcefile, const char *newfile)
{

  struct stat sfstat;
    stat(sourcefile, &sfstat);

  struct utimbuf sftimes;
    sftimes.actime = sfstat.st_atime;
    sftimes.modtime = sfstat.st_mtime;

  utime(newfile, &sftimes);
  return 0;

}
