
PROJECT(flattenCellmlModel)
cmake_minimum_required(VERSION 2.8)

# Options
#OPTION(DEBUG
#  "Build this project with debugging turned on (default)"
#  ON)

# Add in the directory with the FindCellML module
SET(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${flattenCellmlModel_SOURCE_DIR})

IF(WIN32)
    SET(CMAKE_FIND_LIBRARY_SUFFIXES .lib .a ${CMAKE_FIND_LIBRARY_SUFFIXES})
ELSE(WIN32)
    SET(CMAKE_FIND_LIBRARY_SUFFIXES .a ${CMAKE_FIND_LIBRARY_SUFFIXES})
ENDIF(WIN32)

# Can use -DCMAKE_INCLUDE_PATH=$HOME/std-libs/physiome/include -DCMAKE_LIBRARY_PATH=$HOME/std-libs/physiome/lib
# with cmake command to add extra default search locations or set equivalent
# ENV variables so the system path specifications get properly converted to cmake
# path lists.
FIND_PACKAGE(CellML REQUIRED QUIET)
FIND_PACKAGE(LibXml2 REQUIRED QUIET)
FIND_PACKAGE(ZLIB REQUIRED QUIET)

# Set compiler flags
if (WIN32)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4290 /wd4091")
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
else(WIN32)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
    add_definitions(-std=c++11 -Wall -Werror)
endif(WIN32)
ADD_DEFINITIONS(
   ${LIBXML2_DEFINITIONS}
)
# Default to debug build type
#SET(CMAKE_BUILD_TYPE Debug)
# Make a new build type
#SET(CMAKE_BUILD_TYPE distribution)
#SET(CMAKE_CXX_FLAGS_DISTRIBUTION "-O3")
#SET(CMAKE_C_FLAGS_DISTRIBUTION "-O3")

# Global includes?
INCLUDE_DIRECTORIES(
  ${CMAKE_SOURCE_DIR}/src
  ${CELLML_INCLUDE_DIR}
  ${LIBXML2_INCLUDE_DIR}
  ${ZLIB_INCLUDE_DIR}
)

# Sources
SET(flattenCellmlModel_SRCS
  src/flattenCellmlModel.cpp
  src/VersionConverter.cpp
  src/ModelCompactor.cpp
  src/utils.cpp
  src/cellmlutils.cpp
  src/xmlutils.cpp
)

set(EXECUTABLE_NAME "flattenCellmlModel")
ADD_EXECUTABLE(${EXECUTABLE_NAME} ${flattenCellmlModel_SRCS})
TARGET_LINK_LIBRARIES(${EXECUTABLE_NAME}
  ${CELLML_LIBRARIES}
  ${LIBXML2_LIBRARIES}
  ${ZLIB_LIBRARIES}
)
# need this to get things working on Linux?
set_target_properties(${EXECUTABLE_NAME} PROPERTIES
    INSTALL_RPATH "\$ORIGIN"
)

# add a target to generate API documentation with Doxygen
find_package(Doxygen)
if(DOXYGEN_FOUND)
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile @ONLY)
  add_custom_target(docs ${DOXYGEN_EXECUTABLE}
    ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/
    COMMENT "Generating API documentation with Doxygen" VERBATIM
  )
endif(DOXYGEN_FOUND)
