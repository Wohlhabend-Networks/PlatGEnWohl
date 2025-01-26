function(add_libarchive_target)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(ENABLE_MBEDTLS OFF CACHE BOOL "" FORCE)
    set(ENABLE_OPENSSL OFF CACHE BOOL "" FORCE)
    set(ENABLE_LIBB2 OFF CACHE BOOL "" FORCE)

    # enable LZ4 for extraction
    set(ENABLE_LZ4 ON CACHE BOOL "" FORCE)

    set(ENABLE_LZO OFF CACHE BOOL "" FORCE)
    set(ENABLE_LZMA ON CACHE BOOL "" FORCE)
    set(ENABLE_ZSTD OFF CACHE BOOL "" FORCE)

    set(ENABLE_ZLIB ON CACHE BOOL "" FORCE)
    set(ENABLE_BZip2 OFF CACHE BOOL "" FORCE)
    set(ENABLE_LIBXML2 OFF CACHE BOOL "" FORCE)
    set(ENABLE_EXPAT OFF CACHE BOOL "" FORCE)
    set(ENABLE_PCREPOSIX OFF CACHE BOOL "" FORCE)
    set(ENABLE_PCRE2POSIX OFF CACHE BOOL "" FORCE)
    set(ENABLE_LIBGCC OFF CACHE BOOL "" FORCE)
    set(ENABLE_CNG OFF CACHE BOOL "" FORCE)

    set(ENABLE_TAR OFF CACHE BOOL "" FORCE)
    set(ENABLE_TAR_SHARED OFF CACHE BOOL "" FORCE)
    set(ENABLE_CPIO OFF CACHE BOOL "" FORCE)
    set(ENABLE_CPIO_SHARED OFF CACHE BOOL "" FORCE)
    set(ENABLE_CAT OFF CACHE BOOL "" FORCE)
    set(ENABLE_CAT_SHARED OFF CACHE BOOL "" FORCE)
    set(ENABLE_UNZIP OFF CACHE BOOL "" FORCE)
    set(ENABLE_UNZIP_SHARED OFF CACHE BOOL "" FORCE)
    set(ENABLE_XATTR OFF CACHE BOOL "" FORCE)
    set(ENABLE_ACL OFF CACHE BOOL "" FORCE)
    set(ENABLE_ICONV OFF CACHE BOOL "" FORCE)
    set(ENABLE_TEST OFF CACHE BOOL "" FORCE)
    set(ENABLE_COVERAGE OFF CACHE BOOL "" FORCE)
    set(ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
    set(ENABLE_WERROR OFF CACHE BOOL "" FORCE)

    get_target_property(LZ4_INCLUDE_DIR lz4_static INTERFACE_INCLUDE_DIRECTORIES)
    set(LZ4_LIBRARY "$<TARGET_FILE:lz4_static>")

    add_subdirectory(3rdparty/libarchive EXCLUDE_FROM_ALL)

    add_dependencies(archive_static lz4_static)
endfunction()

add_libarchive_target()
