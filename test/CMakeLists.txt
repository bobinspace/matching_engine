include_directories(${MatchingEngine_SOURCE_DIR}/src)
add_executable(me_test test.cpp catch.hpp)
target_link_libraries(me_test me)
target_compile_options(me_test PRIVATE -Wall -Wextra -Wpedantic -Werror -Wno-missing-field-initializers)
#add_test(NAME TestMatchingEngine COMMAND me_test)