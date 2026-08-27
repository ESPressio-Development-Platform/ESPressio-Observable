if(COMMAND idf_component_register)
    idf_component_register(INCLUDE_DIRS "src")
elseif(COMMAND register_component)
    set(COMPONENT_SRCDIRS
        "src"
    )
    set(COMPONENT_ADD_INCLUDEDIRS
        "src"
    )
    register_component()
else()
    add_library(ESPressio-Observable INTERFACE)
    target_include_directories(ESPressio-Observable INTERFACE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
    )
    add_library(ESPressio::Observable ALIAS ESPressio-Observable)
endif()
