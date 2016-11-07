#!/usr/bin/env bash

SCRIPTPATH="$( cd "$(dirname "$0")" ; pwd -P )"
URL="https://raw.githubusercontent.com/duckie/json_backbone/master/include/json_backbone.hpp"
wget "${URL}" -O "${SCRIPTPATH}/../../src/boson/boson/external/json_backbone.hpp"
