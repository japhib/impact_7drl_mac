cmake_minimum_required(VERSION 3.17)
set(CMAKE_CXX_STANDARD 17)
set(TARGET_NAME impact_7drl)

project(${TARGET_NAME})

add_definitions(-DLINUX)

add_subdirectory(libtcod)
include_directories(libtcod/src/libtcod)

add_executable(${TARGET_NAME}
    ai.cpp
    buf.cpp buf.h
    builder.cpp builder.h
    chooser.cpp chooser.h
    command.h
    config.cpp config.h
    dircontrol.cpp dircontrol.h
    display.cpp display.h
    disunion.cpp disunion.h
    dpdf.cpp dpdf.h
    engine.cpp engine.h
    event.h
    firefly.cpp firefly.h
    gamedef.cpp gamedef.h
    gfxengine.cpp gfxengine.h
    glbdef.cpp glbdef.h
    grammar.cpp grammar.h
    indextree.cpp indextree.h
    item.cpp item.h
    level.cpp level.h
    main.cpp
    map.cpp map.h
    mob.cpp mob.h
    msg.cpp msg.h
    mygba.h
    panel.cpp panel.h
    party.cpp party.h
    pq.h
    ptrlist.h
    queue.h
    rand.cpp rand.h
    roomdata.h
    scrpos.cpp scrpos.h
    speed.cpp speed.h
    text.cpp text.h texture.h
    thesaurus.cpp thesaurus.h
    thread_std.cpp thread_std.h
    thread.cpp thread.h
    vec2.h vec3.h
)

target_link_libraries(${TARGET_NAME} libtcod)