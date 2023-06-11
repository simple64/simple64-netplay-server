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

type Registration struct {
	RegId  uint32
	Plugin byte
	Raw    byte
}

type GameServer struct {
	Logger        logr.Logger
	Port          int
	TcpListener   *net.TCPListener
	UdpListener   *net.UDPConn
	Password      string
	Running       bool
	MD5           string
	GameName      string
	Players       map[string]Client
	ClientSha     string
	GameData      GameData
	TCPState      map[string]*TCPData
	TCPFiles      map[string][]byte
	TCPSettings   []byte
	HasSettings   bool
	Registrations map[byte]*Registration
}

func (g *GameServer) CreateNetworkServers(basePort int, roomName string, gameName string, logger logr.Logger) int {
	g.Logger = logger.WithValues("game", gameName, "room", roomName)
	port := g.createTCPServer(basePort)
	if port == 0 {
		return port
	}
	return g.createUDPServer()
}
