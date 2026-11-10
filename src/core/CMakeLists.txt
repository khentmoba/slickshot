target_sources(flameshot_lib PRIVATE
    flameshot.h
    flameshotdaemon.h
    qguiappcurrentscreen.h
)

target_sources(flameshot_lib PRIVATE
    capturerequest.cpp
    flameshot.cpp
    flameshotdaemon.cpp
    qguiappcurrentscreen.cpp
)

if (UNIX AND NOT APPLE)
target_sources(flameshot_lib PRIVATE
    flameshotdbusadapter.h
    flameshotdbusadapter.cpp
)
endif ()

if (USE_KDSINGLEAPPLICATION)
if (NOT WIN32)
target_sources(flameshot_lib PRIVATE
    signaldaemon.h
    signaldaemon.cpp
)
endif ()
endif ()

if (WIN32)
    target_sources(flameshot_lib PRIVATE globalshortcutfilter.h globalshortcutfilter.cpp)
endif ()
