package gameserver

import (
	"net"
	"strings"
	"time"

	"github.com/go-logr/logr"
	"golang.org/x/net/websocket"
)

type Client struct {
	Socket *websocket.Conn
	IP     string
	Number int
}

type Registration struct {
	RegID  uint32
	Plugin byte
	Raw    byte
}

type GameServer struct {
	StartTime     time.Time
	Players       map[string]Client
	TCPListener   *net.TCPListener
	UDPListener   *net.UDPConn
	Registrations map[byte]*Registration
	TCPFiles      map[string][]byte
	Logger        logr.Logger
	Password      string
	GameName      string
	ClientSha     string
	MD5           string
	GameData      GameData
	TCPSettings   []byte
	Port          int
	HasSettings   bool
	Running       bool
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
	if err := g.UDPListener.Close(); err != nil && !g.isConnClosed(err) {
		g.Logger.Error(err, "error closing UdpListener")
	} else if err == nil {
		g.Logger.Info("TCP server closed")
	}
	if err := g.TCPListener.Close(); err != nil && !g.isConnClosed(err) {
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
