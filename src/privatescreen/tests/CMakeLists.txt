include_directories( 
  ${CMAKE_CURRENT_SOURCE_DIR} 

  ${compiz_SOURCE_DIR}/include
  ${compiz_SOURCE_DIR}/src
)

add_definitions (
    -DHAVE_CONFIG_H
    -DPLUGINDIR=\\\"${compiz_plugindir}\\\"
    -DSHAREDIR=\\\"${compiz_sharedir}\\\"
    -DMETADATADIR=\\\"${compiz_metadatadir}\\\"
)


add_executable( 
  compiz_privatescreen_test
  
  ${CMAKE_CURRENT_SOURCE_DIR}/test-privatescreen.cpp
)

target_link_libraries( 
  compiz_privatescreen_test
  
  compiz_core
  compiz_window_constrainment
  compiz_privatescreen
  compiz_logmessage
  
  ${GTEST_BOTH_LIBRARIES}
  ${GMOCK_LIBRARY}
  ${CMAKE_THREAD_LIBS_INIT} # Link in pthread.
)

gtest_add_tests( compiz_privatescreen_test "" ${CMAKE_CURRENT_SOURCE_DIR}/test-privatescreen.cpp )
