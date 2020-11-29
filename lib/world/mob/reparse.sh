#!/bin/bash
for file in `ls *.mob`
  do
   perl tilparse.pl $file
done
