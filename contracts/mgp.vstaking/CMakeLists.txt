add_contract(mgp.vstaking mgp.vstaking
   ${CMAKE_CURRENT_SOURCE_DIR}/src/staking.cpp
)

target_include_directories(mgp.vstaking
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include)

set_target_properties(mgp.vstaking
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/ricardian/mgp.vstaking.contracts.md.in ${CMAKE_CURRENT_BINARY_DIR}/ricardian/mgp.vstaking.contracts.md @ONLY )

target_compile_options( mgp.vstaking PUBLIC -R${CMAKE_CURRENT_SOURCE_DIR}/ricardian -R${CMAKE_CURRENT_BINARY_DIR}/ricardian )
