# Install script for directory: /opt/devkitpro/extras/cpython/3.15.0a7/curl/mbedtls/include

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
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/mbedtls" TYPE FILE PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ FILES
    "/opt/devkitpro/extras/cpython/3.15.0a7/curl/mbedtls/include/mbedtls/build_info.h"
    "/opt/devkitpro/extras/cpython/3.15.0a7/curl/mbedtls/include/mbedtls/debug.h"
    "/opt/devkitpro/extras/cpython/3.15.0a7/curl/mbedtls/include/mbedtls/error.h"
    "/opt/devkitpro/extras/cpython/3.15.0a7/curl/mbedtls/include/mbedtls/mbedtls_config.h"
    "/opt/devkitpro/extras/cpython/3.15.0a7/curl/mbedtls/include/mbedtls/net_sockets.h"
    "/opt/devkitpro/extras/cpython/3.15.0a7/curl/mbedtls/include/mbedtls/oid.h"
    "/opt/devkitpro/extras/cpython/3.15.0a7/curl/mbedtls/include/mbedtls/pkcs7.h"
    "/opt/devkitpro/extras/cpython/3.15.0a7/curl/mbedtls/include/mbedtls/ssl.h"
    "/opt/devkitpro/extras/cpython/3.15.0a7/curl/mbedtls/include/mbedtls/ssl_cache.h"
    "/opt/devkitpro/extras/cpython/3.15.0a7/curl/mbedtls/include/mbedtls/ssl_ciphersuites.h"
    "/opt/devkitpro/extras/cpython/3.15.0a7/curl/mbedtls/include/mbedtls/ssl_cookie.h"
    "/opt/devkitpro/extras/cpython/3.15.0a7/curl/mbedtls/include/mbedtls/ssl_ticket.h"
    "/opt/devkitpro/extras/cpython/3.15.0a7/curl/mbedtls/include/mbedtls/timing.h"
    "/opt/devkitpro/extras/cpython/3.15.0a7/curl/mbedtls/include/mbedtls/version.h"
    "/opt/devkitpro/extras/cpython/3.15.0a7/curl/mbedtls/include/mbedtls/x509.h"
    "/opt/devkitpro/extras/cpython/3.15.0a7/curl/mbedtls/include/mbedtls/x509_crl.h"
    "/opt/devkitpro/extras/cpython/3.15.0a7/curl/mbedtls/include/mbedtls/x509_crt.h"
    "/opt/devkitpro/extras/cpython/3.15.0a7/curl/mbedtls/include/mbedtls/x509_csr.h"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/mbedtls/private" TYPE FILE PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ FILES
    "/opt/devkitpro/extras/cpython/3.15.0a7/curl/mbedtls/include/mbedtls/private/config_adjust_ssl.h"
    "/opt/devkitpro/extras/cpython/3.15.0a7/curl/mbedtls/include/mbedtls/private/config_adjust_x509.h"
    )
endif()

