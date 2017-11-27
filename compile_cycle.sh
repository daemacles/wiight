BUILD=build

if [ ! -d "$BUILD" ]; then
  mkdir $BUILD
  cd $BUILD
  CC=clang CXX=clang++ cmake .. -DCMAKE_BUILD_TYPE=Debug
  cd ..
fi

while true; do
  echo
  echo "========================================================="
  echo
  inotifywait -e modify -r \
    src/ \
    include/ \
    2> /dev/null && \
    make -j4 -s -C $BUILD | egrep "warning:|error:"
  ctags-exuberant --recurse=yes
done
