#!/usr/bin/env bash
set -e
CXX=${CXX:-g++}
$CXX -std=c++11 mutex-ex.cpp -lpthread
$CXX -std=c++11 mtrace.cpp -lpthread -ldl -shared -fPIC -o libmtrace.so
