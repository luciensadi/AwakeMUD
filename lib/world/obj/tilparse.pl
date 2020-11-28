#!/usr/bin/perl
use IO::Socket;

$filename=$ARGV[0];

rename($filename, "temp.temp");
open(TEMP, "temp.temp");
open(OUT, '>>', $filename);
print "$filename\n";

while (!eof(TEMP)) {
  $line = TEMP->getline();
  if(index($line, "Keywords:") >= 0 && index($line, "~",0) > 3) {
    chop $line;
    chop $line;
    $line = "$line\n";
    print "REMOVED TILDE\n\r";
  }
  if(index($line, "Name:") >= 0 && index($line, "~",0) > 3) {
    chop $line;
    chop $line;
    $line = "$line\n";
    print "REMOVED TILDE\n\r";
  }

  print OUT "$line";
}

unlink("temp.temp");
