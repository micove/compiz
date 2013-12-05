add_executable (compiz_autopilot_acceptance_tests
                ${CMAKE_CURRENT_SOURCE_DIR}/compiz_autopilot_acceptance_tests.cpp)

set (COMPIZ_AUTOPILOT_ACCEPTANCE_TEST_LIBRARIES
     ${GTEST_BOTH_LIBRARIES}
     ${CMAKE_THREAD_LIBS_INIT})

target_link_libraries (compiz_autopilot_acceptance_tests
                       ${COMPIZ_AUTOPILOT_ACCEPTANCE_TEST_LIBRARIES})

install (TARGETS compiz_autopilot_acceptance_tests
         RUNTIME DESTINATION ${exec_prefix})
