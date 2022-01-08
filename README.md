Hierachical Filesystem
======================
**Warning:** this code is from 2002, and has barely been adapted to work in 2022.
Plan is to rewite it in python-fuse.

Hierfs is a userspace implementation of a Hierachical storage management
mainly aimed at desktop users with one CD drive.

Hirachical storage management systems are used to keep large amounts
of data visible online in a filesystem whereas the real data is stored
offline on optical disks or tapes. The offline files are usually
read-only.

As soon as a file is read, the required media is loaded and the file
can be accesed after a certain delay.
Except for the delay, the migration to the offline storage and reads
from that storage are transparent for the end user.

This implementation uses FUSE available in your distribution repo

Installation
============
install dependencies. 

    apt install fuse libfuse-dev kdialog

Compile it (yeah, no makefile, sorry)

    gcc -Wall hierfs.c $(pkg-config fuse --cflags --libs) -o hierfs -o hierfs

Install it locall, or system wide

    install hierfs ~/bin/
    OR
    sudo install hierfs /usr/local/bin/




Usage
=====
first watch out for a nice huge filesystem you would like to have on CD
instead of wasting your precious diskspace.

Let's suppose that one is /home/user/data/videos

We need an additional empty directory to mount the hierfs, let's name
that one: /home/user/data/videos.hierfs

Mounting
--------
    export HIERFS_DATA=/home/user/data/videos
    hierfs /home/user/data/videos.hierfs

Now you can see your new filesystem with
    df /home/user/data/videos.hierfs

Nothing changed, the filesytem and the videos directory are exactly the
same and changes (rm, mv, cp ...) are seen from one to the other.

From now on you should preferably access your data through the hierfs
filesystem instead of the real directory.

Putting data on CD
------------------
This is dangerous as the files on harddisk are in fact deleted in the process
so if the CD is broken, the data is lost!
To migrate the data, do this:

    export HIERFS_DATA=/home/user/data/videos
    hierfs-migrate.pl

This tool will ask you for a CD label and burn the oldest files found
to the CD until it's full. After the CD is finished, the files are removed
from the harddisk but still remain visible in the hierfs.

You can mount the CD, it's a standard ISO filesystem with all migrated files
in the root directory.

If you access one of the migrated files, you will get a message box asking
to insert the correct CD.

Adding existing CDs
-------------------
just use hierfs-import.pl



