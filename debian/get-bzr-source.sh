#!/bin/sh

set -e

PACKAGE=compiz
BZR_BRANCH=0.9.12
bzrurl=lp:compiz/${BZR_BRANCH}

CWD_DIR=${PWD}
GOS_DIR=${CWD_DIR}/get-orig-source

DEB_SOURCE=$(dpkg-parsechangelog 2>/dev/null | sed -n 's/^Source: //p')
DEB_VERSION=$(dpkg-parsechangelog 2>/dev/null | sed -n 's/^Version: //p')
UPSTREAM_VERSION=$(echo ${DEB_VERSION} | sed -r 's/^[0-9]*://;s/-[^-]*$$//;s/\+repack[0-9]*$//')
BZR_VERSION=$(echo ${UPSTREAM_VERSION} | sed -nr 's/^[0-9.:-~]+\+bzr([0-9]+)$$/\1/p')

if [ "${DEB_SOURCE}" != "${PACKAGE}" ]; then
	echo 'Please run this script from the sources root directory.'
	exit 1
fi

if [ -z ${BZR_VERSION} ]; then
	echo 'Please update the latest entry in the changelog to show a bzr revision.'
	exit 2
fi

rm -rf ${GOS_DIR}
mkdir ${GOS_DIR} && cd ${GOS_DIR}

# Download sources
bzr export --per-file-timestamps -r ${BZR_VERSION} ${DEB_SOURCE}-${UPSTREAM_VERSION} ${bzrurl}

# Clean-up...
cd ${GOS_DIR}/${DEB_SOURCE}-${UPSTREAM_VERSION}
rm -rf .gitignore debian

# Packing...
cd ${GOS_DIR}
find -L ${DEB_SOURCE}-${UPSTREAM_VERSION} -xdev -type f -print | LC_ALL=C sort \
| XZ_OPT="-6v" tar -caf "${CWD_DIR}/${DEB_SOURCE}_${UPSTREAM_VERSION}+repack.orig.tar.xz" -T- --owner=root --group=root --mode=a+rX

cd ${CWD_DIR} && rm -rf ${GOS_DIR}
