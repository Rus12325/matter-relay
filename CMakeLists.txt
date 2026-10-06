cmake_minimum_required(VERSION 3.16.0)
set(EXTRA_COMPONENT_DIRS managed_components/esp-matter)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(matter-relay)
