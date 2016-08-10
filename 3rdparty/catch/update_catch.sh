#!/usr/bin/env bash

SCRIPTPATH="$( cd "$(dirname "$0")" ; pwd -P )"
URL="https://raw.githubusercontent.com/philsquared/Catch/master/single_include/catch.hpp"
wget "${URL}" -O "${SCRIPTPATH}/catch.hpp"
