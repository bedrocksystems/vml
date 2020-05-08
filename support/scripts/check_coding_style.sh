#!/bin/sh

# This file is intended to be used by the CI of gitlab. It will automatically check the coding
# style of this repo. It should be executed from the root of zeta.


is_cpp_file() {
    if [[ $1 == *.cpp || $1 == *.c || $1 == *.hpp || $1 == *.h ]]
    then
        return 0
    fi
    return 1
}

# Make sure that we have an origin/master branch, otherwise fetch it
git rev-parse --verify origin/master
if [ $? -ne 0 ]
then
    echo "Fetching origin master..."
    git fetch --depth 50 origin master
fi

BASE_COMMIT=`git merge-base HEAD origin/master`
MODIFIED_FILES=`git diff-tree --no-commit-id --name-only -r HEAD $BASE_COMMIT`

# check list of files
for f in $MODIFIED_FILES; do
    if is_cpp_file $f
    then
        if [ ! -e $f ]
        then
          echo "Skipping deleted file $f"
          continue
        fi

        echo "Checking file: $f"
        clang-format $f | diff - $f

        if [ $? -ne 0 ]
        then
            echo "Coding style issue detected."
            echo "Please update your patch and read: https://gitlab.com/bedrocksystems/zeta/wikis/Running-clang-format"
            exit 1
        fi
    else
        echo "Skipping file: $f"
    fi
done
