# Matching engine library
add_library(me 
common_types.h
full_order_detail_handlers.cpp
full_order_detail_handlers.h
market.h
fill_allocator.h
orderbook.h
trade_event_handlers.cpp
trade_event_handlers.h
)
target_compile_options(me PRIVATE -Wall -Wextra -Wpedantic -Werror -Wno-missing-field-initializers)

# Main app that uses the matching engine library
add_executable(me_app main.cpp)
target_compile_options(me_app PRIVATE -Wall -Wextra -Wpedantic -Werror -Wno-missing-field-initializers)
target_link_libraries(me_app me)

# Static analysis
option(ENABLE_STATIC_ANALYSIS "Run static analysis: cppcheck" ON)
message(STATUS "ENABLE_STATIC_ANALYSIS=${ENABLE_STATIC_ANALYSIS}")
if(ENABLE_STATIC_ANALYSIS)
  find_program(cppcheck_path NAMES cppcheck REQUIRED)
  if(cppcheck_path)
    message(STATUS "Will run cppcheck at ${cppcheck_path}")
    set_property(TARGET me PROPERTY CXX_CPPCHECK ${cppcheck_path} 
    "--enable=all" 
    "--suppress=missingIncludeSystem"
    "${MatchingEngine_SOURCE_DIR}/src"
    )
  else()
    message(STATUS "Won't run cppcheck as it is not found")  
  endif()
endif(ENABLE_STATIC_ANALYSIS)

# Keep header inclusions proper
option(ENABLE_IWYU "Run include-what-you-use: Check for redundant header inclusions" OFF)
message(STATUS "ENABLE_IWYU=${ENABLE_IWYU}")
if(ENABLE_IWYU)
  find_program(iwyu_path NAMES include-what-you-use iwyu REQUIRED)
  if(iwyu_path)
    message(STATUS "Will run include-what-you-use at ${iwyu_path}")
    set_property(TARGET me PROPERTY CXX_INCLUDE_WHAT_YOU_USE ${iwyu_path})
  else()
    message(STATUS "Could not find the program include-what-you-use")
  endif()
endif()