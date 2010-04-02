#!/bin/sh

for d in `cat dirlist`
do
	echo creating $d
	mkdir -p $d
done
