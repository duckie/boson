#!/usr/bin/env bash

SCRIPTPATH="$( cd "$(dirname "$0")" ; pwd -P )"

# Core repo
CORE_REPO="https://github.com/facebook/folly"
CORE_DOWNLOAD=${SCRIPTPATH}/download/folly
if [ -d ${CORE_DOWNLOAD} ]; then
  cd ${CORE_DOWNLOAD}
  git fetch
else
  mkdir -p $(dirname ${CORE_DOWNLOAD})
  git clone ${CORE_REPO} ${CORE_DOWNLOAD}
fi
cd ${CORE_DOWNLOAD}
git checkout "v2016.09.12.01"
mkdir -p ${SCRIPTPATH}/folly
rsync -avz --delete --files-from=${SCRIPTPATH}/file_list.txt ${CORE_DOWNLOAD}/folly ${SCRIPTPATH}/folly
