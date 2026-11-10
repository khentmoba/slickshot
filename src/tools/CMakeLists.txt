target_sources(flameshot_lib PRIVATE arrow/arrowtool.h arrow/arrowtool.cpp)
target_sources(flameshot_lib PRIVATE pixelate/pixelatetool.h pixelate/pixelatetool.cpp)
target_sources(flameshot_lib PRIVATE circle/circletool.h circle/circletool.cpp)
target_sources(flameshot_lib PRIVATE circlecount/circlecounttool.h circlecount/circlecounttool.cpp)
target_sources(flameshot_lib PRIVATE copy/copytool.h copy/copytool.cpp)
target_sources(flameshot_lib PRIVATE exit/exittool.h exit/exittool.cpp)
target_sources(flameshot_lib PRIVATE sizeincrease/sizeincreasetool.h sizeincrease/sizeincreasetool.cpp)
target_sources(flameshot_lib PRIVATE sizedecrease/sizedecreasetool.h sizedecrease/sizedecreasetool.cpp)

if (ENABLE_IMGUR)
target_sources(
  flameshot_lib
        PRIVATE imgupload/storages/imgur/imguruploader.h
        imgupload/storages/imgur/imguruploader.cpp
        imgupload/storages/imguploaderbase.h
        imgupload/storages/imguploaderbase.cpp
        imgupload/imguploadertool.h
        imgupload/imguploadertool.cpp
        imgupload/imguploadermanager.h
        imgupload/imguploadermanager.cpp
)
endif()
target_sources(
  flameshot_lib
  PRIVATE launcher/applaunchertool.h
          launcher/applauncherwidget.h
          launcher/launcheritemdelegate.h
          launcher/terminallauncher.h
          launcher/applaunchertool.cpp
          launcher/applauncherwidget.cpp
          launcher/launcheritemdelegate.cpp
          launcher/openwithprogram.cpp
          launcher/terminallauncher.cpp)
target_sources(flameshot_lib PRIVATE line/linetool.h line/linetool.cpp)
target_sources(flameshot_lib PRIVATE marker/markertool.h marker/markertool.cpp)
target_sources(flameshot_lib PRIVATE move/movetool.h move/movetool.cpp)
target_sources(flameshot_lib PRIVATE pencil/penciltool.h pencil/penciltool.cpp)
target_sources(
  flameshot_lib
  PRIVATE pin/pintool.h
          pin/pinwidget.h
          pin/pintool.cpp
          pin/pinwidget.cpp)
target_sources(flameshot_lib PRIVATE rectangle/rectangletool.h rectangle/rectangletool.cpp)
target_sources(flameshot_lib PRIVATE redo/redotool.h redo/redotool.cpp)
target_sources(flameshot_lib PRIVATE save/savetool.h save/savetool.cpp)
target_sources(flameshot_lib PRIVATE accept/accepttool.h accept/accepttool.cpp)
target_sources(flameshot_lib PRIVATE invert/inverttool.h invert/inverttool.cpp)
target_sources(flameshot_lib PRIVATE selection/selectiontool.h selection/selectiontool.cpp)
target_sources(
  flameshot_lib
  PRIVATE text/textconfig.h
          text/texttool.h
          text/textwidget.h
          text/textconfig.cpp
          text/texttool.cpp
          text/textwidget.cpp)
target_sources(flameshot_lib PRIVATE undo/undotool.h undo/undotool.cpp)

target_sources(
  flameshot_lib
  PRIVATE abstractactiontool.cpp
          abstractpathtool.cpp
          abstracttwopointtool.cpp
          capturecontext.cpp
          toolfactory.cpp
          abstractactiontool.h
          abstractpathtool.h
          abstracttwopointtool.h
          capturetool.h
          toolfactory.h)
