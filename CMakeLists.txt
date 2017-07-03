cmake_minimum_required(VERSION 2.8.11)

project(emdlib)

cmake_policy(SET CMP0042 NEW)
cmake_policy(SET CMP0043 NEW)

set(CMAKE_INCUDE_CURRENT_DIR ON)

set(CMAKE_AUTOMOC ON)

set(CMAKE_PREFIX_PATH
  $ENV{QT_DIR}
  )

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

set(CMAKE_CXX_STANDARD 11)

find_package(Qt5Core)
find_package(Qt5Gui)
find_package(HDF5 REQUIRED)

IF (APPLE)
  # find_package(SZIP REQUIRED)
  find_package(ZLIB REQUIRED)
  set(ZIP_LIBS 
    #	        szip
    z
    )
ELSE (WIN32)
  set(ZIP_LIBS 
    $ENV{HDF5_ROOT_DIR_HINT}/lib/szip.lib
    $ENV{HDF5_ROOT_DIR_HINT}/lib/zlib.lib
    )
ENDIF (APPLE)

add_subdirectory(include)
add_subdirectory(src)

include_directories(
  include
  ${HDF5_INCLUDE_DIR}
  )

add_library(emd SHARED
  ${EMDLIB_SOURCES}
  ${EMDLIB_HEADERS}
  )

target_link_libraries(emd
  hdf5
  hdf5_cpp
  ${ZIP_LIBS}
  )

target_compile_definitions(emd PRIVATE BUILD_EMDLIB=1)

qt5_use_modules(emd Core Gui)

#install(TARGETS emd
# RUNTIME DESTINATION bin
#  COMPONENT dependencies
# )

