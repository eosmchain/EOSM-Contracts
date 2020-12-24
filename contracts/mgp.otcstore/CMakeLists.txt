add_contract(mgp.otcstore mgp.otcstore
   ${CMAKE_CURRENT_SOURCE_DIR}/src/otcstore.cpp
)

target_include_directories(mgp.otcstore
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include)

set_target_properties(mgp.otcstore
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/ricardian/mgp.otcstore.contracts.md.in ${CMAKE_CURRENT_BINARY_DIR}/ricardian/mgp.otcstore.contracts.md @ONLY )

target_compile_options( mgp.otcstore PUBLIC -R${CMAKE_CURRENT_SOURCE_DIR}/ricardian -R${CMAKE_CURRENT_BINARY_DIR}/ricardian )
