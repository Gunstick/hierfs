#!/usr/bin/perl
use File::stat;
use File::Basename;

if ($ENV{"HIERFS_DATA"} eq "")
{
  print "environ HIERFS_DATA not set\n";
  exit 1;
}
$CDmaxsize=700;   # MB
$HIERFS_DATA=$ENV{"HIERFS_DATA"};    # for native shell speakers
open DU,"du -s ".$ENV{"HIERFS_DATA"}."|" or die "problem getting disk space\n"; 
$FSused=<DU>;
close DU;
chomp $FSused;
$FSused=~s/^(\d+)\s+.*$/$1/;
if ($FSused < 716800 )
{
  print "$FSused is below 700MB\n";
  exit 0;
}
print "used space=$FSused, we should migrate!!!\n";

$workdir="/.hierfs".$$."tmpCD";
mkdir $ENV{"HIERFS_DATA"} . "/$workdir" or die "cannot create temp dir\n";    # temp dir for CD image
$s=$CDmaxsize*1024*1024/512;            # 700 MB in 512 byte blocks
# find oldest files and link into CD directory
open F,"find $HIERFS_DATA ! '(' -perm +01000 ')' -type f -printf '%T@ %b %p\n' |" or die "cannot start find subprocess\n";
while (<F>)
{
  chomp;
  if (! /$workdir/)
  {
print "found file $_\n";
    /^(\d+) (\d+) (.+)$/;
    push @times, $1;
    push @blocks, $2;
    push @names, $3;
  }
}
close F;

# this is theoretically an NP problem: the minimum bin packing problem
# but we have 2 additional infos
# 1) the order is not by size but ba oldest file
# 2) the is only one destination and we need to fill it up as much as possible
# so first sort by time and put ech file on CD until there is no
# more room. Continue looking for smaller files which may fit the
# remaining space until even a small file does not fit anymore.
# 'small file' being arbitrary chosen to be 40 block (20K)
foreach $idx (sort {  $times[$b] <=> $times[$a] } 0..$#times )
{
  $t=$times[$idx];
  $b=$blocks[$idx];
  $f=$names[$idx];
print "S=$s B=$b T=$f\n";
  if ( ($s-$b) > 0 )   # file fits on CD
  {
    $s=$s-$b;
print "L $f ".basename($f)."\n";
    link $f,"$HIERFS_DATA/$workdir/".basename($f);
    push @toCD, $f;
#    print O "$f\n";
  }
  else
  {      # file does not fit, so we try the next one
print "N $f ".basename($f)."\n";
    # if even a 20K file does not fit, stop looking, so avoiding to
    # collect all miniature files out of the harddisk
    if ($b < 40) { goto CDPREP } ;  
  }
}
CDPREP:
print "space left empty on CD: $s\n";
$label=`kdialog --inputbox "insert empty CDR and give label"`;
chomp $label;
if ( $label ne "" )
{
  # works only as root?
  system("mkisofs -V '$label' -r '$HIERFS_DATA/$workdir' | cdrecord -v dev=0,0,0 -data -");
  if ( ($?>>8) == 0 )
  {
    while (@toCD)
    {
      chomp;
      $file=$_;
#      print "stat $file\n";
      if ($st=stat($file))
      {
#      print "rm $file\n";
        $info="label: $label\n".
          "name: ".basename($file)."\n".
          "size: ".$st->size."\n".
          sprintf("mode: %o\n",$st->mode).
          "uid: ".$st->uid."\n".
          "gid: ".$st->gid."\n".
          "mtime: ".$st->mtime."\n".
          "atime: ".$st->atime."\n".
          "ctime: ".$st->ctime."\n";
        if (!unlink $file)
        { 
          print "could not remove $file\n";
        } elsif (!open O,">$file")
        {
          print "could not create $file info\n";
          link "$HIERFS_DATA/$workdir/".basename($f),$f or print "and impossible to recreate original.\n";
        } elsif (! print O $info)
        {
          print "could not write $file info\n";
        } elsif (! close O)
        {
          print "could not close $file info\n";
        } elsif (! chmod 01600,$file)   # set T bit
        {
          print "problem setting T bit on $file\n";
        }
      }
      else
        print "access error to $file\n";
    }
  }
  else
  {
    print "cd creation error\n";
  }
}
# test CD before removing data?

# remove written image
print "cleanup\n";
opendir DIR,"$HIERFS_DATA/$workdir";
for (readdir(DIR))
{ 
  unlink "$HIERFS_DATA/$workdir/$_";
}
closedir DIR;
rmdir "$HIERFS_DATA/$workdir";

