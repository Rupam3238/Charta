#!/bin/sh
$XGETTEXT $(find . -name "*.cpp" -o -name "*.h") -o $podir/charta_xps.pot
