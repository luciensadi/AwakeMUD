#!/bin/bash
for file in `ls *.obj`
  do
   perl tilparse.pl $file
done
