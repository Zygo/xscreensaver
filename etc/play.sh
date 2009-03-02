#!/bin/sh

[ -c /dev/audio ] && cat $1 > /dev/audio
