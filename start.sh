#!/bin/sh
# if hasn't mounted, mount
count=`ls mount|wc -l`
if [ $count -eq 0 ]
then
    mount a.img mount
fi
# make and start executable file
make
./main