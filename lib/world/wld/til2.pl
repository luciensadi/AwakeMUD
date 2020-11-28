#!/usr/bin/perl
use IO::Socket;

$filename=$ARGV[0];

rename($filename, "temp.temp");
open(TEMP, "temp.temp");
open(OUT, '>>', $filename);
print "$filename\n";

while (!eof(TEMP)) {
  $line = TEMP->getline();
  if(index($line, "Desc:", 0) >= 0) {
    $lookfor = 1;
  }
  if(index($line, "~", 0) >= 0) {
    $lookfor = 0;
  }
  if(index($line, "Flags",0) >= 0 && $lookfor == 1) {
    $line = "~\n$line";
    $lookfor = 0;
    print "ADDED TILDE\n\r";
  }
  print OUT "$line";
}

unlink("temp.temp");
