add_contract(mgp.devshare mgp.devshare
   ${CMAKE_CURRENT_SOURCE_DIR}/src/devshare.cpp
)

target_include_directories(mgp.devshare
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include)

set_target_properties(mgp.devshare
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/ricardian/mgp.devshare.contracts.md.in ${CMAKE_CURRENT_BINARY_DIR}/ricardian/mgp.devshare.contracts.md @ONLY )

target_compile_options( mgp.devshare PUBLIC -R${CMAKE_CURRENT_SOURCE_DIR}/ricardian -R${CMAKE_CURRENT_BINARY_DIR}/ricardian )
