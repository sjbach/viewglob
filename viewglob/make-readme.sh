#! /bin/sh
# This uses README.ms, which I took from the giFTcurs package.  The output will # need to be hand-modified a bit to get rid of page numbers and extra spacing.

groff -Tlatin1 -P-cub -ms README.ms

