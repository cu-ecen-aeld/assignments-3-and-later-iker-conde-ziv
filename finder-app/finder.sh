#!/bin/bash

# Check input argument numbers
if [ "$#" -ne 2 ]; then
    echo "Two parameters are required"
    echo "1) a path to a directory on the filesystem"
    echo "2) a text string which will be searched within these files"
    exit 1
fi

filesdir=$1
searchstr=$2

# Check input directory exists
if [ ! -d "$filesdir" ]; then
    echo "Directory $filesdir does not exist."
    exit 1
fi

number_of_files=$(find "$filesdir" -type f | wc -l)
number_of_lines=$(grep -r "$searchstr" "$filesdir" | wc -l)

echo "The number of files are $number_of_files and the number of matching lines are $number_of_lines"
