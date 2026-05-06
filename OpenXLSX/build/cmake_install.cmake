# Install script for directory: /mnt/c/Users/garre/CLionProjects/cpu-simulator-SC0R9I0N/OpenXLSX

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/usr/local")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Install shared libraries without execute permission?
if(NOT DEFINED CMAKE_INSTALL_SO_NO_EXE)
  set(CMAKE_INSTALL_SO_NO_EXE "1")
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

# Set default install directory permissions.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "/usr/bin/objdump")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/OpenXLSX/headers" TYPE FILE FILES "/mnt/c/Users/garre/CLionProjects/cpu-simulator-SC0R9I0N/OpenXLSX/build/OpenXLSX-Exports.hpp")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/OpenXLSX/headers" TYPE FILE FILES
    "/mnt/c/Users/garre/CLionProjects/cpu-simulator-SC0R9I0N/OpenXLSX/headers/IZipArchive.hpp"
    "/mnt/c/Users/garre/CLionProjects/cpu-simulator-SC0R9I0N/OpenXLSX/headers/XLCell.hpp"
    "/mnt/c/Users/garre/CLionProjects/cpu-simulator-SC0R9I0N/OpenXLSX/headers/XLCellIterator.hpp"
    "/mnt/c/Users/garre/CLionProjects/cpu-simulator-SC0R9I0N/OpenXLSX/headers/XLCellRange.hpp"
    "/mnt/c/Users/garre/CLionProjects/cpu-simulator-SC0R9I0N/OpenXLSX/headers/XLCellReference.hpp"
    "/mnt/c/Users/garre/CLionProjects/cpu-simulator-SC0R9I0N/OpenXLSX/headers/XLCellValue.hpp"
    "/mnt/c/Users/garre/CLionProjects/cpu-simulator-SC0R9I0N/OpenXLSX/headers/XLColor.hpp"
    "/mnt/c/Users/garre/CLionProjects/cpu-simulator-SC0R9I0N/OpenXLSX/headers/XLColumn.hpp"
    "/mnt/c/Users/garre/CLionProjects/cpu-simulator-SC0R9I0N/OpenXLSX/headers/XLCommandQuery.hpp"
    "/mnt/c/Users/garre/CLionProjects/cpu-simulator-SC0R9I0N/OpenXLSX/headers/XLComments.hpp"
    "/mnt/c/Users/garre/CLionProjects/cpu-simulator-SC0R9I0N/OpenXLSX/headers/XLConstants.hpp"
    "/mnt/c/Users/garre/CLionProjects/cpu-simulator-SC0R9I0N/OpenXLSX/headers/XLContentTypes.hpp"
    "/mnt/c/Users/garre/CLionProjects/cpu-simulator-SC0R9I0N/OpenXLSX/headers/XLDateTime.hpp"
    "/mnt/c/Users/garre/CLionProjects/cpu-simulator-SC0R9I0N/OpenXLSX/headers/XLDocument.hpp"
    "/mnt/c/Users/garre/CLionProjects/cpu-simulator-SC0R9I0N/OpenXLSX/headers/XLDrawing.hpp"
    "/mnt/c/Users/garre/CLionProjects/cpu-simulator-SC0R9I0N/OpenXLSX/headers/XLException.hpp"
    "/mnt/c/Users/garre/CLionProjects/cpu-simulator-SC0R9I0N/OpenXLSX/headers/XLFormula.hpp"
    "/mnt/c/Users/garre/CLionProjects/cpu-simulator-SC0R9I0N/OpenXLSX/headers/XLIterator.hpp"
    "/mnt/c/Users/garre/CLionProjects/cpu-simulator-SC0R9I0N/OpenXLSX/headers/XLMergeCells.hpp"
    "/mnt/c/Users/garre/CLionProjects/cpu-simulator-SC0R9I0N/OpenXLSX/headers/XLProperties.hpp"
    "/mnt/c/Users/garre/CLionProjects/cpu-simulator-SC0R9I0N/OpenXLSX/headers/XLRelationships.hpp"
    "/mnt/c/Users/garre/CLionProjects/cpu-simulator-SC0R9I0N/OpenXLSX/headers/XLRow.hpp"
    "/mnt/c/Users/garre/CLionProjects/cpu-simulator-SC0R9I0N/OpenXLSX/headers/XLRowData.hpp"
    "/mnt/c/Users/garre/CLionProjects/cpu-simulator-SC0R9I0N/OpenXLSX/headers/XLSharedStrings.hpp"
    "/mnt/c/Users/garre/CLionProjects/cpu-simulator-SC0R9I0N/OpenXLSX/headers/XLSheet.hpp"
    "/mnt/c/Users/garre/CLionProjects/cpu-simulator-SC0R9I0N/OpenXLSX/headers/XLStyles.hpp"
    "/mnt/c/Users/garre/CLionProjects/cpu-simulator-SC0R9I0N/OpenXLSX/headers/XLTables.hpp"
    "/mnt/c/Users/garre/CLionProjects/cpu-simulator-SC0R9I0N/OpenXLSX/headers/XLWorkbook.hpp"
    "/mnt/c/Users/garre/CLionProjects/cpu-simulator-SC0R9I0N/OpenXLSX/headers/XLXmlData.hpp"
    "/mnt/c/Users/garre/CLionProjects/cpu-simulator-SC0R9I0N/OpenXLSX/headers/XLXmlFile.hpp"
    "/mnt/c/Users/garre/CLionProjects/cpu-simulator-SC0R9I0N/OpenXLSX/headers/XLXmlParser.hpp"
    "/mnt/c/Users/garre/CLionProjects/cpu-simulator-SC0R9I0N/OpenXLSX/headers/XLZipArchive.hpp"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/OpenXLSX" TYPE FILE FILES "/mnt/c/Users/garre/CLionProjects/cpu-simulator-SC0R9I0N/OpenXLSX/OpenXLSX.hpp")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "lib" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "/mnt/c/Users/garre/CLionProjects/cpu-simulator-SC0R9I0N/OpenXLSX/build/libOpenXLSX.a")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/OpenXLSX" TYPE FILE FILES
    "/mnt/c/Users/garre/CLionProjects/cpu-simulator-SC0R9I0N/OpenXLSX/OpenXLSXConfig.cmake"
    "/mnt/c/Users/garre/CLionProjects/cpu-simulator-SC0R9I0N/OpenXLSX/build/OpenXLSX/OpenXLSXConfigVersion.cmake"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/OpenXLSX/OpenXLSXTargets.cmake")
    file(DIFFERENT _cmake_export_file_changed FILES
         "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/OpenXLSX/OpenXLSXTargets.cmake"
         "/mnt/c/Users/garre/CLionProjects/cpu-simulator-SC0R9I0N/OpenXLSX/build/CMakeFiles/Export/c72cc94553a1a0c9b05f75dae42fb1d7/OpenXLSXTargets.cmake")
    if(_cmake_export_file_changed)
      file(GLOB _cmake_old_config_files "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/OpenXLSX/OpenXLSXTargets-*.cmake")
      if(_cmake_old_config_files)
        string(REPLACE ";" ", " _cmake_old_config_files_text "${_cmake_old_config_files}")
        message(STATUS "Old export file \"$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/OpenXLSX/OpenXLSXTargets.cmake\" will be replaced.  Removing files [${_cmake_old_config_files_text}].")
        unset(_cmake_old_config_files_text)
        file(REMOVE ${_cmake_old_config_files})
      endif()
      unset(_cmake_old_config_files)
    endif()
    unset(_cmake_export_file_changed)
  endif()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/OpenXLSX" TYPE FILE FILES "/mnt/c/Users/garre/CLionProjects/cpu-simulator-SC0R9I0N/OpenXLSX/build/CMakeFiles/Export/c72cc94553a1a0c9b05f75dae42fb1d7/OpenXLSXTargets.cmake")
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^()$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/OpenXLSX" TYPE FILE FILES "/mnt/c/Users/garre/CLionProjects/cpu-simulator-SC0R9I0N/OpenXLSX/build/CMakeFiles/Export/c72cc94553a1a0c9b05f75dae42fb1d7/OpenXLSXTargets-noconfig.cmake")
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT)
  set(CMAKE_INSTALL_MANIFEST "install_manifest_${CMAKE_INSTALL_COMPONENT}.txt")
else()
  set(CMAKE_INSTALL_MANIFEST "install_manifest.txt")
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
file(WRITE "/mnt/c/Users/garre/CLionProjects/cpu-simulator-SC0R9I0N/OpenXLSX/build/${CMAKE_INSTALL_MANIFEST}"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
