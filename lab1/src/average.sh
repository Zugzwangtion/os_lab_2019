#!/bin/bash

sum=0
for number in "$@"
do
	sum=$((sum+number))
done

count=$#

average=$((sum/count))

echo "Kolichestvo: $count"
echo "Srednee: $average"

