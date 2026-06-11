# Optional third-party libraries for CASSETTE_BUILD_ONNX / CASSETTE_BUILD_RUBBERBAND

function(cassette_find_onnxruntime out_found)
    set(_found FALSE)
    set(_include "")
    set(_lib "")

    if(DEFINED ENV{ONNXRUNTIME_ROOT})
        set(_root "$ENV{ONNXRUNTIME_ROOT}")
        find_path(_include onnxruntime_cxx_api.h
            HINTS "${_root}/include" "${_root}/include/onnxruntime/core/session" "${_root}/include/onnxruntime")
        find_library(_lib NAMES onnxruntime libonnxruntime HINTS "${_root}/lib" "${_root}/lib64")
    endif()

    if(NOT _include)
        set(_brewOnnx "/opt/homebrew/opt/onnxruntime")
        if(EXISTS "${_brewOnnx}/include/onnxruntime/onnxruntime_cxx_api.h")
            set(_include "${_brewOnnx}/include/onnxruntime")
            set(_lib "${_brewOnnx}/lib/libonnxruntime.dylib")
            if(EXISTS "${_lib}")
                set(_found TRUE)
            endif()
        endif()
    endif()

    if(NOT _found)
        find_path(_include onnxruntime_cxx_api.h
            HINTS
                /opt/homebrew/opt/onnxruntime/include
                /usr/local/opt/onnxruntime/include
                /opt/homebrew/include
                /usr/local/include
            PATH_SUFFIXES onnxruntime onnxruntime/core/session)
    endif()

    if(NOT _lib)
        find_library(_lib NAMES onnxruntime libonnxruntime
            HINTS
                /opt/homebrew/opt/onnxruntime/lib
                /usr/local/opt/onnxruntime/lib
                /opt/homebrew/lib
                /usr/local/lib)
    endif()

    if(_include AND _lib)
        set(_found TRUE)
    endif()

    set(CASSETTE_ONNX_INCLUDE "${_include}" PARENT_SCOPE)
    set(CASSETTE_ONNX_LIBRARY "${_lib}" PARENT_SCOPE)
    set(${out_found} "${_found}" PARENT_SCOPE)
endfunction()

function(cassette_find_rubberband out_found)
    set(_found FALSE)
    set(_include "")
    set(_lib "")

    find_path(_include rubberband/RubberBandStretcher.h
        HINTS
            /opt/homebrew/opt/rubberband/include
            /usr/local/opt/rubberband/include
            /opt/homebrew/include
            /usr/local/include)

    find_library(_lib NAMES rubberband librubberband
        HINTS
            /opt/homebrew/opt/rubberband/lib
            /usr/local/opt/rubberband/lib
            /opt/homebrew/lib
            /usr/local/lib)

    if(NOT _found AND EXISTS "/opt/homebrew/opt/rubberband/include/rubberband/RubberBandStretcher.h")
        set(_include "/opt/homebrew/opt/rubberband/include")
        set(_lib "/opt/homebrew/opt/rubberband/lib/librubberband.dylib")
        if(EXISTS "${_lib}")
            set(_found TRUE)
        endif()
    endif()

    if(_include AND _lib AND NOT _found)
        set(_found TRUE)
    endif()

    set(CASSETTE_RUBBERBAND_INCLUDE "${_include}" PARENT_SCOPE)
    set(CASSETTE_RUBBERBAND_LIBRARY "${_lib}" PARENT_SCOPE)
    set(${out_found} "${_found}" PARENT_SCOPE)
endfunction()

function(cassette_find_essentia out_found)
    set(_found FALSE)
    set(_include "")
    set(_lib "")

    if(DEFINED ENV{ESSENTIA_ROOT})
        set(_root "$ENV{ESSENTIA_ROOT}")
        find_path(_include essentia/essentia.h HINTS "${_root}/include")
        find_library(_lib NAMES essentia HINTS "${_root}/lib" "${_root}/lib64")
    endif()

    if(NOT _include)
        find_path(_include essentia/essentia.h
            HINTS
                /opt/homebrew/include
                /usr/local/include
                /opt/homebrew/opt/essentia/include
                /usr/local/opt/essentia/include)
    endif()

    if(NOT _lib)
        find_library(_lib NAMES essentia
            HINTS
                /opt/homebrew/lib
                /usr/local/lib
                /opt/homebrew/opt/essentia/lib
                /usr/local/opt/essentia/lib)
    endif()

    if(_include AND _lib)
        set(_found TRUE)
    endif()

    set(CASSETTE_ESSENTIA_INCLUDE "${_include}" PARENT_SCOPE)
    set(CASSETTE_ESSENTIA_LIBRARY "${_lib}" PARENT_SCOPE)
    set(${out_found} "${_found}" PARENT_SCOPE)
endfunction()
