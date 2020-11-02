add_contract(addressbookt addressbookt
   ${CMAKE_CURRENT_SOURCE_DIR}/src/addressbookt.cpp
)

target_include_directories(addressbookt
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include)

set_target_properties(addressbookt
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/ricardian/addressbookt.contracts.md.in ${CMAKE_CURRENT_BINARY_DIR}/ricardian/addressbookt.contracts.md @ONLY )

target_compile_options( addressbookt PUBLIC -R${CMAKE_CURRENT_SOURCE_DIR}/ricardian -R${CMAKE_CURRENT_BINARY_DIR}/ricardian )
