# Intel macOS 10.14 Mojave release defaults.
# Usage: cmake -C cmake/LegacyMacOptions.cmake -B build-legacy ...
set(CASSETTE_ENABLE_PI_TAPE OFF CACHE BOOL "Private dev-only Pi FTP upload" FORCE)
set(CASSETTE_LEGACY_MACOS ON CACHE BOOL "Intel macOS 10.14+ (Mojave) build" FORCE)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.14" CACHE STRING "Minimum macOS version" FORCE)
set(CMAKE_OSX_ARCHITECTURES "x86_64" CACHE STRING "Target CPU architectures" FORCE)
set(CASSETTE_BUILD_GSTPEAQ OFF CACHE BOOL "GstPEAQ needs gstreamer at runtime" FORCE)
