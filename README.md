# gen4

## Install

### Ubuntu

```sh
sudo apt install -y curl build-essential git libusb-1.0-0-dev qtbase5-dev qtdeclarative5-dev qml-module-qtquick-controls qml-module-qtquick-controls2 qml-module-qttest
git clone https://github.com/neuromorphicsystems/gen4.git
cd gen4/app
curl -L https://github.com/premake/premake-core/releases/download/v5.0.0-beta2/premake-5.0.0-beta2-linux.tar.gz | tar xz
./premake5 gmake
cd build
make
```

Create _/etc/udev/rules.d/65-event-based-cameras.rules_ (as super-user) with the following content:

```txt
SUBSYSTEM=="usb", ATTR{idVendor}=="152a",ATTR{idProduct}=="84[0-1]?", MODE="0666"
SUBSYSTEM=="usb", ATTR{idVendor}=="04b4",ATTR{idProduct}=="00f[4-5]", MODE="0666"
```

### macOS

```sh
brew install libusb qt@5 premake
git clone https://github.com/neuromorphicsystems/gen4.git
cd gen4/app
premake5 gmake
cd build
make
```

### Windows

1. Install Visual Studio Community (https://visualstudio.microsoft.com/)
2. Download https://github.com/martinrotter/qt-minimalistic-builds/releases/download/5.15.3/qt-5.15.3-dynamic-msvc2019-x86_64.7z, decompress it, and copy the directory to C:\Qt\5.15.3 (so that moc.exe is at C:\Qt\5.15.3\bin\moc.exe)
3. Download and unzip this repository
4. Download and unzip Premake5 (https://premake.github.io/), and copy premake5.exe to gen4/app
5. Generate a Visual Studio project
    ```powershell
    cd gen4/app
    premake5 vs2019
    ```
6. Open gen4/app/build/gen4_recorder.vcxproj
7. Retarget the solution and set the mode to release
8. Build the solution
9. Download https://drive.google.com/file/d/1PKU1-IiZGfqYMU2Y6LlJevxUKrbrveJL/view?usp=sharing and unzip it
10. Copy gen4/app/build/bin/release/gen4_recorder.exe to gen4-windows

## Use

### Ubuntu and macOS

```sh
cd gen4
./gen4
```

### Windows

Double-click on gen4-windows/gen4_recorder.exe.
