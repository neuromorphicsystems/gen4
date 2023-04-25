# gen4

## Install

### Ubuntu

```
cd gen4
sudo apt install curl
curl -L https://github.com/premake/premake-core/releases/download/v5.0.0-beta2/premake-5.0.0-beta2-linux.tar.gz | tar xz
sudo apt install -y build-essential git libusb-1.0-0-dev
sudo apt install -y qtbase5-dev qtdeclarative5-dev qml-module-qtquick-controls qml-module-qtquick-controls2 qml-module-qttest
```

Create _/etc/udev/rules.d/65-event-based-cameras.rules_ (as super-user) with the following content:

```txt
SUBSYSTEM=="usb", ATTR{idVendor}=="152a",ATTR{idProduct}=="84[0-1]?", MODE="0666"
SUBSYSTEM=="usb", ATTR{idVendor}=="04b4",ATTR{idProduct}=="00f[4-5]", MODE="0666"
```

```
cd app
../premake5 gmake
cd build
make
```

### macOS

```
brew install libusb qt@5
```

### Windows

```powershell
Set-ExecutionPolicy Bypass -Scope Process -Force; [System.Net.ServicePointManager]::SecurityProtocol = [System.Net.ServicePointManager]::SecurityProtocol -bor 3072; iex ((New-Object System.Net.WebClient).DownloadString('https://community.chocolatey.org/install.ps1'))
```

```powershell
choco install -y visualstudio2019buildtools visualstudio2019-workload-vctools qt5-default uniextract
```
