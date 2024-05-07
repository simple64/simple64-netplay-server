package gameserver

import (
	"net"
	"strings"
	"sync"
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
	StartTime          time.Time
	Players            map[string]Client
	PlayersMutex       sync.Mutex
	TCPListener        *net.TCPListener
	UDPListener        *net.UDPConn
	Registrations      map[byte]*Registration
	RegistrationsMutex sync.Mutex
	TCPFiles           map[string][]byte
	CustomData         map[byte][]byte
	Logger             logr.Logger
	GameName           string
	Password           string
	ClientSha          string
	MD5                string
	Emulator           string
	TCPSettings        []byte
	GameData           GameData
	GameDataMutex      sync.Mutex
	Port               int
	HasSettings        bool
	Running            bool
	Features           map[string]string
}

func (g *GameServer) CreateNetworkServers(basePort int, maxGames int, roomName string, gameName string, logger logr.Logger) int {
	g.Logger = logger.WithValues("game", gameName, "room", roomName)
	port := g.createTCPServer(basePort, maxGames)
	if port == 0 {
		return port
	}
	if err := g.createUDPServer(); err != nil {
		g.Logger.Error(err, "error creating UDP server")
		if err := g.TCPListener.Close(); err != nil && !g.isConnClosed(err) {
			g.Logger.Error(err, "error closing TcpListener")
		}
		return 0
	}
	return port
}

func (g *GameServer) CloseServers() {
	if err := g.UDPListener.Close(); err != nil && !g.isConnClosed(err) {
		g.Logger.Error(err, "error closing UdpListener")
	} else if err == nil {
		g.Logger.Info("UDP server closed")
	}
	if err := g.TCPListener.Close(); err != nil && !g.isConnClosed(err) {
		g.Logger.Error(err, "error closing TcpListener")
	} else if err == nil {
		g.Logger.Info("TCP server closed")
	}
}

func (g *GameServer) isConnClosed(err error) bool {
	if err == nil {
		return false
	}
	return strings.Contains(err.Error(), "use of closed network connection")
}

func (g *GameServer) ManageBuffer() {
	for {
		if !g.Running {
			g.Logger.Info("done managing buffers")
			return
		}
		// Adjust the buffer size for the lead player(s)
		for i := range 4 {
			if g.GameData.BufferHealth[i] != -1 && g.GameData.CountLag[i] == 0 {
				if g.GameData.BufferHealth[i] > BufferTarget && g.GameData.BufferSize[i] > 0 {
					g.GameData.BufferSize[i]--
					// g.Logger.Info("reducing buffer size", "player", i, "bufferSize", g.GameData.BufferSize[i])
				} else if g.GameData.BufferHealth[i] < BufferTarget {
					g.GameData.BufferSize[i]++
					// g.Logger.Info("increasing buffer size", "player", i, "bufferSize", g.GameData.BufferSize[i])
				}
			}
		}
		time.Sleep(time.Second * 5) //nolint:gomnd,mnd
	}
}

func (g *GameServer) ManagePlayers() {
	time.Sleep(time.Second * DisconnectTimeoutS)
	for {
		playersActive := false // used to check if anyone is still around
		var i byte

		g.GameDataMutex.Lock() // PlayerAlive and Status can be modified by processUDP in a different thread
		for i = range 4 {
			_, ok := g.Registrations[i]
			if ok {
				if g.GameData.PlayerAlive[i] {
					g.Logger.Info("player status", "player", i, "regID", g.Registrations[i].RegID, "bufferSize", g.GameData.BufferSize[i], "bufferHealth", g.GameData.BufferHealth[i], "countLag", g.GameData.CountLag[i], "address", g.GameData.PlayerAddresses[i])
					playersActive = true
				} else {
					g.Logger.Info("play disconnected UDP", "player", i, "regID", g.Registrations[i].RegID, "address", g.GameData.PlayerAddresses[i])
					g.GameData.Status |= (0x1 << (i + 1)) //nolint:gomnd,mnd

					g.RegistrationsMutex.Lock() // Registrations can be modified by processTCP
					delete(g.Registrations, i)
					g.RegistrationsMutex.Unlock()
				}
			}
			g.GameData.PlayerAlive[i] = false
		}
		g.GameDataMutex.Unlock()

		if !playersActive {
			g.Logger.Info("no more players, closing room", "numPlayers", len(g.Players), "playTime", time.Since(g.StartTime).String(), "emulator", g.Emulator)
			g.CloseServers()
			g.Running = false
			return
		}
		time.Sleep(time.Second * DisconnectTimeoutS)
	}
}
