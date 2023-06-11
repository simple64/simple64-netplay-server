package gameserver

import (
	"bytes"
	"encoding/binary"
	"net"
	"time"
)

type TCPData struct {
	Request  byte
	Buffer   bytes.Buffer
	Filename string
	Filesize uint32
}

const SETTINGS_SIZE = 28
const MAX_GAMES = 20

const (
	REQUEST_NONE              = 255
	REQUEST_SEND_SAVE         = 1
	REQUEST_RECEIVE_SAVE      = 2
	REQUEST_SEND_SETTINGS     = 3
	REQUEST_RECEIVE_SETTINGS  = 4
	REQUEST_REGISTER_PLAYER   = 5
	REQUEST_GET_REGISTRATION  = 6
	REQUEST_DISCONNECT_NOTICE = 7
)

func (g *GameServer) tcpSendFile(tcpData *TCPData, conn *net.TCPConn) {
	var ok bool
	for !ok {
		_, ok = g.TCPFiles[tcpData.Filename]
		if !ok {
			time.Sleep(time.Second)
		} else {
			_, err := conn.Write(g.TCPFiles[tcpData.Filename])
			if err != nil {
				g.Logger.Error(err, "could not write file")
			}
			tcpData.Filename = ""
			tcpData.Filesize = 0
			tcpData.Request = REQUEST_NONE
		}
	}
}
func (g *GameServer) tcpSendSettings(tcpData *TCPData, conn *net.TCPConn) {
	for !g.HasSettings {
		if !g.HasSettings {
			time.Sleep(time.Second)
		} else {
			_, err := conn.Write(g.TCPSettings)
			if err != nil {
				g.Logger.Error(err, "could not write settings")
				tcpData.Request = REQUEST_NONE
			}
		}
	}
}

func (g *GameServer) processTCP(conn *net.TCPConn) {
	_, ok := g.TCPState[conn.RemoteAddr().String()]
	if !ok {
		g.TCPState[conn.RemoteAddr().String()] = &TCPData{Request: REQUEST_NONE}
	}
	tcpData := g.TCPState[conn.RemoteAddr().String()]
	_, err := tcpData.Buffer.ReadFrom(conn)
	if err != nil {
		g.Logger.Error(err, "could not read TCP data")
	}
	process := true
	for process {
		process = false
		if tcpData.Request == REQUEST_NONE {
			if tcpData.Buffer.Len() > 0 {
				tcpData.Request, err = tcpData.Buffer.ReadByte()
				if err != nil {
					g.Logger.Error(err, "could not read request")
				}
				process = true
			}
		}
		data := tcpData.Buffer.Bytes()
		nullIndex := bytes.IndexByte(data, 0)
		if (tcpData.Request == REQUEST_SEND_SAVE || tcpData.Request == REQUEST_RECEIVE_SAVE) && (nullIndex != -1 && tcpData.Filename == "") { // get file name
			filenameBytes, err := tcpData.Buffer.ReadBytes(0)
			tcpData.Filename = string(filenameBytes)
			if err != nil {
				g.Logger.Error(err, "could not read filename")
			}
			_, err = tcpData.Buffer.ReadByte() // skip the \0
			if err != nil {
				g.Logger.Error(err, "error reading byte")
			}
			process = true
		}
		if tcpData.Filename != "" && tcpData.Request == REQUEST_SEND_SAVE && tcpData.Filesize == 0 { // get file size from sender
			if tcpData.Buffer.Len() >= 4 {
				filesizeBytes := make([]byte, 4)
				_, err = tcpData.Buffer.Read(filesizeBytes)
				if err != nil {
					g.Logger.Error(err, "could not read filesize")
				}
				tcpData.Filesize = binary.BigEndian.Uint32(filesizeBytes)
				process = true
			}
		}
		if tcpData.Filename != "" && tcpData.Request == REQUEST_SEND_SAVE && tcpData.Filesize != 0 { // read in file from sender
			if tcpData.Buffer.Len() >= int(tcpData.Filesize) {
				g.TCPFiles[tcpData.Filename] = make([]byte, tcpData.Filesize)
				_, err = tcpData.Buffer.Read(g.TCPFiles[tcpData.Filename])
				if err != nil {
					g.Logger.Error(err, "could not read file")
				}
				tcpData.Filename = ""
				tcpData.Filesize = 0
				tcpData.Request = REQUEST_NONE
				process = true
			}
		}
		if tcpData.Filename != "" && tcpData.Request == REQUEST_RECEIVE_SAVE { // send requested file
			go g.tcpSendFile(tcpData, conn)
		}
		if tcpData.Request == REQUEST_SEND_SETTINGS { // get settings from P1
			if tcpData.Buffer.Len() >= SETTINGS_SIZE {
				_, err = tcpData.Buffer.Read(g.TCPSettings)
				if err != nil {
					g.Logger.Error(err, "could not read settings")
				}
				g.HasSettings = true
				tcpData.Request = REQUEST_NONE
				process = true
			}
		}
		if tcpData.Request == REQUEST_RECEIVE_SETTINGS { // send settings to P2-4
			go g.tcpSendSettings(tcpData, conn)
		}
		if tcpData.Request == REQUEST_REGISTER_PLAYER && tcpData.Buffer.Len() >= 7 { // register player
			tcpData.Request = REQUEST_NONE
		}
		if tcpData.Request == REQUEST_GET_REGISTRATION { // send registration
			tcpData.Request = REQUEST_NONE
		}
		if tcpData.Request == REQUEST_DISCONNECT_NOTICE && tcpData.Buffer.Len() >= 4 { // disconnect notice
			tcpData.Request = REQUEST_NONE
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
		g.processTCP(conn)
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
			g.TCPState = make(map[string]*TCPData)
			g.TCPFiles = make(map[string][]byte)
			g.TCPSettings = make([]byte, SETTINGS_SIZE)
			g.watchTCP()
			return g.Port
		}
	}
	return 0
}
