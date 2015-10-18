#!/bin/bash

for file in $(find . -name "*.html"); do
  sed -i -e 's/src="\(..\/\)\+/src="http:\/\/www.boost.org\/doc\/libs\/1_57_0\//g' $file
  sed -i -e 's/href="\(..\/\)\+/href="http:\/\/www.boost.org\/doc\/libs\/1_57_0\//g' $file
done
