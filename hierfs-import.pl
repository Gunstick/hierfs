#!/usr/bin/perl
use File::stat;
use File::Find;
$File::Find::dont_use_nlink = 1;   # make shure for cd-rom
if ($ARGV[0] eq "")
{
  print "usage $0 cd_mountpoint hierfs_datadir cd_label\n";
  exit;
}

# $File::Find::name is the complete pathname to the file.
chdir $ARGV[0] or die "cannot cd to $ARGV[0]";
$findconf{"wanted"}=\&enterfile;
$findconf{"no_chdir"}=1;
find (\%findconf,".");

sub enterfile {
  $direntry=$File::Find::name;
  if ($direntry ne ".")
  {
    $direntry =~ s,^\./,,;
    $st=stat("$direntry");
    if($st->mode & 0040000)
    {
      if (! -d "$ARGV[1]/$direntry")
      {
        mkdir "$ARGV[1]/$direntry";
        print "mkdir $ARGV[1]/$direntry\n";
      }
    } else { 
      open O,">$ARGV[1]/$direntry";
      print O "label: $ARGV[2]\n";
      print O "name: $direntry\n";
      print O "size: ".$st->size."\n";
      printf O "mode: %o\n",$st->mode;
      print O "uid: ".$st->uid."\n";
      print O "gid: ".$st->gid."\n";
      print O "mtime: ".$st->mtime."\n";
      print O "atime: ".$st->atime."\n";
      print O "ctime: ".$st->ctime."\n";
      close O;
#      print "$direntry -> $ARGV[1]/$direntry\n";
      chmod 01600, "$ARGV[1]/$direntry";
    }
  }
}
