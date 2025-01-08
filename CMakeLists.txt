#       TOP LEVEL CMAKE FILE
#       --------------------

cmake_minimum_required(VERSION 3.5)

set(SDKCONFIG_DEFAULTS "sdkconfig.defaults")
set(IDF_TARGET "esp32s3")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(ulp_playground)
