# gen4

## Install

### Ubuntu

```
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

## Use

### Ubuntu

```
cd gen4
./gen4
```
