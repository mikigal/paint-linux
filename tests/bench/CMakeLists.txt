# Timings on large pictures, only in optimised builds (cmake --preset bench, ctest --preset bench).
if(CMAKE_BUILD_TYPE MATCHES "Rel")
    paint_add_test(bench_large SOURCES bench_large.cpp LIBRARIES paint_app)
    set_tests_properties(bench_large PROPERTIES LABELS benchmark TIMEOUT 900)
endif()
