INCLUDE_DIRECTORIES (  
  ${CMAKE_CURRENT_SOURCE_DIR}/../../include
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/src
    
  ${Boost_INCLUDE_DIRS}
)

LINK_DIRECTORIES (${COMPIZ_LIBRARY_DIRS}) 

SET( 
  SRCS 
  ${CMAKE_CURRENT_SOURCE_DIR}/src/backbuffertracking.cpp
)

ADD_LIBRARY( 
  compiz_composite_backbuffertracking STATIC
  
  ${SRCS}
)

if (COMPIZ_BUILD_TESTING)
ADD_SUBDIRECTORY( ${CMAKE_CURRENT_SOURCE_DIR}/tests )
endif (COMPIZ_BUILD_TESTING)

TARGET_LINK_LIBRARIES(
  compiz_composite_backbuffertracking

  compiz_size
  compiz_core
)
