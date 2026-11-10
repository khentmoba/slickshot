find_package(Qt${QT_VERSION_MAJOR} CONFIG QUIET COMPONENTS Test)

if(Qt${QT_VERSION_MAJOR}Test_FOUND)
  set(CMAKE_AUTOMOC ON)
  set(CMAKE_AUTORCC ON)

  add_executable(tst_slickshot
    tst_slickshot.cpp
    $<TARGET_OBJECTS:flameshot_lib>
    ${CMAKE_SOURCE_DIR}/data/graphics.qrc
  )

  target_link_libraries(tst_slickshot
    PRIVATE
    flameshot_lib
    Qt${QT_VERSION_MAJOR}::Test
  )

  if(DISABLE_UPDATE_CHECKER)
    target_sources(tst_slickshot PRIVATE
      ${CMAKE_SOURCE_DIR}/src/utils/appupdate.cpp
    )
  endif()

  add_test(NAME tst_slickshot COMMAND tst_slickshot)
  set_tests_properties(tst_slickshot PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
  )
  if(WIN32)
    set_property(TEST tst_slickshot APPEND PROPERTY
      ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>"
    )
    set_property(TEST tst_slickshot APPEND PROPERTY
      ENVIRONMENT_MODIFICATION "QT_PLUGIN_PATH=path_list_append:$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>/../plugins"
    )
  endif()
endif()
