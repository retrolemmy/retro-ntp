cmake_minimum_required(VERSION 3.10)
project(NTPSet C)

add_application(NTPSet
    ntpset.c
    ../common/timecalc.c
    ntpset.r
    CREATOR "NTPs"
)
target_include_directories(NTPSet PRIVATE ../common)
set_target_properties(NTPSet PROPERTIES COMPILE_OPTIONS "-Os;-Wall;-Wextra;-Wno-multichar;-Wno-pointer-sign")
# strip unused code and keep the binary small
set_target_properties(NTPSet PROPERTIES LINK_FLAGS "-Wl,-gc-sections")
