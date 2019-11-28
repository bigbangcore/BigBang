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

find_path(RocksDB_INCLUDE_DIR
    NAMES rocksdb/db.h
    PATHS ${ROCKSDB_ROOT_DIR} ${ROCKSDB_ROOT_DIR}/include
)

find_library(RocksDB_LIBRARY
    NAMES rocksdb
    PATHS ${ROCKSDB_ROOT_DIR} ${ROCKSDB_ROOT_DIR}/lib
)

if (UNIX)
    if (APPLE)
        # bzip2
        FIND_PATH(BZip2_ROOT_DIR
            NAMES include/bzlib.h
            PATHS ${root_path}
            PATH_SUFFIXES bzip2
            NO_DEFAULT_PATH
        )
        FIND_PATH(BZip2_INCLUDE_DIR
            NAMES bzlib.h 
            HINTS ${BZip2_ROOT_DIR}/include
        )
        FIND_LIBRARY(BZip2_LIBRARY
            NAMES bz2
            HINTS ${BZip2_ROOT_DIR}/lib
        )
        if(NOT BZip2_LIBRARY)
            message(FATAL_ERROR "bzip2 library not found")
        endif()

        # lz4
        FIND_PATH(LZ4_ROOT_DIR
            NAMES include/lz4.h
            PATHS ${root_path}
            PATH_SUFFIXES lz4
            NO_DEFAULT_PATH
        )
        FIND_PATH(LZ4_INCLUDE_DIR
            NAMES lz4.h
            HINTS ${LZ4_ROOT_DIR}/include
        )
        FIND_LIBRARY(LZ4_LIBRARY
            NAMES lz4
            HINTS ${LZ4_ROOT_DIR}/lib
        )
        if(NOT LZ4_LIBRARY)
            message(FATAL_ERROR "lz4 library not found")
        endif()

        # zstd
        FIND_PATH(ZSTD_ROOT_DIR
            NAMES include/zstd.h
            PATHS ${root_path}
            PATH_SUFFIXES zstd
            NO_DEFAULT_PATH
        )
        FIND_PATH(ZSTD_INCLUDE_DIR
            NAMES zstd.h
            HINTS ${ZSTD_ROOT_DIR}/include
        )
        FIND_LIBRARY(ZSTD_LIBRARY
            NAMES zstd
            HINTS ${ZSTD_ROOT_DIR}/lib
        )
        if(NOT ZSTD_LIBRARY)
            message(FATAL_ERROR "zstd library not found")
        endif()

        # zlib
        FIND_PATH(ZLIB_ROOT_DIR
            NAMES include/zlib.h
            PATHS ${root_path}
            PATH_SUFFIXES zlib
            NO_DEFAULT_PATH
        )
        FIND_PATH(ZLIB_INCLUDE_DIR
            NAMES zlib.h
            HINTS ${ZLIB_ROOT_DIR}/include
        )
        FIND_LIBRARY(ZLIB_LIBRARY
            NAMES z
            HINTS ${ZLIB_ROOT_DIR}/lib
        )
        if(NOT ZLIB_LIBRARY)
            message(FATAL_ERROR "zlib library not found")
        endif()

        set(RocksDB_LIBRARY ${RocksDB_LIBRARY} ${BZip2_LIBRARY} ${LZ4_LIBRARY} ${ZSTD_LIBRARY} ${ZLIB_LIBRARY})

    else()
        find_package(BZip2 1.0.6 REQUIRED)
        find_package(ZLIB 1.2.0 REQUIRED)
        set(RocksDB_LIBRARY ${RocksDB_LIBRARY} ${BZip2_LIBRARIES} ${ZLIB_LIBRARIES})
    endif()
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(RocksDB
    DEFAULT_MSG
        RocksDB_INCLUDE_DIR
        RocksDB_LIBRARY
)

if(ROCKSDB_FOUND)
  message(STATUS "Found RocksDB  (include: ${RocksDB_INCLUDE_DIR}, library: ${RocksDB_LIBRARY})")
  mark_as_advanced(RocksDB_INCLUDE_DIR RocksDB_LIBRARY)
endif()