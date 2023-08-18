#!/bin/bash

# Check input argument numbers
if [ "$#" -ne 2 ]; then
    echo "Two parameters are required"
    echo "1) a full path to a file (including filename) on the filesystem"
    echo "2) a text string which will be written within this file"
    exit 1
fi

writefile=$1
writestr=$2

filedir=$(dirname "$writefile")

# Check input directory exists. If not, create it
if [ ! -d "$filedir" ]; then
    mkdir -p $filedir
    if [ $? -ne 0 ]; then
        echo "File directory could not be created"
        exit 1
    fi
fi

echo "$writestr" > $writefile
if [ $? -ne 0 ]; then
    echo "File could not be created"
    exit 1
fi