cmake_minimum_required(VERSION 3.7...3.26)

# Allow IDE Source Tree Visualization
if(${CMAKE_VERSION} VERSION_LESS 3.26)
	set_property(GLOBAL PROPERTY USE_FOLDERS ON)
else()
	cmake_policy(SET CMP0143 NEW)
endif()

project(Sirius-Tune-2)

set(SRCS
    "src/dataset.cpp"
    "src/dataset.h"
    "src/eval_fn.cpp"
    "src/eval_fn.h"
    "src/main.cpp"
    "src/tune.cpp"
    "src/tune.h"
)

add_executable(tune ${SRCS})

target_compile_features(tune PRIVATE cxx_std_20)
target_include_directories(tune PRIVATE "external")

# for Visual Studio/MSVC
set_target_properties(tune PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SRCS})
