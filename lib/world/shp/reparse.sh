#!/bin/bash
for file in `ls *.shp`
  do
   sed 's/END/OFF/g' $file >> $file.2
   sed 's/BREAK/END/g' $file.2 >> $file.3
   rm $file
   sed 's/OFF/BREAK/g' $file.3 >> $file
done
