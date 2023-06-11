package gameserver

import (
	"bytes"
	"net"
)

type TCPData struct {
	Request  byte
	Buffer   bytes.Buffer
	Filename string
}

const MAX_GAMES = 20

func (g *GameServer) processTCP(conn *net.TCPConn) {
	_, err := g.TCPData.Buffer.ReadFrom(conn)
	if err != nil {
		g.Logger.Error(err, "could not read TCP data")
	}
	process := true
	for process {
		process = false
		if g.TCPData.Request == 255 {
			if g.TCPData.Buffer.Len() > 0 {
				g.TCPData.Request, err = g.TCPData.Buffer.ReadByte()
				if err != nil {
					g.Logger.Error(err, "could not read request")
				}
				process = true
			}
		}
		data := g.TCPData.Buffer.Bytes()
		nullIndex := bytes.IndexByte(data, 0)
		if (g.TCPData.Request == 1 || g.TCPData.Request == 2) && (nullIndex != -1 && g.TCPData.Filename == "") { // get file name
			g.TCPData.Filename = string(data[0 : nullIndex+1])
			g.TCPData.Buffer.Next(nullIndex + 1)
			process = true
		}
	}
}

func (g *GameServer) watchTCP() {
	for {
		conn, err := g.TcpListener.AcceptTCP()
		if err != nil {
			g.Logger.Error(err, "TCP error")
			continue
		}
		go g.processTCP(conn)
	}
}

func (g *GameServer) createTCPServer(basePort int) int {
	var err error
	for i := 0; i < MAX_GAMES; i++ {
		g.TcpListener, err = net.ListenTCP("tcp", &net.TCPAddr{Port: basePort + i})
		if err == nil {
			defer g.TcpListener.Close()
			g.Port = basePort + i
			g.Logger.Info("Created TCP server", "port", g.Port)
			g.TCPData.Request = 255
			g.watchTCP()
			return g.Port
		}
	}
	return 0
}
