package gameserver

import (
	"bytes"
	"encoding/binary"
	"errors"
	"fmt"
	"io"
	"net"
	"os"
	"time"
)

type TCPData struct {
	Filename       string
	Buffer         bytes.Buffer
	Filesize       uint32
	Request        byte
	CustomID       byte
	CustomDatasize uint32
}

const (
	SettingsSize = 24
	MaxGames     = 20
	BufferTarget = 2
	TCPTimeout   = time.Minute * 5
)

const (
	RequestNone             = 255
	RequestSendSave         = 1
	RequestReceiveSave      = 2
	RequestSendSettings     = 3
	RequestReceiveSettings  = 4
	RequestRegisterPlayer   = 5
	RequestGetRegistration  = 6
	RequestDisconnectNotice = 7
	RequestSendCustomStart  = 64 // 64-127 are custom data send slots, 128-191 are custom data receive slots
	CustomDataOffset        = 64
)

func (g *GameServer) tcpSendFile(tcpData *TCPData, conn *net.TCPConn) {
	startTime := time.Now()
	var ok bool
	for !ok {
		_, ok = g.TCPFiles[tcpData.Filename]
		if !ok {
			time.Sleep(time.Second)
			if time.Since(startTime) > TCPTimeout {
				g.Logger.Error(fmt.Errorf("tcp timeout"), "TCP connection timed out in tcpSendFile")
				return
			}
		} else {
			_, err := conn.Write(g.TCPFiles[tcpData.Filename])
			if err != nil {
				g.Logger.Error(err, "could not write file", "address", conn.RemoteAddr().String())
			}
			// g.Logger.Info("sent file", "filename", tcpData.Filename, "filesize", tcpData.Filesize, "address", conn.RemoteAddr().String())
			tcpData.Filename = ""
			tcpData.Filesize = 0
		}
	}
}

func (g *GameServer) tcpSendSettings(conn *net.TCPConn) {
	startTime := time.Now()
	for !g.HasSettings {
		time.Sleep(time.Second)
		if time.Since(startTime) > TCPTimeout {
			g.Logger.Error(fmt.Errorf("tcp timeout"), "TCP connection timed out in tcpSendSettings")
			return
		}
	}
	_, err := conn.Write(g.TCPSettings)
	if err != nil {
		g.Logger.Error(err, "could not write settings", "address", conn.RemoteAddr().String())
	}
	// g.Logger.Info("sent settings", "address", conn.RemoteAddr().String())
}

func (g *GameServer) tcpSendCustom(conn *net.TCPConn, customID byte) {
	startTime := time.Now()
	var ok bool
	for !ok {
		_, ok = g.CustomData[customID]
		if !ok {
			time.Sleep(time.Second)
			if time.Since(startTime) > TCPTimeout {
				g.Logger.Error(fmt.Errorf("tcp timeout"), "TCP connection timed out in tcpSendCustom")
				return
			}
		} else {
			_, err := conn.Write(g.CustomData[customID])
			if err != nil {
				g.Logger.Error(err, "could not write data", "address", conn.RemoteAddr().String())
			}
		}
	}
}

func (g *GameServer) tcpSendReg(conn *net.TCPConn) {
	startTime := time.Now()
	for len(g.Players) != len(g.Registrations) {
		time.Sleep(time.Second)
		if time.Since(startTime) > TCPTimeout {
			g.Logger.Error(fmt.Errorf("tcp timeout"), "TCP connection timed out in tcpSendReg")
			return
		}
	}
	var i byte
	registrations := make([]byte, 24) //nolint:gomnd
	current := 0
	for i = 0; i < 4; i++ {
		_, ok := g.Registrations[i]
		if ok {
			binary.BigEndian.PutUint32(registrations[current:], g.Registrations[i].RegID)
			current += 4
			registrations[current] = g.Registrations[i].Plugin
			current++
			registrations[current] = g.Registrations[i].Raw
			current++
		} else {
			current += 6
		}
	}
	// g.Logger.Info("sent registration data", "address", conn.RemoteAddr().String())
	_, err := conn.Write(registrations)
	if err != nil {
		g.Logger.Error(err, "failed to send registration data", "address", conn.RemoteAddr().String())
	}
}

func (g *GameServer) processTCP(conn *net.TCPConn) {
	tcpData := &TCPData{Request: RequestNone}
	incomingBuffer := make([]byte, 1500) //nolint:gomnd
	for {
		err := conn.SetReadDeadline(time.Now().Add(time.Second))
		if err != nil {
			g.Logger.Error(err, "could not set read deadline", "address", conn.RemoteAddr().String())
		}
		length, err := conn.Read(incomingBuffer)
		if errors.Is(err, io.EOF) {
			// g.Logger.Info("Remote side closed TCP connection", "address", conn.RemoteAddr().String())
			return
		}
		if err != nil && !errors.Is(err, os.ErrDeadlineExceeded) {
			g.Logger.Info("could not read TCP message", "reason", err.Error(), "address", conn.RemoteAddr().String())
			continue
		}
		if length > 0 {
			tcpData.Buffer.Write(incomingBuffer[:length])
		}

		if tcpData.Request == RequestNone { // find the request type
			if tcpData.Buffer.Len() > 0 {
				tcpData.Request, err = tcpData.Buffer.ReadByte()
				if err != nil {
					g.Logger.Error(err, "TCP error", "address", conn.RemoteAddr().String())
				}
			} else {
				continue // nothing to do
			}
		}

		if (tcpData.Request == RequestSendSave || tcpData.Request == RequestReceiveSave) && tcpData.Filename == "" { // get file name
			if bytes.IndexByte(tcpData.Buffer.Bytes(), 0) != -1 {
				filenameBytes, err := tcpData.Buffer.ReadBytes(0)
				if err != nil {
					g.Logger.Error(err, "TCP error", "address", conn.RemoteAddr().String())
				}
				tcpData.Filename = string(filenameBytes[:len(filenameBytes)-1])
			}
		}

		if tcpData.Request == RequestSendSave && tcpData.Filename != "" && tcpData.Filesize == 0 { // get file size from sender
			if tcpData.Buffer.Len() >= 4 { //nolint:gomnd
				filesizeBytes := make([]byte, 4)
				_, err = tcpData.Buffer.Read(filesizeBytes)
				if err != nil {
					g.Logger.Error(err, "TCP error", "address", conn.RemoteAddr().String())
				}
				tcpData.Filesize = binary.BigEndian.Uint32(filesizeBytes)
			}
		}

		if tcpData.Request == RequestSendSave && tcpData.Filename != "" && tcpData.Filesize != 0 { // read in file from sender
			if tcpData.Buffer.Len() >= int(tcpData.Filesize) {
				g.TCPFiles[tcpData.Filename] = make([]byte, tcpData.Filesize)
				_, err = tcpData.Buffer.Read(g.TCPFiles[tcpData.Filename])
				if err != nil {
					g.Logger.Error(err, "TCP error", "address", conn.RemoteAddr().String())
				}
				// g.Logger.Info("read file from sender", "filename", tcpData.Filename, "filesize", tcpData.Filesize, "address", conn.RemoteAddr().String())
				tcpData.Filename = ""
				tcpData.Filesize = 0
				tcpData.Request = RequestNone
			}
		}

		if tcpData.Request == RequestReceiveSave && tcpData.Filename != "" { // send requested file
			go g.tcpSendFile(tcpData, conn)
			tcpData.Request = RequestNone
		}

		if tcpData.Request == RequestSendSettings { // get settings from P1
			if tcpData.Buffer.Len() >= SettingsSize {
				_, err = tcpData.Buffer.Read(g.TCPSettings)
				if err != nil {
					g.Logger.Error(err, "TCP error", "address", conn.RemoteAddr().String())
				}
				// g.Logger.Info("read settings via TCP", "bufferLeft", tcpData.Buffer.Len(), "address", conn.RemoteAddr().String())
				g.HasSettings = true
				tcpData.Request = RequestNone
			}
		}

		if tcpData.Request == RequestReceiveSettings { // send settings to P2-4
			go g.tcpSendSettings(conn)
			tcpData.Request = RequestNone
		}

		if tcpData.Request == RequestRegisterPlayer && tcpData.Buffer.Len() >= 7 { // register player
			playerNumber, err := tcpData.Buffer.ReadByte()
			if err != nil {
				g.Logger.Error(err, "TCP error", "address", conn.RemoteAddr().String())
			}
			plugin, err := tcpData.Buffer.ReadByte()
			if err != nil {
				g.Logger.Error(err, "TCP error", "address", conn.RemoteAddr().String())
			}
			raw, err := tcpData.Buffer.ReadByte()
			if err != nil {
				g.Logger.Error(err, "TCP error", "address", conn.RemoteAddr().String())
			}
			regIDBytes := make([]byte, 4) //nolint:gomnd
			_, err = tcpData.Buffer.Read(regIDBytes)
			if err != nil {
				g.Logger.Error(err, "TCP error", "address", conn.RemoteAddr().String())
			}
			regID := binary.BigEndian.Uint32(regIDBytes)

			response := make([]byte, 2) //nolint:gomnd
			_, ok := g.Registrations[playerNumber]
			if !ok {
				if playerNumber > 0 && plugin == 2 { // Only P1 can use mempak
					plugin = 1
				}
				g.Registrations[playerNumber] = &Registration{
					RegID:  regID,
					Plugin: plugin,
					Raw:    raw,
				}
				response[0] = 1
				g.Logger.Info("registered player", "registration", g.Registrations[playerNumber], "number", playerNumber, "bufferLeft", tcpData.Buffer.Len(), "address", conn.RemoteAddr().String())
				g.GameData.PendingPlugin[playerNumber] = plugin
				g.GameData.PlayerAlive[playerNumber] = true
			} else {
				if g.Registrations[playerNumber].RegID == regID {
					g.Logger.Error(fmt.Errorf("re-registration"), "player already registered", "registration", g.Registrations[playerNumber], "number", playerNumber, "bufferLeft", tcpData.Buffer.Len(), "address", conn.RemoteAddr().String())
					response[0] = 1
				} else {
					g.Logger.Error(fmt.Errorf("registration failure"), "could not register player", "registration", g.Registrations[playerNumber], "number", playerNumber, "bufferLeft", tcpData.Buffer.Len(), "address", conn.RemoteAddr().String())
					response[0] = 0
				}
			}
			response[1] = BufferTarget
			_, err = conn.Write(response)
			if err != nil {
				g.Logger.Error(err, "TCP error", "address", conn.RemoteAddr().String())
			}
			tcpData.Request = RequestNone
		}

		if tcpData.Request == RequestGetRegistration { // send registration
			go g.tcpSendReg(conn)
			tcpData.Request = RequestNone
		}

		if tcpData.Request == RequestDisconnectNotice && tcpData.Buffer.Len() >= 4 { // disconnect notice
			regIDBytes := make([]byte, 4) //nolint:gomnd
			_, err = tcpData.Buffer.Read(regIDBytes)
			if err != nil {
				g.Logger.Error(err, "TCP error", "address", conn.RemoteAddr().String())
			}
			regID := binary.BigEndian.Uint32(regIDBytes)
			var i byte
			for i = 0; i < 4; i++ {
				v, ok := g.Registrations[i]
				if ok {
					if v.RegID == regID {
						g.Logger.Info("player disconnected TCP", "regID", regID, "player", i, "address", conn.RemoteAddr().String())
						g.GameData.PlayerAlive[i] = false
						g.GameData.Status |= (0x1 << (i + 1)) //nolint:gomnd
						delete(g.Registrations, i)
					}
				}
			}
			tcpData.Request = RequestNone
		}

		if tcpData.Request >= RequestSendCustomStart && tcpData.Request < RequestSendCustomStart+CustomDataOffset && tcpData.Buffer.Len() >= 4 && tcpData.CustomID == 0 { // get custom data (for example, plugin settings)
			dataSizeBytes := make([]byte, 4) //nolint:gomnd
			_, err = tcpData.Buffer.Read(dataSizeBytes)
			if err != nil {
				g.Logger.Error(err, "TCP error", "address", conn.RemoteAddr().String())
			}
			tcpData.CustomID = tcpData.Request
			tcpData.CustomDatasize = binary.BigEndian.Uint32(dataSizeBytes)
		}

		if tcpData.Request >= RequestSendCustomStart && tcpData.Request < RequestSendCustomStart+CustomDataOffset && tcpData.CustomID != 0 { // read in custom data from sender
			if tcpData.Buffer.Len() >= int(tcpData.CustomDatasize) {
				g.CustomData[tcpData.CustomID] = make([]byte, tcpData.CustomDatasize)
				_, err = tcpData.Buffer.Read(g.CustomData[tcpData.CustomID])
				if err != nil {
					g.Logger.Error(err, "TCP error", "address", conn.RemoteAddr().String())
				}
				tcpData.CustomID = 0
				tcpData.CustomDatasize = 0
				tcpData.Request = RequestNone
			}
		}

		if tcpData.Request >= RequestSendCustomStart+CustomDataOffset && tcpData.Request < RequestSendCustomStart+CustomDataOffset+CustomDataOffset { // send custom data (for example, plugin settings)
			go g.tcpSendCustom(conn, tcpData.Request-CustomDataOffset)
			tcpData.Request = RequestNone
		}
	}
}

func (g *GameServer) watchTCP() {
	for {
		conn, err := g.TCPListener.AcceptTCP()
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
	for i := 0; i < MaxGames; i++ {
		g.TCPListener, err = net.ListenTCP("tcp", &net.TCPAddr{Port: basePort + i})
		if err == nil {
			g.Port = basePort + i
			g.Logger.Info("Created TCP server", "port", g.Port)
			g.TCPFiles = make(map[string][]byte)
			g.CustomData = make(map[byte][]byte)
			g.TCPSettings = make([]byte, SettingsSize)
			g.Registrations = map[byte]*Registration{}
			go g.watchTCP()
			return g.Port
		}
	}
	return 0
}
