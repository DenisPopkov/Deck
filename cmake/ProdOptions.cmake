# Production / release configure defaults.
# Usage: cmake -C cmake/ProdOptions.cmake -B build ...
set(CASSETTE_ENABLE_PI_TAPE OFF CACHE BOOL "Private dev-only Pi FTP upload" FORCE)
set(CASSETTE_LEGACY_MACOS OFF CACHE BOOL "Intel macOS 10.14+ (Mojave) build" FORCE)
# Modern macOS (Big Sur 11+). Harmless on non-Apple hosts; overridden by LegacyMacOptions.cmake.
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version" FORCE)
