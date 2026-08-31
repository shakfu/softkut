# Offline test harness for the host-agnostic softkut engine.
# Plain executable (no Max dependency); links the softcut static library.

find_package(Threads REQUIRED)   # the concurrent-poll test spawns a thread

add_executable(softkut_test test_engine.cpp)
target_compile_features(softkut_test PRIVATE cxx_std_14)
target_include_directories(softkut_test PRIVATE "${SOFTKUT_INCLUDE}" "${SOFTCUT_INCLUDE}")
target_link_libraries(softkut_test PRIVATE softcut Threads::Threads)

add_test(NAME softkut_test COMMAND softkut_test)
