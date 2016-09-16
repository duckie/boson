#!/usr/bin/env bash


SCRIPTPATH="$( cd "$(dirname "$0")" ; pwd -P )"

# Core repo
CORE_REPO="https://github.com/boostorg/core"
CORE_DOWNLOAD=${SCRIPTPATH}/download/core
if [ -d ${CORE_DOWNLOAD} ]; then
  cd ${CORE_DOWNLOAD}
  git fetch
else
  mkdir -p $(dirname ${CORE_DOWNLOAD})
  git clone ${CORE_REPO} ${CORE_DOWNLOAD}
fi
cd ${CORE_DOWNLOAD}
git checkout "boost-1.61.0"
mkdir -p ${SCRIPTPATH}/include/boost/
rsync -avz --delete ${CORE_DOWNLOAD}/include/boost/ ${SCRIPTPATH}/include/boost/
