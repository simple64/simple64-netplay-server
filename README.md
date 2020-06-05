# m64p-netplay-server

## Building
```
git clone https://github.com/loganmc10/m64p-netplay-server.git
cd m64p-netplay-server
mkdir build
cd build
qmake ..
make -j4
```

## Running
```
cd m64p-netplay-server/build
./m64p-netplay-server --name "Server Name"
```

## Playing locally
The server is discoverable on a LAN (similar to how a Chromecast works). When the server is running, clients on the same LAN should find the server automatically.
