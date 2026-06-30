# Apple platform flags shared by Deck targets (included from CMakeLists.txt).

if(NOT APPLE)
    return()
endif()

if(CASSETTE_LEGACY_MACOS)
    set(_deck_macos_min "10.14")
    message(STATUS "macOS legacy build: x86_64, minimum macOS ${_deck_macos_min}")
else()
    if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
        set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version" FORCE)
    endif()
    set(_deck_macos_min "${CMAKE_OSX_DEPLOYMENT_TARGET}")
    message(STATUS "macOS modern build: minimum macOS ${_deck_macos_min}")
endif()

function(deck_apply_macos_target_properties target)
    if(NOT APPLE)
        return()
    endif()
    target_compile_options(${target} PRIVATE "-mmacosx-version-min=${_deck_macos_min}")
    target_link_options(${target} PRIVATE
        "-mmacosx-version-min=${_deck_macos_min}"
    )
    if(CASSETTE_LEGACY_MACOS)
        # New Xcode linkers may emit libc++ symbols unavailable on Mojave.
        target_link_options(${target} PRIVATE "-Wl,-ld_classic")
    endif()
endfunction()
