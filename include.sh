#!/usr/bin/env bash

## GETS THE CURRENT MODULE ROOT DIRECTORY
MOD_1V1ARENA_ROOT="$( cd "$( dirname "${BASH_SOURCE[0]}" )/" && pwd )"

source "$MOD_1V1ARENA_ROOT/conf/conf.sh.dist"

if [ -f "$MOD_1V1ARENA_ROOT/conf/conf.sh" ]; then
    source "$MOD_1V1ARENA_ROOT/conf/conf.sh"
fi