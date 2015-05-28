#!/bin/bash

for i in `find -type d -name BUILD -prune -o -name '*.?xx' -print`; do
	uncrustify -c uncrustify.cfg --replace --no-backup $i
done
