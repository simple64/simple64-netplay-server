package gameserver

import (
	"net"

	"github.com/go-logr/logr"
	"golang.org/x/net/websocket"
)

type Client struct {
	IP     string
	Socket *websocket.Conn
	Number int
}

type GameServer struct {
	Logger      logr.Logger
	Port        int
	TcpListener *net.TCPListener
	UdpListener *net.UDPConn
	Password    string
	Running     bool
	MD5         string
	GameName    string
	Players     map[string]Client
	ClientSha   string
	GameData    GameData
	TCPData     TCPData
}

func (g *GameServer) CreateNetworkServers(basePort int, logger logr.Logger) int {
	g.Logger = logger
	port := g.createTCPServer(basePort)
	if port == 0 {
		return port
	}
	return g.createUDPServer()
}
