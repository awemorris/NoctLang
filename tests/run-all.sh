#!/bin/sh

set -eu

echo 'NoctLang Test Suites'
echo

sh run-syntax.sh
sh run-thread.sh
sh run-httpserver.sh
sh run-webapp.sh

echo
echo 'All suites passed.'
