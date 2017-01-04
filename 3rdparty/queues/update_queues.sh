#!/usr/bin/env bash

SCRIPTPATH="$( cd "$(dirname "$0")" ; pwd -P )"

# Core repo
CORE_REPO="https://github.com/mstump/queues.git"
CORE_DOWNLOAD=${SCRIPTPATH}/download/queues
if [ -d ${CORE_DOWNLOAD} ]; then
  cd ${CORE_DOWNLOAD}
  git fetch
else
  mkdir -p $(dirname ${CORE_DOWNLOAD})
  git clone ${CORE_REPO} ${CORE_DOWNLOAD}
fi
cd ${CORE_DOWNLOAD}
git checkout "master"
mkdir -p ${SCRIPTPATH}/queues
rsync -avz --delete --files-from=${SCRIPTPATH}/file_list.txt ${CORE_DOWNLOAD}/ ${SCRIPTPATH}/queues/
