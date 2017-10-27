#!/usr/bin/env bash

set -ex

base="$PWD"
deps="$base/deps"
inst="$deps/install"
export deps inst

osmo-clean-workspace.sh

mkdir "$deps" || true

osmo-build-dep.sh libosmocore

verify_value_string_arrays_are_terminated.py $(find . -name "*.[hc]")

export PKG_CONFIG_PATH="$inst/lib/pkgconfig:$PKG_CONFIG_PATH"
export LD_LIBRARY_PATH="$inst/lib"

set +x
echo
echo
echo
echo " =============================== osmo-sip-connector ==============================="
echo
set -x

autoreconf --install --force
./configure --enable-vty-tests --enable-external-tests
$MAKE $PARALLEL_MAKE
$MAKE check \
  || cat-testlogs.sh
$MAKE distcheck \
  || cat-testlogs.sh

osmo-clean-workspace.sh
