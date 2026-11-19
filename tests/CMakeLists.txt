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

  # AppUpdate helpers are exercised directly by tst_slickshot. When the update
  # checker is compiled out they are not part of flameshot_lib, so compile the
  # source into the test target in that configuration.
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
    # Qt is linked dynamically. CTest puts Qt's bin directory on PATH, but a
    # direct launch of tst_slickshot.exe (from an IDE or Explorer) otherwise
    # fails with "Qt6Test.dll was not found" before any test runs. Deploy the
    # runtime libraries and plugins next to the executable so it is self
    # contained. Deploying plugins matters too: copying only the DLLs makes Qt
    # look for its plugins beside the executable and no platform plugin loads.
    foreach(prefix ${CMAKE_PREFIX_PATH})
      list(APPEND TEST_WINDEPLOYQT_SEARCH_PATHS "${prefix}/bin")
    endforeach()
    list(APPEND TEST_WINDEPLOYQT_SEARCH_PATHS "$ENV{QTDIR}/bin")
    find_program(TEST_WINDEPLOYQT_EXE windeployqt.exe
      PATHS ${TEST_WINDEPLOYQT_SEARCH_PATHS})
    if(TEST_WINDEPLOYQT_EXE)
      add_custom_command(TARGET tst_slickshot POST_BUILD
        COMMAND ${TEST_WINDEPLOYQT_EXE}
          --no-translations --no-system-d3d-compiler --no-quick-import
          --compiler-runtime
          --dir $<TARGET_FILE_DIR:tst_slickshot> $<TARGET_FILE:tst_slickshot>
        COMMENT "Deploying Qt runtime for tst_slickshot"
        VERBATIM
      )
    else()
      message(WARNING "windeployqt not found; tst_slickshot may fail outside CTest")
    endif()

    set_property(TEST tst_slickshot APPEND PROPERTY
      ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>"
    )
    set_property(TEST tst_slickshot APPEND PROPERTY
      ENVIRONMENT_MODIFICATION "QT_PLUGIN_PATH=path_list_append:$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>/../plugins"
    )
  endif()
endif()
