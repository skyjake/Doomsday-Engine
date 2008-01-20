## deng-ng test scripts. 
## Copyright © 2008 Jamie Jones <jamie_jones_au@yahoo.com.au>
## This file is licensed under the GNU GPLv3 or any later versions,
##
## the deng build scripts is free software; you can redistribute it and/or
## modify it under the terms of the GNU General Public License as published by
## the Free Software Foundation; either version 3 of the License, or
## (at your option) any later version.
##
## the deng build scripts is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with the deng build scripts; if not, write to the Free Software
## Foundation, Inc., 51 Franklin St, Fifth Floor, 
## Boston, MA  02110-1301  USA
##
################################################################################
PROJECT(compare_float_tests C CXX)
CMAKE_MINIMUM_REQUIRED (VERSION 2.4.5 FATAL_ERROR)

################################################################################
######################## float_compare_basic  ##################################
INCLUDE_DIRECTORIES (${CMAKE_SOURCE_DIR}/utilities/include )
ADD_EXECUTABLE (test_float_compare_basic test_float_compare_basic.cpp ${CMAKE_SOURCE_DIR}/utilities/src/compare_float.cpp)
ADD_TEST (float_compare_basic test_float_compare_basic)
SET_TESTS_PROPERTIES (float_compare_basic PROPERTIES FAIL_REGULAR_EXPRESSION "TEST FAILED" PASS_REGULAR_EXPRESSION "TEST PASSED" )


