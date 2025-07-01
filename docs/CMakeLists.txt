# Documentation generation with Doxygen - smart auto-setup
include(FetchContent)

# First, try to find Doxygen locally
find_package(Doxygen QUIET)

if(NOT DOXYGEN_FOUND)
    message(STATUS "Doxygen not found locally - attempting auto-setup...")
    
    # Determine platform and download appropriate Doxygen binary
    if(WIN32)
        # Windows - download portable binary (this actually works well)
        message(STATUS "Downloading Doxygen for Windows...")
        FetchContent_Declare(
            doxygen_portable
            URL https://github.com/doxygen/doxygen/releases/download/Release_1_9_8/doxygen-1.9.8.windows.x64.bin.zip
        )
        
        FetchContent_MakeAvailable(doxygen_portable)
        set(DOXYGEN_EXECUTABLE "${doxygen_portable_SOURCE_DIR}/doxygen.exe")
        set(DOXYGEN_FOUND TRUE)
        message(STATUS "Doxygen downloaded successfully for Windows")
        
    else()
        # Linux/macOS - provide helpful installation instructions instead of complex builds
        message(STATUS "Auto-download not available for this platform.")
        message(STATUS "")
        message(STATUS "Please install Doxygen using your system package manager:")
        message(STATUS "")
        if(APPLE)
            message(STATUS "  macOS:   brew install doxygen")
        else()
            message(STATUS "  Ubuntu:  sudo apt-get install doxygen")
            message(STATUS "  Fedora:  sudo dnf install doxygen")
            message(STATUS "  Arch:    sudo pacman -S doxygen")
        endif()
        message(STATUS "")
        message(STATUS "Then reconfigure with: cmake -B build -DBUILD_DOCUMENTATION=ON")
        message(STATUS "")
        
        # Try a simple approach - check if user wants to install automatically
        if(DEFINED ENV{AUTO_INSTALL_DOXYGEN})
            message(STATUS "AUTO_INSTALL_DOXYGEN detected - attempting installation...")
            if(APPLE)
                execute_process(COMMAND brew install doxygen RESULT_VARIABLE brew_result)
                if(brew_result EQUAL 0)
                    find_package(Doxygen QUIET)
                endif()
            elseif(EXISTS "/usr/bin/apt-get")
                message(STATUS "Run: sudo apt-get install doxygen && cmake --build build --target docs")
            endif()
        endif()
        
        return()
    endif()
endif()

# Generate documentation if Doxygen is available
if(DOXYGEN_FOUND)
    message(STATUS "Doxygen found - setting up documentation generation...")
    
    # Set Doxygen configuration variables
    set(DOXYGEN_PROJECT_NAME "LockFreeMemoryPool")
    set(DOXYGEN_PROJECT_BRIEF "High-performance, thread-safe, lock-free memory pool")
    set(DOXYGEN_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
    
    # Doxygen settings for beautiful output
    set(DOXYGEN_EXTRACT_ALL YES)
    set(DOXYGEN_RECURSIVE YES)
    set(DOXYGEN_GENERATE_HTML YES)
    set(DOXYGEN_GENERATE_LATEX NO)
    set(DOXYGEN_USE_MDFILE_AS_MAINPAGE "${CMAKE_SOURCE_DIR}/README.md")
    set(DOXYGEN_EXAMPLE_RECURSIVE YES)
    set(DOXYGEN_SOURCE_BROWSER YES)
    set(DOXYGEN_INLINE_SOURCES YES)
    set(DOXYGEN_GENERATE_TREEVIEW YES)
    set(DOXYGEN_DISABLE_INDEX NO)
    set(DOXYGEN_FULL_SIDEBAR NO)
    set(DOXYGEN_HTML_COLORSTYLE_HUE 220)
    set(DOXYGEN_HTML_COLORSTYLE_SAT 100)
    set(DOXYGEN_HTML_COLORSTYLE_GAMMA 80)
    set(DOXYGEN_QUIET YES)
    
    # Create docs target
    doxygen_add_docs(docs
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/README.md
        ${CMAKE_SOURCE_DIR}/examples
        COMMENT "Generating API documentation with Doxygen"
    )
    
    message(STATUS "Documentation target 'docs' available")
    message(STATUS "Generate docs with: cmake --build build --target docs")
    message(STATUS "Docs will be at: ${DOXYGEN_OUTPUT_DIRECTORY}/html/index.html")
    
else()
    message(STATUS "Doxygen not available - skipping documentation generation")
endif()
