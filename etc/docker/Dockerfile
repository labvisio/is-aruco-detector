FROM labvisio/is-wire-base:1.1.6 as build
ARG IS_REMOTE="http://0.0.0.0:9300"

ADD conanfile.py .
RUN conan remote add is ${IS_REMOTE} \
 && conan install .                  \
   -s compiler=gcc                   \
   -s compiler.version=11            \
   -s compiler.libcxx=libstdc++11    \
   -s build_type=Release             \
   -b missing                        \
   -o:h is-wire/*:shared=True

ADD . /project
RUN cd /project                                                         \
 && ./build.sh                                                          \
 && mkdir -v -p /tmp/is/lib /tmp/is/bin                                 \
 && libs=`find build/ -type f -name '*.bin' -exec ldd {} \;             \
    | cut -d '(' -f 1 | cut -d '>' -f 2 | sort | uniq`                  \
 && for lib in $libs;                                                   \
    do                                                                  \
      dir="/tmp/is/lib`dirname $lib`";                                  \
      mkdir -v -p  $dir;                                                \
      cp --verbose $lib $dir;                                           \
    done                                                                \
 && cp --verbose `find build/ -type f -name '*.bin'` /tmp/is/bin/

FROM ubuntu:22.04
COPY --from=build /tmp/is/lib/root/ /root/
COPY --from=build /tmp/is/lib/project/ /project/
COPY --from=build /tmp/is/bin/ /
