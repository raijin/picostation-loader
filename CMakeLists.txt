
cmake_minimum_required(VERSION 3.25)

# Set the path to the toolchain file, which will configure CMake to use the MIPS
# toolchain rather than its default compiler and proceed in turn to execute
# setup.cmake.
set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_LIST_DIR}/ps1-bare-metal/cmake/toolchain.cmake")

# Tell CMake about the project. The VERSION, DESCRIPTION and HOMEPAGE_URL fields
# are optional, but the project name and LANGUAGES field should be present.
project(
    picostation-loader
    LANGUAGES    C CXX ASM
    VERSION      1.0.0
    DESCRIPTION  "PSX.Dev project template"
    HOMEPAGE_URL "https://github.com/grumpycoders/pcsx-redux"
)

# Set up compiler flags and initialize the Python environment used to run the
# scripts in the tools directory.
include(ps1-bare-metal/cmake/setup.cmake)
include(ps1-bare-metal/cmake/virtualenv.cmake)

# Build a "common" library containing basic support code. We are going to link
# this library into our executable.
add_library(
    common OBJECT
    ps1-bare-metal/libc/clz.s
    ps1-bare-metal/libc/crt0.c
    ps1-bare-metal/libc/cxxsupport.cpp
    ps1-bare-metal/libc/malloc.c
    ps1-bare-metal/libc/misc.c
    ps1-bare-metal/libc/setjmp.s
    ps1-bare-metal/libc/string.c
    ps1-bare-metal/libc/string.s
    ps1-bare-metal/ps1/cache.s
    ps1-bare-metal/vendor/printf.c
    src/includes/system.s
    src/includes/unhandledexc.c
)
target_include_directories(
    common PUBLIC
    ps1-bare-metal/
    ps1-bare-metal/libc
)

# Compile the main executable. You may add more source files by listing them
# here.
add_executable(
    picostation-loader
    src/gpu.c
    src/main.c
    src/controller.c
    src/includes/cdrom.c
    src/includes/system.c
    src/includes/filesystem.c
    src/includes/irq.c
    src/includes/stream.c
)
target_link_libraries(picostation-loader PRIVATE common)

# Define a CMake macro that invokes convertImage.py in order to generate VRAM
# texture data from an image file.
function(convertImage input bpp)
    add_custom_command(
        OUTPUT  ${ARGN}
        DEPENDS "${PROJECT_SOURCE_DIR}/${input}"
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/ps1-bare-metal/tools/convertImage.py"
            -b ${bpp}
            "${PROJECT_SOURCE_DIR}/${input}"
            ${ARGN}
        VERBATIM
    )
endfunction()


# Convert the font spritesheet to a 4bpp texture and palette, then embed them
# into the executable. The addBinaryFile() macro is defined in setup.cmake; you
# may call it multiple times to embed other data into the binary.
convertImage(assets/font.png 4 fontTexture.dat fontPalette.dat)
convertImage(assets/picostationlogo.png 4 logoTexture.dat logoPalette.dat)
addBinaryFile(picostation-loader fontTexture "${PROJECT_BINARY_DIR}/fontTexture.dat")
addBinaryFile(picostation-loader fontPalette "${PROJECT_BINARY_DIR}/fontPalette.dat")
addBinaryFile(picostation-loader logoTexture "${PROJECT_BINARY_DIR}/logoTexture.dat")
addBinaryFile(picostation-loader logoPalette "${PROJECT_BINARY_DIR}/logoPalette.dat")




# Add a step to run convertExecutable.py after the executable is compiled in
# order to convert it into a PS1 executable. By default all custom commands run
# from the build directory, so paths to files in the source directory must be
# prefixed with ${PROJECT_SOURCE_DIR}.
add_custom_command(
    TARGET     picostation-loader POST_BUILD
    BYPRODUCTS picostation-loader.psexe
    COMMAND
        "${Python3_EXECUTABLE}"
        "${PROJECT_SOURCE_DIR}/ps1-bare-metal/tools/convertExecutable.py"
        "$<TARGET_FILE:picostation-loader>"
        picostation-loader.psexe
    VERBATIM
)

add_custom_command(
    TARGET     picostation-loader POST_BUILD
    COMMAND
        "${PROJECT_SOURCE_DIR}/makeimage.bat"
        "$<TARGET_FILE:picostation-loader>"
        picostation-loader.psexe
    VERBATIM
)
