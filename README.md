- [App](#app)
    - [Install](#install)
        - [Ubuntu](#ubuntu)
        - [macOS](#macos)
        - [Windows](#windows)
    - [Use](#use)
        - [Ubuntu and macOS](#ubuntu-and-macos)
        - [Windows](#windows-1)
- [Recorder 3D and Python](#recorder-3d-and-python)
    - [Dependencies](#dependencies)
        - [Ubuntu](#ubuntu-1)
        - [macOS](#macos-1)
    - [Install](#install-1)
        - [Recorder 3D](#recorder-3d)
        - [Python](#python)

# App

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

Ubuntu 20.04 ships an older version of Qt 5. Before running `make`, replace the following lines 2 to 5 in *app/gen4_recorder.qml.hpp* from:

```qml
import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import QtQuick.Controls 2.15
```

to:

```qml
import QtQuick 2.7
import QtQuick.Layouts 1.1
import QtQuick.Window 2.7
import QtQuick.Controls 2.7
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

Windows users can download a pre-compiled version here: https://drive.google.com/file/d/1-8IyxxZ3wXqpdSKnPt6N7Hs3ZR6OkStc/view?usp=sharing

Follow the instructions below to build the software from source.

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
9. Download https://drive.google.com/file/d/1-8IyxxZ3wXqpdSKnPt6N7Hs3ZR6OkStc/view?usp=sharing and unzip it (the directory contains the required shared libraries, copied from C:\Qt\5.15.3)
10. Copy gen4/app/build/bin/release/gen4_recorder.exe to gen4-windows

## Use

Edit configuration.json to change the default biases, recordings directory, and buffer overflow behhaviour (set "drop_threshold" to 0 disable paacket drop).

### Ubuntu and macOS

```sh
cd gen4
./gen4 # or ./gen4 -c configuration.json to use another configuration file
```

### Windows

Double-click on gen4-windows/gen4_recorder.exe.

# Recorder 3D and Python

Unlike the app, which supports two Gen 4 versions (Denebola dev board and EVK4), recorder 3D and the Python extension only support the EVK4.

## Dependencies

### Ubuntu

```sh
sudo apt install -y build-essential git libusb-1.0-0-dev python3-pip
git clone https://github.com/neuromorphicsystems/gen4.git
```

Create _/etc/udev/rules.d/65-event-based-cameras.rules_ (as super-user) with the following content:

```txt
SUBSYSTEM=="usb", ATTR{idVendor}=="152a",ATTR{idProduct}=="84[0-1]?", MODE="0666"
SUBSYSTEM=="usb", ATTR{idVendor}=="04b4",ATTR{idProduct}=="00f[4-5]", MODE="0666"
```

### macOS

```sh
brew install libusb
git clone https://github.com/neuromorphicsystems/gen4.git
```

## Install

### Recorder 3D

```sh
cd recorder_3d
python3 -m pip install -e .
```

### Python

```sh
cd python
python3 -m pip install -e .
```
