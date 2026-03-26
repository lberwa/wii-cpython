# Install script for directory: /opt/devkitpro/extras/cpython/3.15.0a7/curl/mbedtls/tf-psa-crypto

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/opt/devkitpro/extras/cpython/3.15.0a7/build-wii/curl/mbedtls/install-wii")
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

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "TRUE")
endif()

# Set default install directory permissions.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "/opt/devkitpro/devkitPPC/bin/powerpc-eabi-objdump")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/TF-PSA-Crypto" TYPE FILE FILES
    "/opt/devkitpro/extras/cpython/3.15.0a7/build-wii/curl/mbedtls/build-wii/tf-psa-crypto/cmake/TF-PSA-CryptoConfig.cmake"
    "/opt/devkitpro/extras/cpython/3.15.0a7/build-wii/curl/mbedtls/build-wii/tf-psa-crypto/cmake/TF-PSA-CryptoConfigVersion.cmake"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/TF-PSA-Crypto/TF-PSA-CryptoTargets.cmake")
    file(DIFFERENT _cmake_export_file_changed FILES
         "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/TF-PSA-Crypto/TF-PSA-CryptoTargets.cmake"
         "/opt/devkitpro/extras/cpython/3.15.0a7/build-wii/curl/mbedtls/build-wii/tf-psa-crypto/CMakeFiles/Export/09558911d5a5b54b9ef304c36fb1e8b6/TF-PSA-CryptoTargets.cmake")
    if(_cmake_export_file_changed)
      file(GLOB _cmake_old_config_files "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/TF-PSA-Crypto/TF-PSA-CryptoTargets-*.cmake")
      if(_cmake_old_config_files)
        string(REPLACE ";" ", " _cmake_old_config_files_text "${_cmake_old_config_files}")
        message(STATUS "Old export file \"$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/TF-PSA-Crypto/TF-PSA-CryptoTargets.cmake\" will be replaced.  Removing files [${_cmake_old_config_files_text}].")
        unset(_cmake_old_config_files_text)
        file(REMOVE ${_cmake_old_config_files})
      endif()
      unset(_cmake_old_config_files)
    endif()
    unset(_cmake_export_file_changed)
  endif()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/TF-PSA-Crypto" TYPE FILE FILES "/opt/devkitpro/extras/cpython/3.15.0a7/build-wii/curl/mbedtls/build-wii/tf-psa-crypto/CMakeFiles/Export/09558911d5a5b54b9ef304c36fb1e8b6/TF-PSA-CryptoTargets.cmake")
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^()$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/TF-PSA-Crypto" TYPE FILE FILES "/opt/devkitpro/extras/cpython/3.15.0a7/build-wii/curl/mbedtls/build-wii/tf-psa-crypto/CMakeFiles/Export/09558911d5a5b54b9ef304c36fb1e8b6/TF-PSA-CryptoTargets-noconfig.cmake")
  endif()
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for each subdirectory.
  include("/opt/devkitpro/extras/cpython/3.15.0a7/build-wii/curl/mbedtls/build-wii/tf-psa-crypto/framework/cmake_install.cmake")
  include("/opt/devkitpro/extras/cpython/3.15.0a7/build-wii/curl/mbedtls/build-wii/tf-psa-crypto/include/cmake_install.cmake")
  include("/opt/devkitpro/extras/cpython/3.15.0a7/build-wii/curl/mbedtls/build-wii/tf-psa-crypto/drivers/cmake_install.cmake")
  include("/opt/devkitpro/extras/cpython/3.15.0a7/build-wii/curl/mbedtls/build-wii/tf-psa-crypto/core/cmake_install.cmake")
  include("/opt/devkitpro/extras/cpython/3.15.0a7/build-wii/curl/mbedtls/build-wii/tf-psa-crypto/pkgconfig/cmake_install.cmake")

endif()

