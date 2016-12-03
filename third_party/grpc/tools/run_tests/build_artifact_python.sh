#!/bin/bash
# Copyright 2016, Google Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

set -ex

cd $(dirname $0)/../..

export GRPC_PYTHON_USE_CUSTOM_BDIST=0
export GRPC_PYTHON_BUILD_WITH_CYTHON=1
export PYTHON=${PYTHON:-python}
export PIP=${PIP:-pip}
export AUDITWHEEL=${AUDITWHEEL:-auditwheel}


# Build the source distribution first because MANIFEST.in cannot override
# exclusion of built shared objects among package resources (for some
# inexplicable reason).
${SETARCH_CMD} ${PYTHON} setup.py  \
    sdist

# Wheel has a bug where directories don't get excluded.
# https://bitbucket.org/pypa/wheel/issues/99/cannot-exclude-directory
${SETARCH_CMD} ${PYTHON} setup.py  \
    bdist_wheel

# Build gRPC tools package distribution
${PYTHON} tools/distrib/python/make_grpcio_tools.py

# Build gRPC tools package source distribution
${SETARCH_CMD} ${PYTHON} tools/distrib/python/grpcio_tools/setup.py  \
    sdist

# Build gRPC tools package binary distribution
CFLAGS="$CFLAGS -fno-wrapv" ${SETARCH_CMD} \
  ${PYTHON} tools/distrib/python/grpcio_tools/setup.py bdist_wheel

mkdir -p artifacts
if [ "$BUILD_MANYLINUX_WHEEL" != "" ]
then
  for wheel in dist/*.whl; do
    ${AUDITWHEEL} repair $wheel -w artifacts/
    rm $wheel
  done
  for wheel in tools/distrib/python/grpcio_tools/dist/*.whl; do
    ${AUDITWHEEL} repair $wheel -w artifacts/
    rm $wheel
  done
fi

# We need to use the built grpcio-tools/grpcio to compile the health proto
# Wheels are not supported by setup_requires/dependency_links, so we
# manually install the dependency.  Note we should only do this if we
# are in a docker image or in a virtualenv.
if [ "$BUILD_HEALTH_CHECKING" != "" ]
then
  ${PIP} install -rrequirements.txt
  ${PIP} install grpcio --no-index --find-links "file://${PWD}/artifacts/"
  ${PIP} install grpcio-tools --no-index --find-links "file://${PWD}/artifacts/"

  # Build gRPC health check source distribution
  ${SETARCH_CMD} ${PYTHON} src/python/grpcio_health_checking/setup.py \
      preprocess build_package_protos sdist
  cp -r src/python/grpcio_health_checking/dist/* artifacts
fi

cp -r dist/* artifacts
cp -r tools/distrib/python/grpcio_tools/dist/* artifacts
