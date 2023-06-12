package gameserver

import (
	"bytes"
	"encoding/binary"
	"io"
	"net"
	"time"
)

type TCPData struct {
	Request  byte
	Buffer   bytes.Buffer
	Filename string
	Filesize uint32
}

const (
	SETTINGS_SIZE = 28
	MAX_GAMES     = 20
	BUFFER_TARGET = 2
)

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
			g.Logger.Info("sent file", "filename", tcpData.Filename, "filesize", tcpData.Filesize, "address", conn.RemoteAddr().String())
			tcpData.Filename = ""
			tcpData.Filesize = 0
		}
	}
}

func (g *GameServer) tcpSendSettings(tcpData *TCPData, conn *net.TCPConn) {
	for !g.HasSettings {
		time.Sleep(time.Second)
	}
	_, err := conn.Write(g.TCPSettings)
	if err != nil {
		g.Logger.Error(err, "could not write settings")
	}
	g.Logger.Info("sent settings", "address", conn.RemoteAddr().String())
}

func (g *GameServer) tcpSendReg(tcpData *TCPData, conn *net.TCPConn) {
	for len(g.Players) != len(g.Registrations) {
		time.Sleep(time.Second)
	}
	var i byte
	registrations := make([]byte, 24)
	current := 0
	for i = 0; i < 4; i++ {
		_, ok := g.Registrations[i]
		if ok {
			binary.BigEndian.PutUint32(registrations[current:], g.Registrations[i].RegId)
			current += 4
			registrations[current] = g.Registrations[i].Plugin
			current += 1
			registrations[current] = g.Registrations[i].Raw
			current += 1
		} else {
			current += 6
		}
	}
	g.Logger.Info("sent registration data", "address", conn.RemoteAddr().String())
	_, err := conn.Write(registrations)
	if err != nil {
		g.Logger.Error(err, "failed to send registration data")
	}
}

func (g *GameServer) processTCP(conn *net.TCPConn) {
	tcpData := &TCPData{Request: REQUEST_NONE}
	incomingBuffer := make([]byte, 1024)
	for {
		length, err := conn.Read(incomingBuffer)
		if err == io.EOF {
			g.Logger.Info("Remote side closed TCP connection", "address", conn.RemoteAddr().String())
			return
		}
		if err != nil {
			g.Logger.Error(err, "TCP error")
		}
		tcpData.Buffer.Write(incomingBuffer[:length])
		process := true
		for process {
			process = false
			if tcpData.Request == REQUEST_NONE {
				if tcpData.Buffer.Len() > 0 {
					tcpData.Request, err = tcpData.Buffer.ReadByte()
					if err != nil {
						g.Logger.Error(err, "TCP error")
					}
					process = true
				}
			}
			data := tcpData.Buffer.Bytes()
			nullIndex := bytes.IndexByte(data, 0)
			if (tcpData.Request == REQUEST_SEND_SAVE || tcpData.Request == REQUEST_RECEIVE_SAVE) && (nullIndex != -1 && tcpData.Filename == "") { // get file name
				filenameBytes, err := tcpData.Buffer.ReadBytes(0)
				tcpData.Filename = string(filenameBytes[:len(filenameBytes)-1])
				if err != nil {
					g.Logger.Error(err, "TCP error")
				}
				process = true
			}
			if tcpData.Filename != "" && tcpData.Request == REQUEST_SEND_SAVE && tcpData.Filesize == 0 { // get file size from sender
				if tcpData.Buffer.Len() >= 4 {
					filesizeBytes := make([]byte, 4)
					_, err = tcpData.Buffer.Read(filesizeBytes)
					if err != nil {
						g.Logger.Error(err, "TCP error")
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
						g.Logger.Error(err, "TCP error")
					}
					g.Logger.Info("read file from sender", "filename", tcpData.Filename, "filesize", tcpData.Filesize, "address", conn.RemoteAddr().String())
					tcpData.Filename = ""
					tcpData.Filesize = 0
					tcpData.Request = REQUEST_NONE
					process = true
				}
			}
			if tcpData.Filename != "" && tcpData.Request == REQUEST_RECEIVE_SAVE { // send requested file
				go g.tcpSendFile(tcpData, conn)
				tcpData.Request = REQUEST_NONE
			}
			if tcpData.Request == REQUEST_SEND_SETTINGS { // get settings from P1
				if tcpData.Buffer.Len() >= SETTINGS_SIZE {
					_, err = tcpData.Buffer.Read(g.TCPSettings)
					if err != nil {
						g.Logger.Error(err, "TCP error")
					}
					g.Logger.Info("read settings via TCP", "bufferLeft", tcpData.Buffer.Len(), "address", conn.RemoteAddr().String())
					g.HasSettings = true
					tcpData.Request = REQUEST_NONE
					process = true
				}
			}
			if tcpData.Request == REQUEST_RECEIVE_SETTINGS { // send settings to P2-4
				go g.tcpSendSettings(tcpData, conn)
				tcpData.Request = REQUEST_NONE
			}
			if tcpData.Request == REQUEST_REGISTER_PLAYER && tcpData.Buffer.Len() >= 7 { // register player
				playerNumber, err := tcpData.Buffer.ReadByte()
				if err != nil {
					g.Logger.Error(err, "TCP error")
				}
				plugin, err := tcpData.Buffer.ReadByte()
				if err != nil {
					g.Logger.Error(err, "TCP error")
				}
				raw, err := tcpData.Buffer.ReadByte()
				if err != nil {
					g.Logger.Error(err, "TCP error")
				}
				regIdBytes := make([]byte, 4)
				_, err = tcpData.Buffer.Read(regIdBytes)
				if err != nil {
					g.Logger.Error(err, "TCP error")
				}
				regId := binary.BigEndian.Uint32(regIdBytes)

				response := make([]byte, 2)
				_, ok := g.Registrations[playerNumber]
				if !ok {
					if playerNumber > 0 && plugin == 2 { // Only P1 can use mempak
						plugin = 1
					}
					g.Registrations[playerNumber] = &Registration{
						RegId:  regId,
						Plugin: plugin,
						Raw:    raw,
					}
					response[0] = 1
					g.Logger.Info("registered player", "registration", g.Registrations[playerNumber], "number", playerNumber, "bufferLeft", tcpData.Buffer.Len(), "address", conn.RemoteAddr().String())
					g.GameData.PendingPlugin[playerNumber] = plugin
					g.GameData.PlayerAlive[playerNumber] = true
				} else {
					g.Logger.Info("player already registered", "registration", g.Registrations[playerNumber], "number", playerNumber, "bufferLeft", tcpData.Buffer.Len(), "address", conn.RemoteAddr().String())
					if g.Registrations[playerNumber].RegId == regId {
						response[0] = 1
					} else {
						response[0] = 0
					}
				}
				response[1] = BUFFER_TARGET
				_, err = conn.Write(response)
				if err != nil {
					g.Logger.Error(err, "TCP error")
				}
				tcpData.Request = REQUEST_NONE
				process = true
			}
			if tcpData.Request == REQUEST_GET_REGISTRATION { // send registration
				go g.tcpSendReg(tcpData, conn)
				tcpData.Request = REQUEST_NONE
			}
			if tcpData.Request == REQUEST_DISCONNECT_NOTICE && tcpData.Buffer.Len() >= 4 { // disconnect notice
				regIdBytes := make([]byte, 4)
				_, err = tcpData.Buffer.Read(regIdBytes)
				if err != nil {
					g.Logger.Error(err, "TCP error")
				}
				regId := binary.BigEndian.Uint32(regIdBytes)
				var i byte
				for i = 0; i < 4; i++ {
					v, ok := g.Registrations[i]
					if ok {
						if v.RegId == regId {
							g.Logger.Info("player disconnected TCP", "regID", regId, "player", i)
							g.GameData.PlayerAlive[i] = false
							g.GameData.Status |= (0x1 << (i + 1))
							delete(g.Registrations, i)
						}
					}
				}
				tcpData.Request = REQUEST_NONE
				process = true
			}
		}
	}
}

func (g *GameServer) watchTCP() {
	for {
		conn, err := g.TcpListener.AcceptTCP()
		if err != nil && !g.isConnClosed(err) {
			g.Logger.Error(err, "error from TcpListener")
			continue
		} else if g.isConnClosed(err) {
			return
		}
		g.Logger.Info("received TCP connection", "address", conn.RemoteAddr().String())
		go g.processTCP(conn)
	}
}

func (g *GameServer) createTCPServer(basePort int) int {
	var err error
	for i := 0; i < MAX_GAMES; i++ {
		g.TcpListener, err = net.ListenTCP("tcp", &net.TCPAddr{Port: basePort + i})
		if err == nil {
			g.Port = basePort + i
			g.Logger.Info("Created TCP server", "port", g.Port)
			g.TCPFiles = make(map[string][]byte)
			g.TCPSettings = make([]byte, SETTINGS_SIZE)
			g.Registrations = map[byte]*Registration{}
			go g.watchTCP()
			return g.Port
		}
	}
	return 0
}
