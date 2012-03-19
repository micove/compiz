include_directories (${CMAKE_CURRENT_SOURCE_DIR}
		     ${compiz_SOURCE_DIR}/gtk/window-decorator/local-menus/tests
		     ${compiz_SOURCE_DIR}/gtk/window-decorator
		     ${compiz_SOURCE_DIR}/gtk/window-decorator/local-menus/src)

add_executable( 
  gtk_window_decorator_check_local_menu_on_off_test
  
  ${CMAKE_CURRENT_SOURCE_DIR}/test-local-menu-on-off.cpp
)

target_link_libraries( 
  gtk_window_decorator_check_local_menu_on_off_test
  
  gtk_window_decorator_local_menus
  
  ${GTEST_BOTH_LIBRARIES}
  ${CMAKE_THREAD_LIBS_INIT} # Link in pthread.
)

add_test (gtk_window_decorator_local_menus_on_off gtk_window_decorator_check_local_menu_on_off_test)
