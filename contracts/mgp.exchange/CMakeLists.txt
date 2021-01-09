add_contract(mgp.exchange mgp.exchange ${CMAKE_CURRENT_SOURCE_DIR}/src/dex.cpp)

target_include_directories(mgp.exchange
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include)

set_target_properties(mgp.exchange
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/ricardian/dex.contracts.md.in ${CMAKE_CURRENT_BINARY_DIR}/ricardian/dex.contracts.md @ONLY )

target_compile_options( mgp.exchange PUBLIC -R${CMAKE_CURRENT_SOURCE_DIR}/ricardian -R${CMAKE_CURRENT_BINARY_DIR}/ricardian )