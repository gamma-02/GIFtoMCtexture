cmake_minimum_required(VERSION 3.30)
project(png_stuff)

set(CMAKE_CXX_STANDARD 23)

find_package(PNG REQUIRED)
find_package(GIF REQUIRED)



add_executable(png_stuff1 main.cpp
        EasyGifReader.cpp
        EasyGifReader.h)

target_link_libraries(png_stuff1 PRIVATE PNG::PNG)
target_link_libraries(png_stuff1 PRIVATE GIF::GIF)





