
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
convertImage(assets/pilogo.png 4 piLogo.dat piPalette.dat)
addBinaryFile(picostation-loader fontTexture "${PROJECT_BINARY_DIR}/fontTexture.dat")
addBinaryFile(picostation-loader fontPalette "${PROJECT_BINARY_DIR}/fontPalette.dat")
addBinaryFile(picostation-loader piTexture "${PROJECT_BINARY_DIR}/piLogo.dat")
addBinaryFile(picostation-loader piPalette "${PROJECT_BINARY_DIR}/piPalette.dat")




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

# ==============================================================================
# Image Generation Post-Build Step
# Generates .psexe image from compiled binary using platform-specific scripts
# ==============================================================================

# Define script based on platform - fail fast with clear error message
if(WIN32)
    set(IMAGE_SCRIPT "${PROJECT_SOURCE_DIR}/makeimage.bat")
    set(PLATFORM_NAME "Windows")
else()
    set(IMAGE_SCRIPT "${PROJECT_SOURCE_DIR}/makeimage.sh")
    set(PLATFORM_NAME "Unix/Linux")
endif()

# Validate script exists before proceeding
if(NOT EXISTS "${IMAGE_SCRIPT}")
    message(FATAL_ERROR 
        "Image generation script not found for ${PLATFORM_NAME}: ${IMAGE_SCRIPT}\n"
        "Please ensure the appropriate script exists in the project root."
    )
endif()

# Generate image post-build
add_custom_command(
    TARGET picostation-loader 
    POST_BUILD
    COMMAND 
        "${IMAGE_SCRIPT}"
        "$<TARGET_FILE:picostation-loader>"
        "picostation-loader.psexe"
    COMMENT "Generating picostation-loader.psexe image for ${PLATFORM_NAME}"
    VERBATIM
    USES_TERMINAL
)

# Optional: Add dependency tracking for the script itself
# This ensures rebuild if the script changes
set_property(
    TARGET picostation-loader 
    APPEND PROPERTY ADDITIONAL_CLEAN_FILES 
    "${CMAKE_CURRENT_BINARY_DIR}/picostation-loader.psexe"
)