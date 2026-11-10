# Required to generate MOC
target_sources(
  flameshot_lib
  PRIVATE abstractlogger.h
          filenamehandler.h
          screengrabber.h
          systemnotification.h
          valuehandler.h
          themeutils.h
          snippingtoolmanager.h
          strfparse.h
)

target_sources(
  flameshot_lib
  PRIVATE abstractlogger.cpp
          filenamehandler.cpp
          screengrabber.cpp
          monitorpreview.cpp
          confighandler.cpp
          systemnotification.cpp
          valuehandler.cpp
          screenshotsaver.cpp
          globalvalues.cpp
          desktopfileparse.cpp
          desktopinfo.cpp
          pathinfo.cpp
          colorutils.cpp
          themeutils.cpp
          snippingtoolmanager.cpp
          history.cpp
          strfparse.cpp
)

IF (UNIX AND NOT APPLE)
target_sources(
  flameshot_lib
  PRIVATE request.h
          request.cpp
)
ENDIF()

IF (WIN32)
  target_sources(
    flameshot_lib
    PRIVATE winlnkfileparse.cpp
  )
ENDIF()

if (NOT DISABLE_UPDATE_CHECKER)
  target_sources(
    flameshot_lib
    PRIVATE appupdate.h
            appupdate.cpp
  )
endif()
