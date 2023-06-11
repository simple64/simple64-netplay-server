package gameserver

import (
	"net"
	"strings"
	"time"

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
	TCPFiles      map[string][]byte
	TCPSettings   []byte
	HasSettings   bool
	Registrations map[byte]*Registration
	StartTime     time.Time
}

func (g *GameServer) CreateNetworkServers(basePort int, roomName string, gameName string, logger logr.Logger) int {
	g.Logger = logger.WithValues("game", gameName, "room", roomName)
	port := g.createTCPServer(basePort)
	if port == 0 {
		return port
	}
	return g.createUDPServer()
}

func (g *GameServer) closeServers() {
	if err := g.UdpListener.Close(); err != nil && !g.isConnClosed(err) {
		g.Logger.Error(err, "error closing UdpListener")
	} else if err == nil {
		g.Logger.Info("TCP server closed")
	}
	if err := g.TcpListener.Close(); err != nil && !g.isConnClosed(err) {
		g.Logger.Error(err, "error closing TcpListener")
	} else if err == nil {
		g.Logger.Info("UDP server closed")
	}
}

func (g *GameServer) isConnClosed(err error) bool {
	if err == nil {
		return false
	}
	return strings.Contains(err.Error(), "use of closed network connection")
}
