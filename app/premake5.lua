local qt = require "chameleon/qt"

workspace "recorder"
    location "build"
    configurations {"release", "debug"}
    filter "configurations:release"
        defines {"NDEBUG"}
        optimize "On"
    filter "configurations:debug"
        defines {"DEBUG"}
        symbols "On"

target_os = "macosx_m1"

project "evk4_recorder"
    location "build"
    kind "ConsoleApp"
    language "C++"
    defines {"SEPIA_COMPILER_WORKING_DIRECTORY='" .. project().location .. "'"}
    files {"evk4_recorder.cpp", "../common/*.hpp"}
    files(qt.moc({
        "chameleon/source/background_cleaner.hpp",
        "chameleon/source/dvs_display.hpp"},
        "build/moc",
        target_os))
    includedirs(qt.includedirs(target_os))
    libdirs(qt.libdirs(target_os))
    links(qt.links(target_os))
    buildoptions(qt.buildoptions(target_os))
    linkoptions(qt.linkoptions(target_os))
    filter "system:linux"
        buildoptions {"-std=c++17"}
        linkoptions {"-std=c++17"}
        links {"pthread", "usb-1.0"}
    filter "system:macosx"
        buildoptions {"-std=c++17"}
        linkoptions {"-std=c++17"}
        libdirs {"/usr/local/lib", "/opt/homebrew/lib"}
        links {"usb-1.0"}
    filter "system:windows"
        architecture "x64"
        defines {"NOMINMAX"}
        buildoptions {"/std:c++17"}
        files {"../.clang-format"}
        links {"libusb-1.0"}

project "lsevk4"
    location "build"
    kind "ConsoleApp"
    language "C++"
    defines {"SEPIA_COMPILER_WORKING_DIRECTORY='" .. project().location .. "'"}
    files {"lsevk4.cpp", "../common/*.hpp"}
    filter "system:linux"
        buildoptions {"-std=c++17"}
        linkoptions {"-std=c++17"}
        links {"pthread", "usb-1.0"}
    filter "system:macosx"
        buildoptions {"-std=c++17"}
        linkoptions {"-std=c++17"}
        libdirs {"/usr/local/lib", "/opt/homebrew/lib"}
        links {"usb-1.0"}
    filter "system:windows"
        architecture "x64"
        defines {"NOMINMAX"}
        buildoptions {"/std:c++17"}
        files {"../.clang-format"}
        links {"libusb-1.0"}
