#
# Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/boostorg/beast
#

option (Boost_USE_STATIC_LIBS "Use static libraries for boost" ON)

set (BOOST_COMPONENTS coroutine context filesystem program_options system thread)
find_package (Boost 1.58.0 REQUIRED COMPONENTS ${BOOST_COMPONENTS})
link_directories(${Boost_LIBRARY_DIRS})

GroupSources(include/boost/beast beast)
GroupSources(example/common common)
GroupSources(example/websocket/client/flat_write_test "/")

add_executable (websocket-client-flat-write-sync-ssl
    ${BOOST_BEAST_INCLUDES}
    ${PROJECT_SOURCE_DIR}/example/common/root_certificates.hpp
    ${PROJECT_SOURCE_DIR}/example/common/flat_write_stream.hpp
    Jamfile
    websocket_client_flat_write_sync_ssl.cpp
)

target_link_libraries (websocket-client-flat-write-sync-ssl
        ${OPENSSL_LIBRARIES}
        ${Boost_LIBRARIES}
        Threads::Threads
)

add_executable (websocket-client-flat-write-coro-ssl
        ${BOOST_BEAST_INCLUDES}
        ${PROJECT_SOURCE_DIR}/example/common/root_certificates.hpp
        ${PROJECT_SOURCE_DIR}/example/common/flat_write_stream.hpp
        Jamfile
        websocket_client_flat_write_coro_ssl.cpp
)

target_link_libraries (websocket-client-flat-write-coro-ssl
        ${OPENSSL_LIBRARIES}
        ${Boost_LIBRARIES}
        Threads::Threads
)
