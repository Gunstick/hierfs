/* hierfs: hierachical filesystems for standalone CD-R / CDwriter
   Copyright (C) 2002 FSF, Georges Kesseler (georges.kesseler@cpu.lu)
   implemented via FUSE
*/
/*
    FUSE: Filesystem in Userspace
    Copyright (C) 2001  Miklos Szeredi (mszeredi@inf.bme.hu)

    This program can be distributed under the terms of the GNU GPL.
    See the file COPYING.
*/

/*
hierfs: hierachical filesystem documentation

yes, I'm one of those everything-in-one-single-file programmers
this is my first C program since 10 years

* princip

copy move edit etc... files in the hierfs 
once there is 700MB of files (or harddiskdisk storage is full?) put files 
   into isofs: ask for a CD, ask for the CD label name, mkisofs, write cd
               record which files got witten (incl attributes)
it is still possible to move files around in the filesystem after they are on CD

implements: getattr getdir mknod mkdir unlink rmdir rename chmod chown utime  open read write statfs     

* restrictions

does not implement: readlink symlink link truncate
max file size is 700MB [extension perhaps with multi-CD]
destaged files get readonly, can't unlink rmdir rename chmod chown write

* internal database

  NEW version to be implemented here
the 'database' is implemented in the filesystem below the mountpoint
so even if the hierfs does not work you will still be able to get 
to the non-destaged files and find out on which CD the destaged files are.
/.../.../dir/   classic directories mapped directly to underlying FS
/.../.../file      there are 3 cases
          if the T bit is cleared, it's the real file
          if the T bit is set, the file is on CD
              if the filesize is 0 (zero) then the file is no more online
                                          else it's the cached file from CD
            the fileinfo is in .hierfs/file
            fileinfo is a txt file with infos about the file in dumb ASCII 
            label: <label of the CD> [<label of the CD>]
            name: what's the name on CD (usually identical to file name)
            size: <nb bytes>
            mode: <octal mode e.g. 100777>
            uid: <like in passwd>
            gid: <like in passwd>
            mtime: <uxtimestamp> is last modif time in archive
            atime: <uxtimestamp> is destage date
            ctime: <uxtimestamp> is first appearing in archive (or last chmod, chown ...)

* cd format
each CD has the files in the main dir, no structure, the structure is
given by the filesystem on harddisk. 
as bonus perhaps put a tgz of the T-bit set files structure on CD, so on the 
latest CD you will have the complete database.

*/

#ifdef linux
/* For pread()/pwrite() */
#define _XOPEN_SOURCE 500
#endif

#include <fuse.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <sys/statfs.h>
#include <stdlib.h>
#include <string.h>


int xmp_ttest (const char *path)
{   /* check if T bit is set */
    /* 0 = unset */
    /* -1 = set */
    /* <-1 = stat error */
    
    int res;
    struct stat stbuf;

    res = lstat(path, &stbuf);
    if(res == -1)
        return -errno;  /* this works because lstat does never return errno = EPERM (1) */
    if ((stbuf.st_mode & S_ISVTX) && (stbuf.st_mode & S_IFREG))
    {
//      printf("this file is on CD\n");
      return -1;
    }
    else
    {
      return 0;
    }
}

char * xmp_path(const char *path)
{
  char *newpath;
  newpath=(char*)malloc(strlen(getenv("HIERFS_DATA"))+strlen(path)+1);
  strcpy(newpath,getenv("HIERFS_DATA"));
  strcat(newpath,path);
  return newpath;
}

char * cdrom_path(const char *path)
{
  char *newpath;
  newpath=(char*)malloc(strlen("/cdrom/")+strlen(path)+1);
  strcpy(newpath,"/cdrom/");
  strcat(newpath,path);
  return newpath;
}

void tfile2stat(const char *path,struct stat *stbuf,char *label,char *name)
{
  char buff[256];
  FILE *info;
  char attribute[20],value[100];

  info = fopen(path, "r");
  while(!feof(info))
  {
    int val;

    fgets(buff,255,info);
    sscanf(buff,"%[^:]: %s",attribute,value);
    /* read label */
    if(sscanf(buff,"label: %[^\n]",value) && (label != NULL))
        strncpy(label,value,100); 
    if(sscanf(buff,"name: %[^\n]",value) && (name != NULL))
        strncpy(name,value,100); 
    /* put read infos insto struct data */
//    printf("att=%s,val=%s , %ld.\n",attribute,value,atol(value));
    /* st_dev no used */
    /* st_ino just keep the inode of the info file */
    if(sscanf(buff,"mode: %o",&val))
          stbuf->st_mode=(mode_t)val | S_ISVTX;
    /* st_nlink not supported on this filesystem, keeping data of info file */
    if(sscanf(buff,"uid: %s",value))
          stbuf->st_uid=(uid_t)atol(value);
    if(sscanf(buff,"gid: %s",value))
          stbuf->st_gid=(gid_t)atol(value);
    /* st_rdev there are no devices on this FS */
    if(sscanf(buff,"size: %s",value))
    {
          stbuf->st_size=(off_t)atol(value);
          stbuf->st_blksize=512;
          stbuf->st_blocks=(unsigned long)(atol(value)/stbuf->st_blksize);
    }
    if(sscanf(buff,"mtime: %s",value))
          stbuf->st_mtime=(time_t)atol(value);
    if(sscanf(buff,"atime: %s",value))
          stbuf->st_atime=(time_t)atol(value);
    if(sscanf(buff,"ctime: %s",value))
          stbuf->st_ctime=(time_t)atol(value);
  }
  fclose(info);
}


static int xmp_getattr(const char *path, struct stat *stbuf)
{
    int res;
    char * npath;
// 
//             struct stat
//              {
//                  dev_t         st_dev;      /* device */
//                  ino_t         st_ino;      /* inode */
//                  mode_t        st_mode;     /* protection */
//                  nlink_t       st_nlink;    /* number of hard links */
//                  uid_t         st_uid;      /* user ID of owner */
//                  gid_t         st_gid;      /* group ID of owner */
//                  dev_t         st_rdev;     /* device type (if inode device) */
//                  off_t         st_size;     /* total size, in bytes */
//                  unsigned long st_blksize;  /* blocksize for filesystem I/O */
//                  unsigned long st_blocks;   /* number of blocks allocated */
//                  time_t        st_atime;    /* time of last access */
//                  time_t        st_mtime;    /* time of last modification */
//                  time_t        st_ctime;    /* time of last change */
//              };
//
    npath=xmp_path(path);
//    printf("getattr: %s\n",npath);
    res = lstat(npath, stbuf);
    if(res == -1)
    {   free(npath);
        return -errno;
    }
    if ((stbuf->st_mode & S_ISVTX) && (stbuf->st_mode & S_IFREG))
    { 
      tfile2stat(npath, stbuf,NULL,NULL);
    }
    else
      printf("this file is online\n");
    free(npath);
    return 0;
}


static int xmp_getdir(const char *path, fuse_dirh_t h, fuse_dirfil_t filler)
{   /* simply pass through */
    DIR *dp;
    struct dirent *de;
    int res = 0;
    char * npath;

    npath=xmp_path(path);
    printf("getdir: %s\n",npath);
    dp = opendir(npath);
    free(npath);
    if(dp == NULL)
        return -errno;

    while((de = readdir(dp)) != NULL) {
        res = filler(h, de->d_name, de->d_type);
        if(res != 0)
            break;
    }

    closedir(dp);
    return res;
}

static int xmp_mknod(const char *path, mode_t mode, dev_t rdev)
{   /* stat the file first and don't do anything if T bit set ? */
    int res;
    char * npath;

    npath=xmp_path(path);
    printf("mknod: %s %d %d\n",npath,(int)mode,(int)rdev);
    res = mknod(npath, mode, rdev);
    free(npath);
    if(res == -1)
        return -errno;

    return 0;
}

static int xmp_mkdir(const char *path, mode_t mode)
{   /* simple pass through */
    int res;
    char * npath;

    npath=xmp_path(path);
    printf("mkdir: %s %d\n",npath,mode);
    res = mkdir(npath, mode);
    free(npath);
    if(res == -1)
        return -errno;

    return 0;
}

static int xmp_unlink(const char *path)
{   /* stat first the file, if T bit set, then EROFS (read only FS) */ 
    int res;
    char * npath;
//    struct stat stbuf;

    npath=xmp_path(path);
    printf("unlink: %s\n",npath);
    res=xmp_ttest(npath);
    if(res < -1)
    { free(npath);
      return res;  /* it is already in -errno format */
    }
    if(res == -1)
    {
      printf("this file is on CD\n");
      free(npath);
      return -EROFS;   /* cannot RM on CD */
    }
    res = unlink(npath);
    free(npath);
    if(res == -1)
      return -errno;
    return 0;
}

static int xmp_rmdir(const char *path)
{   /* simple pass through */
    int res;
    char * npath;

    npath=xmp_path(path);
    printf("rmdir: %s\n",npath);
    res = rmdir(npath);
    free(npath);
    if(res == -1)
        return -errno;

    return 0;
}


static int xmp_rename(const char *from, const char *to)
{   /* simple pass through, works as the info files contain the filename on CD */
    int res;
    char *nfrom; char *nto;

    nfrom=xmp_path(from); nto=xmp_path(to);
    printf("rename: %s %s\n",nfrom,nto);
    res = rename(nfrom, nto);
    free(nfrom);free(nto);
    if(res == -1)
        return -errno;

    return 0;
}


static int xmp_chmod(const char *path, mode_t mode)
{   /* stat T bit, if set return EROFS, else pass through */
    int res;
//    struct stat stbuf;
    char * npath;

    npath=xmp_path(path);
    printf("chmod: %s %o %o %o\n",npath,mode,S_ISVTX,~S_ISVTX);

    res=xmp_ttest(npath);
//    res = lstat(npath, &stbuf);
    if(res < -1)
//    if(res == -1)
    { free(npath);
      return res;  /* it is already in -errno format */
//        return -errno;
    }

    if(res == -1)
//    if ((stbuf.st_mode & S_ISVTX) && (stbuf.st_mode & S_IFREG))
    {
      printf("this file is on CD\n");
      free(npath);
      return -EROFS;  /* cannot chmod on CD */
    }
//    else
//    {
      res = chmod(npath, mode & ~S_ISVTX);
      free(npath);
      if(res == -1)
          return -errno;
//    }
    return 0;
}

static int xmp_chown(const char *path, uid_t uid, gid_t gid)
{   /* stat T bit, if set return EROFS, else pass through */
    int res;
//    struct stat stbuf;
    char * npath;

    npath=xmp_path(path);
    printf("chown: %s %d %d\n",npath,uid,gid);
    res=xmp_ttest(npath);
//    res = lstat(npath, &stbuf);
    if(res < -1)
//    if(res == -1)
    { free(npath);
      return res;  /* it is already in -errno format */
//        return -errno;
    }
    if(res == -1)
//    if ((stbuf.st_mode & S_ISVTX) && (stbuf.st_mode & S_IFREG))
    {
      printf("this file is on CD\n");
      free(npath);
      return -EROFS; /* cannot chown on CD */
    }
//    else
//    {
      res = lchown(npath, uid, gid);
      free(npath);
      if(res == -1)
          return -errno;
//    }
    return 0;
}


static int xmp_utime(const char *path, struct utimbuf *buf)
{   /* stat T bit, if set return EROFS, else pass through */
    int res;
//    struct stat stbuf;
    char * npath;
    
    npath=xmp_path(path);
    printf("utime: %s\n",npath);
    res=xmp_ttest(npath);
//    res = lstat(npath, &stbuf);
    if(res < -1)
//    if(res == -1)
    {
      free(npath);
      return res;  /* it is already in -errno format */
//        return -errno;
    }
    if(res == -1)
//    if ((stbuf.st_mode & S_ISVTX) && (stbuf.st_mode & S_IFREG))
    {
      printf("this file is on CD\n");
      free(npath);
      return -EROFS;  /* cannot utime on CD */
    }
//    else
//    {
      res = utime(npath, buf);
      free(npath);
      if(res == -1)
          return -errno;
//    }
    return 0;
}


static int xmp_open(const char *path, int flags)
{   /* stat T bit, if set and wite flag, return EROFS */ 
    int res;
//    struct stat stbuf;
    char * npath;

    npath=xmp_path(path);
    printf("open: %s %o\n",npath,flags);
    res=xmp_ttest(npath);
//    res = lstat(npath, &stbuf);
    if(res < -1)
//    if(res == -1)
    { free(npath);
      return res;  /* it is already in -errno format */
//        return -errno;
    }
//    if ((stbuf.st_mode & S_ISVTX) && (stbuf.st_mode & S_IFREG) &&
//        ((flags & O_WRONLY) || (flags & O_RDWR)) )
    if((res == -1) && ((flags & O_WRONLY) || (flags & O_RDWR)) )
    {
      printf("this file is on CD, not allowed to write\n");
      free(npath);
      return -EROFS;  /* cannot open as write on CD */
    }
//    else
//    {
      res = open(npath, flags);
      free(npath);
      if(res == -1) 
          return -errno;
//    }
    close(res);
    return 0;  /* always return 0 on success ! */
}

static int xmp_read(const char *path, char *buf, size_t size, off_t offset)
{   /* stat T bit, if set, do the CD volume mount and open file from there */
    int fd;
    int res;
    struct stat stbuf;
    char * npath;

    npath=xmp_path(path);
//    printf("read: %s %d %d\n",npath,(int)size,(int)offset);
    res=xmp_ttest(npath);
//    res = lstat(npath, &stbuf);
    if(res < -1)
//    if(res == -1)
    {  free(npath);
      return res;  /* it is already in -errno format */
//        return -errno;
    }
    if(res == -1)
//    if ((stbuf.st_mode & S_ISVTX) && (stbuf.st_mode & S_IFREG))
    { char label[100],filename[100];

//      printf("this file is on CD\n");
      tfile2stat(npath, &stbuf,label,filename);
//printf ("file on disk: %s, file on CD: %s, label: %s\n",npath,filename,label);
      /* stat the file on cd, if not accessible */
      free(npath);
      npath=cdrom_path(filename);
      res = lstat(npath, &stbuf);
      while(res == -1)  /* not on CD */
      { 
        char msg[100];
        system("/usr/bin/eject /cdrom");  
        sprintf(msg,"kdialog --yesno 'Please put cd\\n%s\\ninto drive'",label) ;
        if (system(msg) == 1)
        { printf ("user answered no\n");
          free(npath);
          return -1;
        }
        system("/bin/mount /cdrom");
        res = lstat(npath, &stbuf);
      }
    }
    fd = open(npath, O_RDONLY);
    free(npath);
    if(fd == -1)
        return -errno;
   
    res = pread(fd, buf, size, offset);
    if(res == -1)
        res = -errno;
    
    close(fd);
    return res;
}

static int xmp_write(const char *path, const char *buf, size_t size,
                     off_t offset)
{ /* check T bit, if set return EROFS, else pass through */
/* write the file and then remove write in mode so that it can't
   get overwritten by other files? */
    int fd;
    int res;
//    struct stat stbuf;
    char * npath;

    npath=xmp_path(path);
    printf("write: %s %d %d\n",npath,(int)size,(int)offset);
    res=xmp_ttest(npath);
//    res = lstat(npath, &stbuf);
    if(res < -1)
//    if(res == -1)
    {   free(npath);
      return res;  /* it is already in -errno format */
//        return -errno;
    }
//    if ((stbuf.st_mode & S_ISVTX) && (stbuf.st_mode & S_IFREG) )
    if(res == -1)
    {
      printf("this file is on CD, not allowed to write\n");
      free(npath);
      return -EROFS;
    }
    
    fd = open(npath, O_WRONLY);
    free(npath);
    if(fd == -1)
        return -errno;

    res = pwrite(fd, buf, size, offset);
    if(res == -1)
        res = -errno;
    
    close(fd);
    return res;
}

static int xmp_statfs(const char *path, struct statfs *fst)
{   /* hmm.... how to do this ? */
    struct statfs st;
/*
              struct statfs {
                 long    f_type;     * type of filesystem (see below) *
                 long    f_bsize;    * optimal transfer block size *
                 long    f_blocks;   * total data blocks in file system *
                 long    f_bfree;    * free blocks in fs *
                 long    f_bavail;   * free blocks avail to non-superuser *
                 long    f_files;    * total file nodes in file system *
                 long    f_ffree;    * free file nodes in fs *
                 fsid_t  f_fsid;     * file system id *
                 long    f_namelen;  * maximum length of filenames *
                 long    f_spare[6]; * spare for later *
              };
*/
    int rv = statfs("/",&st);

    printf("statfs: \n");
    if(!rv)
	memcpy(fst,&st,sizeof(st));
    return rv;
}

static struct fuse_operations xmp_oper = {
    getattr:	xmp_getattr,
    readlink:	NULL,
    getdir:     xmp_getdir,
    mknod:	xmp_mknod,
    mkdir:	xmp_mkdir,
    symlink:	NULL,
    unlink:	xmp_unlink,
    rmdir:	xmp_rmdir,
    rename:     xmp_rename,
    link:	NULL,
    chmod:	xmp_chmod,
    chown:	xmp_chown,
    truncate:	NULL,
    utime:	xmp_utime,
    open:	xmp_open,
    read:	xmp_read,
    write:	xmp_write,
    statfs:	xmp_statfs,
};

int main(int argc, char *argv[])
{
    if (getenv("HIERFS_DATA") == NULL ) 
    {
      printf ("please set environment variable HIERFS_DATA to the directory where the files are stored\n");
      printf ("this directory has to be *different* from the mountpoint!\n");
      exit(1);
    }
    if (argc > 1) /* we have a mountpoint argument */
    {
      FILE *mounts;
      mounts = fopen("/proc/mounts", "r");
      while(!feof(mounts))
      { char fstype[20],dummy[40],mount[256];
        fscanf(mounts,"%s %s %s %s %s %s",dummy,mount,fstype,dummy,dummy,dummy);
        if ((strcmp("fuse",fstype)==0) && (strcmp(argv[1],mount) == 0))
        {   /* is still mounted */
          struct stat stbuf;
          char umount[100];

          printf("still mounted\n");
          if(lstat(mount, &stbuf) == 0)
            exit (1);
          sprintf(umount,"fusermount -u %s",mount) ;
          if(system(umount) !=0)
            exit (1);
        }
      }
    }
    fuse_main(argc, argv, &xmp_oper);
    return 0;
}
