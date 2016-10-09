#!/usr/bin/env bash

SCRIPTPATH="$( cd "$(dirname "$0")" ; pwd -P )"

# Core repo
CORE_REPO="https://github.com/chaoran/fast-wait-free-queue"
CORE_DOWNLOAD=${SCRIPTPATH}/download/wfqueue
if [ -d ${CORE_DOWNLOAD} ]; then
  cd ${CORE_DOWNLOAD}
  git fetch
else
  mkdir -p $(dirname ${CORE_DOWNLOAD})
  git clone ${CORE_REPO} ${CORE_DOWNLOAD}
fi
cd ${CORE_DOWNLOAD}
git checkout "master"
mkdir -p ${SCRIPTPATH}/wfqueue
rsync -avz --delete --files-from=${SCRIPTPATH}/file_list.txt ${CORE_DOWNLOAD}/ ${SCRIPTPATH}/wfqueue/
