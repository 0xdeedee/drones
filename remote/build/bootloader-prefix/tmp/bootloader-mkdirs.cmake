# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/Users/deedee/devel/esp/esp-idf/components/bootloader/subproject"
  "/Users/deedee/devel/drones/remote/build/bootloader"
  "/Users/deedee/devel/drones/remote/build/bootloader-prefix"
  "/Users/deedee/devel/drones/remote/build/bootloader-prefix/tmp"
  "/Users/deedee/devel/drones/remote/build/bootloader-prefix/src/bootloader-stamp"
  "/Users/deedee/devel/drones/remote/build/bootloader-prefix/src"
  "/Users/deedee/devel/drones/remote/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/Users/deedee/devel/drones/remote/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/Users/deedee/devel/drones/remote/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
