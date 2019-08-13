#!/bin/bash

if [ ! -d "${HOME}/.bigbang/" ]; then
    mkdir -p ${HOME}/.bigbang/
fi

if [ ! -f "${HOME}/.bigbang/bigbang.conf" ]; then
    cp /bigbang.conf ${HOME}/.bigbang/bigbang.conf
fi

exec "$@"
