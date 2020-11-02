add_contract(mgp.bpvoting mgp.bpvoting
   ${CMAKE_CURRENT_SOURCE_DIR}/src/bpvoting.cpp
)

target_include_directories(mgp.bpvoting
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include)

set_target_properties(mgp.bpvoting
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/ricardian/mgp.bpvoting.contracts.md.in ${CMAKE_CURRENT_BINARY_DIR}/ricardian/mgp.bpvoting.contracts.md @ONLY )

target_compile_options( mgp.bpvoting PUBLIC -R${CMAKE_CURRENT_SOURCE_DIR}/ricardian -R${CMAKE_CURRENT_BINARY_DIR}/ricardian )
