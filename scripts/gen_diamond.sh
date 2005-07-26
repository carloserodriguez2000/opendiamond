#!/bin/sh

RELEASE_NAME=diamond-1.1.0-rc1
#
# Make sure we have cleaned the source tree
#
pushd ../src
make distclean
autoconf
popd

#
# Remove the old version in /tmp if it exists
#
rm -rf /tmp/"$RELEASE_NAME"

#
# copy over the whole tree
#
cp -r ../src /tmp/"$RELEASE_NAME"/

# remove the CVS directories
find /tmp/"$RELEASE_NAME" -name CVS -exec rm -rf {} \;

# remove diretories we don't want to ship

# remove autoconf side effects
rm -rf autom4te.cache


# generate the tar file
pushd /tmp
tar -czf /tmp/"$RELEASE_NAME".tgz "$RELEASE_NAME"

popd
cp /tmp/"$RELEASE_NAME".tgz .
