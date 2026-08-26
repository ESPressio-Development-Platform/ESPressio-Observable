cmake_minimum_required(VERSION 3.13)
project(ESPressioObservableTests LANGUAGES CXX)

option(ESPRESSIO_ENABLE_COVERAGE "Enable source coverage instrumentation" OFF)
option(ESPRESSIO_ENABLE_SANITIZERS "Enable address and undefined-behavior sanitizers" OFF)

add_executable(espressio_observable_tests test_observable.cpp)
target_include_directories(espressio_observable_tests PRIVATE ../src)
target_compile_features(espressio_observable_tests PRIVATE cxx_std_14)

add_executable(espressio_observable_no_rtti_tests test_observable_no_rtti.cpp)
target_include_directories(espressio_observable_no_rtti_tests PRIVATE ../src)
target_compile_features(espressio_observable_no_rtti_tests PRIVATE cxx_std_14)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(espressio_observable_tests PRIVATE
        -Wall -Wextra -Wpedantic -Werror
    )
    target_compile_options(espressio_observable_no_rtti_tests PRIVATE
        -Wall -Wextra -Wpedantic -Werror -fno-rtti
    )
elseif(MSVC)
    target_compile_options(espressio_observable_tests PRIVATE /W4 /WX)
    target_compile_options(espressio_observable_no_rtti_tests PRIVATE /W4 /WX /GR-)
endif()

find_package(Threads REQUIRED)
target_link_libraries(espressio_observable_tests PRIVATE Threads::Threads)
target_link_libraries(espressio_observable_no_rtti_tests PRIVATE Threads::Threads)

if(ESPRESSIO_ENABLE_COVERAGE)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(espressio_observable_tests PRIVATE -O0 -g --coverage)
        target_link_options(espressio_observable_tests PRIVATE --coverage)
    else()
        message(FATAL_ERROR "Coverage is supported only with GCC or Clang")
    endif()
endif()

if(ESPRESSIO_ENABLE_SANITIZERS)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(espressio_observable_tests PRIVATE
            -fsanitize=address,undefined -fno-omit-frame-pointer
        )
        target_link_options(espressio_observable_tests PRIVATE
            -fsanitize=address,undefined
        )
    else()
        message(FATAL_ERROR "Sanitizers are supported only with GCC or Clang")
    endif()
endif()

enable_testing()
add_test(NAME espressio_observable_tests COMMAND espressio_observable_tests)
add_test(NAME espressio_observable_no_rtti_tests COMMAND espressio_observable_no_rtti_tests)
