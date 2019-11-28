# Find the RocksDB libraries
#
# The following variables are optionally searched for defaults
#  ROCKSDB_ROOT_DIR:    Base directory where all RocksDB components are found
#
# The following are set after configuration is done:
#  ROCKSDB_FOUND
#  RocksDB_INCLUDE_DIR
#  RocksDB_LIBRARIES

if(RocksDB_USE_STATIC_LIBS)
  set(_CMAKE_FIND_LIBRARY_SUFFIXES ${CMAKE_FIND_LIBRARY_SUFFIXES})
  set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
endif()

if(UNIX AND NOT APPLE)
  set(root_path "/opt/local/" "/usr/" "/usr/local/")
else()
  set(root_path "/usr/local/" "/usr/local/opt")
endif()

find_path(ROCKSDB_ROOT_DIR
    NAMES include/rocksdb
    PATHS ${root_path}
    PATH_SUFFIXES lib rocksdb
    NO_DEFAULT_PATH
)

find_path(RocksDB_INCLUDE_DIR NAMES rocksdb/db.h
                             PATHS ${ROCKSDB_ROOT_DIR} ${ROCKSDB_ROOT_DIR}/include)

find_library(RocksDB_LIBRARIES NAMES rocksdb
                              PATHS ${ROCKSDB_ROOT_DIR} ${ROCKSDB_ROOT_DIR}/lib)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(RocksDB DEFAULT_MSG RocksDB_INCLUDE_DIR RocksDB_LIBRARIES)

if(ROCKSDB_FOUND)
  message(STATUS "Found RocksDB  (include: ${RocksDB_INCLUDE_DIR}, library: ${RocksDB_LIBRARIES})")
  mark_as_advanced(RocksDB_INCLUDE_DIR RocksDB_LIBRARIES)
endif()