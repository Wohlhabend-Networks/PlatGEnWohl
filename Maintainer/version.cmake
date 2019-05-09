# Major
set(MAINTAINER_VERSION_1 0)
# Minor
set(MAINTAINER_VERSION_2 0)
# Revision
set(MAINTAINER_VERSION_3 1)
# Patch
set(MAINTAINER_VERSION_4 0)
# Type of version: "-alpha","-beta","-dev", or "" aka "release"
set(MAINTAINER_VERSION_REL "-alpha")

add_definitions(-DMAINTAINER_VERSION_1=${MAINTAINER_VERSION_1})
add_definitions(-DMAINTAINER_VERSION_2=${MAINTAINER_VERSION_2})
add_definitions(-DMAINTAINER_VERSION_3=${MAINTAINER_VERSION_3})
add_definitions(-DMAINTAINER_VERSION_4=${MAINTAINER_VERSION_4})
add_definitions(-DMAINTAINER_VERSION_REL=${MAINTAINER_VERSION_REL})

set(MAINTAINER_VERSION_STRING "${MAINTAINER_VERSION_1}.${MAINTAINER_VERSION_2}")

if(NOT ${MAINTAINER_VERSION_3} EQUAL 0 OR NOT ${MAINTAINER_VERSION_4} EQUAL 0)
    string(CONCAT MAINTAINER_VERSION_STRING "${MAINTAINER_VERSION_STRING}" ".${MAINTAINER_VERSION_3}")
endif()

if(NOT ${MAINTAINER_VERSION_4} EQUAL 0)
    string(CONCAT MAINTAINER_VERSION_STRING "${MAINTAINER_VERSION_STRING}" ".${MAINTAINER_VERSION_4}")
endif()

if(NOT "${MAINTAINER_VERSION_REL}" STREQUAL "")
    string(CONCAT MAINTAINER_VERSION_STRING "${MAINTAINER_VERSION_STRING}" "${MAINTAINER_VERSION_REL}")
endif()

message("== Maintainer version ${MAINTAINER_VERSION_STRING} ==")

