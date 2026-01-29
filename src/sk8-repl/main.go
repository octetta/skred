package main

import (
	"bufio"
	"io"
	"net"
	"strconv"
	"strings"

	"fyne.io/fyne/v2"
	"fyne.io/fyne/v2/app"
	"fyne.io/fyne/v2/container"
	"fyne.io/fyne/v2/theme"
	"fyne.io/fyne/v2/widget"
	"github.com/fyne-io/terminal"
)

type UDPSession struct {
	term *terminal.Terminal
	conn *net.UDPConn
}

func main() {
	myApp := app.New()
	window := myApp.NewWindow("UDP Shell Pro")
	s := &UDPSession{term: terminal.New()}

	addrEntry := widget.NewEntry()
	addrEntry.SetText("127.0.0.1:60440")
	initEntry := widget.NewEntry()
	initEntry.SetPlaceHolder("Optional init string...")
	
	settings := container.NewVBox(widget.NewForm(
		widget.NewFormItem("Address", addrEntry),
		widget.NewFormItem("Init String", initEntry),
	))
	settings.Hide()

	playBtn := widget.NewButtonWithIcon("", theme.MediaPlayIcon(), func() {
		s.connect(addrEntry.Text, initEntry.Text)
		settings.Hide()
		window.Canvas().Focus(s.term)
	})

	top := container.NewHBox(widget.NewButtonWithIcon("", theme.SettingsIcon(), func() {
		if settings.Hidden { settings.Show() } else { settings.Hide() }
	}), playBtn)

	window.SetContent(container.NewBorder(container.NewVBox(top, settings), nil, nil, nil, s.term))
	window.Resize(fyne.NewSize(800, 600))
	window.ShowAndRun()
}

func (s *UDPSession) connect(target, init string) {
	addr, _ := net.ResolveUDPAddr("udp", target)
	conn, err := net.DialUDP("udp", nil, addr)
	if err != nil {
		s.term.Write([]byte("!! Error: " + err.Error() + "\r\n"))
		return
	}
	s.conn = conn

	logicReader, termOut := io.Pipe()
	termIn, logicWriter := io.Pipe()
	go s.term.RunWithConnection(termOut, termIn)
	s.term.Write([]byte("# Connected\r\n# "))

	if init != "" {
		s.conn.Write([]byte(init + "\n"))
	}

	go func() {
		reader := bufio.NewReader(logicReader)
		var lineBuffer []rune
		cursorPos := 0
		var history []string
		hIdx := -1

		redraw := func() {
			// Move to start (\r), Clear to end (\x1b[K), print prompt + buffer
			logicWriter.Write([]byte("\r\x1b[K# " + string(lineBuffer)))
			
			// If we aren't at the end of the line, move cursor back to cursorPos
			if dist := len(lineBuffer) - cursorPos; dist > 0 {
				moveLeft := "\x1b[" + strconv.Itoa(dist) + "D"
				logicWriter.Write([]byte(moveLeft))
			}
		}

		for {
			char, _, err := reader.ReadRune()
			if err != nil { break }

			// Handle ANSI Escape Sequences (Arrows)
			if char == '\x1b' {
				next, _ := reader.Peek(2)
				if len(next) == 2 && next[0] == '[' {
					reader.Discard(2)
					switch next[1] {
					case 'A', 'B': // UP/DOWN (History)
						if next[1] == 'A' && hIdx < len(history)-1 { hIdx++ } else if next[1] == 'B' && hIdx > -1 { hIdx-- }
						if hIdx == -1 { lineBuffer = []rune("") } else { lineBuffer = []rune(history[len(history)-1-hIdx]) }
						cursorPos = len(lineBuffer)
						redraw()
					case 'C': // RIGHT
						if cursorPos < len(lineBuffer) {
							cursorPos++
							logicWriter.Write([]byte("\x1b[C"))
						}
					case 'D': // LEFT
						if cursorPos > 0 {
							cursorPos--
							logicWriter.Write([]byte("\x1b[D"))
						}
					}
					continue
				}
			}

			// Handle Input Actions
			if char == '\r' || char == '\n' {
				logicWriter.Write([]byte("\r\n"))
				cmd := string(lineBuffer)
				if strings.TrimSpace(cmd) != "" {
					s.conn.Write([]byte(cmd + "\n"))
					history = append(history, cmd)
				}
				lineBuffer, cursorPos, hIdx = []rune(""), 0, -1
				logicWriter.Write([]byte("# "))
			} else if char == '\x7f' || char == '\b' { // Backspace
				if cursorPos > 0 {
					lineBuffer = append(lineBuffer[:cursorPos-1], lineBuffer[cursorPos:]...)
					cursorPos--
					redraw()
				}
			} else { // Typing/Inserting
				newLine := append(lineBuffer[:cursorPos], char)
				lineBuffer = append(newLine, lineBuffer[cursorPos:]...)
				cursorPos++
				redraw()
			}
		}
	}()

	// Inbound Network Logic
	go func() {
		buf := make([]byte, 65535)
		for {
			n, _, err := s.conn.ReadFromUDP(buf)
			if err != nil { return }
			msg := strings.ReplaceAll(string(buf[:n]), "\n", "\r\n")
			// Clear line before printing message to prevent UI overlap
			logicWriter.Write([]byte("\r\x1b[K" + msg + "\r\n# "))
		}
	}()
}
