#
# Copyright(c) 2024 Erik Boasson
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v. 2.0 which is available at
# http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
# v. 1.0 which is available at
# http://www.eclipse.org/org/documents/edl-v10.php.
#
# SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
#
project(helloworld LANGUAGES C CXX)
cmake_minimum_required(VERSION 3.16)

set(CMAKE_CXX_STANDARD 20)

find_package(CycloneDDS REQUIRED)
find_package(CycloneDDS-CXX REQUIRED)

idlcxx_generate(TARGET data_lib FILES data.idl)
add_executable(talker talker.cpp)
target_link_libraries(talker CycloneDDS-CXX::ddscxx data_lib)

add_executable(listener listener.cpp)
target_link_libraries(listener CycloneDDS-CXX::ddscxx data_lib)
