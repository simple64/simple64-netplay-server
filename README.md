# simple64-netplay-server

## Container

```
podman pull quay.io/simple64/simple64-netplay-server:latest
```

## Running
```
./simple64-netplay-server --name "Server Name"
```

## Playing locally
The server is discoverable on a LAN (similar to how a Chromecast works). When the server is running, clients on the same LAN should find the server automatically.

## Port/firewall requirements
The server will be listening on ports 45000-45010, using TCP and UDP. Firewalls will need to be configured to allow connections on these ports.
