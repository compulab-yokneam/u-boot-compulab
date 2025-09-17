#!/bin/bash

for f in lpddr*c; do
	echo $f
	grep '^.*[[:blank:]][^[:blank:]] = {' $f
done
