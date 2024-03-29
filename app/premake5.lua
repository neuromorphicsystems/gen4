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

project "gen4_recorder"
    location "build"
    kind "ConsoleApp"
    language "C++"
    defines {"SEPIA_COMPILER_WORKING_DIRECTORY='" .. project().location .. "'"}
    files {"gen4_recorder.cpp", "../common/*.hpp"}
    files(qt.moc({
        "chameleon/source/background_cleaner.hpp",
        "chameleon/source/dvs_display.hpp",
        "chameleon/source/count_display.hpp"},
        "build/moc"))
    includedirs(qt.includedirs())
    libdirs(qt.libdirs())
    links(qt.links())
    buildoptions(qt.buildoptions())
    linkoptions(qt.linkoptions())
    filter "system:linux"
        buildoptions {"-std=c++17"}
        linkoptions {"-std=c++17"}
        links {"pthread", "usb-1.0"}
    filter "system:macosx"
        buildoptions {"-std=c++17"}
        linkoptions {"-std=c++17"}
        includedirs {"/usr/local/include", "/opt/homebrew/include"}
        libdirs {"/usr/local/lib", "/opt/homebrew/lib"}
        links {"usb-1.0"}
    filter "system:windows"
        architecture "x64"
        defines {"NOMINMAX"}
        buildoptions {"/std:c++17"}
        linkoptions {"/SUBSYSTEM:WINDOWS"}
        files {"../.clang-format"}
        libdirs {"../common/libusb"}
        links {"libusb-1.0"}

project "lsgen4"
    location "build"
    kind "ConsoleApp"
    language "C++"
    defines {"SEPIA_COMPILER_WORKING_DIRECTORY='" .. project().location .. "'"}
    files {"lsgen4.cpp", "../common/*.hpp"}
    filter "system:linux"
        buildoptions {"-std=c++17"}
        linkoptions {"-std=c++17"}
        links {"pthread", "usb-1.0"}
    filter "system:macosx"
        buildoptions {"-std=c++17"}
        linkoptions {"-std=c++17"}
        includedirs {"/usr/local/include", "/opt/homebrew/include"}
        libdirs {"/usr/local/lib", "/opt/homebrew/lib"}
        links {"usb-1.0"}
    filter "system:windows"
        architecture "x64"
        defines {"NOMINMAX"}
        buildoptions {"/std:c++17"}
        files {"../.clang-format"}
        libdirs {"../common/libusb"}
        links {"libusb-1.0"}
