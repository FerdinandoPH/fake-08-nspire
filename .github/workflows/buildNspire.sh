#!/bin/sh
set -ex

# The stepney141/ndless-docker image already puts the Ndless SDK toolchain
# (/opt/ndless-dev/Ndless/ndless-sdk/{bin,toolchain/install/bin}) on PATH.

cd /build_dir

# DEPLOYDIR in platform/nspire/Makefile points at the maintainer's local
# WSL path; override it to a writable scratch dir so the final `cp` succeeds.
mkdir -p /tmp/nspire-deploy
make nspire DEPLOYDIR=/tmp/nspire-deploy
