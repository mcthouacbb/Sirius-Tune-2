cmake_minimum_required(VERSION 3.7...3.26)

# Allow IDE Source Tree Visualization
if(${CMAKE_VERSION} VERSION_LESS 3.26)
	set_property(GLOBAL PROPERTY USE_FOLDERS ON)
else()
	cmake_policy(SET CMP0143 NEW)
endif()

project(Sirius-Tune-2)

set(SRCS
    "src/sirius/util/enum_array.h"
    "src/sirius/util/multi_array.h"
    "src/sirius/util/murmur.h"
    "src/sirius/util/prng.h"
    "src/sirius/attacks.cpp"
    "src/sirius/attacks.h"
    "src/sirius/bitboard.h"
    "src/sirius/board.cpp"
    "src/sirius/board.h"
    "src/sirius/castling.h"
    "src/sirius/cuckoo.cpp"
    "src/sirius/cuckoo.h"
    "src/sirius/defs.h"
    "src/sirius/movegen.cpp"
    "src/sirius/movegen.h"
    "src/sirius/zobrist.h"

    "src/dataset.cpp"
    "src/dataset.h"
    "src/eval_constants.h"
    "src/eval_fn.cpp"
    "src/eval_fn.h"
    "src/main.cpp"
    "src/settings.h"
    "src/thread_pool.cpp"
    "src/thread_pool.h"
    "src/tune.cpp"
    "src/tune.h"
)

add_executable(tune ${SRCS})

target_compile_features(tune PRIVATE cxx_std_20)
target_include_directories(tune PRIVATE "external")

# for Visual Studio/MSVC
set_target_properties(tune PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SRCS})
