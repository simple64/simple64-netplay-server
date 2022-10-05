# simple64-netplay-server

## Container

```
podman pull quay.io/simple64/simple64-netplay-server:latest
```

## Building

### Ubuntu

Qt 5.10 is most easily available on Ubuntu 20+. On older versions of Ubuntu, the `qt5-default` package will install an older version.

```sh
sudo apt install build-essential qt5-default libqt5websockets5-dev
```

### Fedora

For Fedora, replace the `qmake` command with `qmake-qt5`.

```sh
sudo dnf install git-all qt5-qtbase qt5-qtbase-devel qt5-qtwebsockets
```

### Compiling
```
git clone https://github.com/simple64/simple64-netplay-server.git
cd simple64-netplay-server
mkdir build
cd build
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release ..
VERBOSE=1 cmake --build .
```

## Running
```
cd simple64-netplay-server/build
./simple64-netplay-server --name "Server Name"
```

## Playing locally
The server is discoverable on a LAN (similar to how a Chromecast works). When the server is running, clients on the same LAN should find the server automatically.

## Port/firewall requirements
The server will be listening on ports 45000-45010, using TCP and UDP. Firewalls will need to be configured to allow connections on these ports.
