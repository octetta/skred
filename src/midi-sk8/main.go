package main

import (
	_ "embed"
	"fmt"
	"image/color"
	"net"
	"regexp"
	"strconv"
	"strings"
	"time"

	"fyne.io/fyne/v2"
	"fyne.io/fyne/v2/app"
	"fyne.io/fyne/v2/canvas"
	"fyne.io/fyne/v2/container"
	"fyne.io/fyne/v2/theme"
	"fyne.io/fyne/v2/widget"
	"gitlab.com/gomidi/midi/v2"
	"gitlab.com/gomidi/midi/v2/drivers"
	_ "gitlab.com/gomidi/midi/v2/drivers/rtmididrv"
)

//go:embed icon.png
var iconBytes []byte

var mathRegex = regexp.MustCompile(`\{([^}]+)\}`)
var parenRegex = regexp.MustCompile(`\(([^()]+)\)`)

// customTheme improves visibility and scaling
type customTheme struct{ fyne.Theme }
func (m customTheme) Size(name fyne.ThemeSizeName) float32 {
	if name == theme.SizeNameText { return 13 }
	if name == theme.SizeNamePadding { return 6 }
	return theme.DefaultTheme().Size(name)
}

type AppState struct {
	udpConn     net.Conn
	stopMidi    func()
	isPaused    bool
	isDark      bool
	midiLog     *widget.Entry
	udpLog      *widget.Entry
	addrEntry   *widget.Entry
	portEntry   *widget.Entry
	midiSelect  *widget.Select
	noteOnTpl   *widget.Entry
	noteOffTpl  *widget.Entry
	pbTpl       *widget.Entry
	manualEntry *widget.Entry
	indicator   *canvas.Circle
}

// solveBase handles basic arithmetic for a single level of expression
func solveBase(expr string) string {
	expr = strings.ReplaceAll(expr, " ", "")
	if matches, _ := regexp.MatchString(`[^0-9+\-*/.]`, expr); matches { return "NAN" }
	operators := "+-*/"
	var ops []rune
	var nums []float64
	curr := ""
	for _, char := range expr {
		if strings.ContainsRune(operators, char) {
			if curr == "" && char == '-' { curr = "-"; continue }
			val, err := strconv.ParseFloat(curr, 64)
			if err != nil { return "VAL_ERR" }
			nums = append(nums, val)
			ops = append(ops, char)
			curr = ""
		} else { curr += string(char) }
	}
	lastVal, err := strconv.ParseFloat(curr, 64)
	if err != nil { return "VAL_ERR" }
	nums = append(nums, lastVal)
	if len(nums) == 0 { return "0" }
	res := nums[0]
	for i, op := range ops {
		if i+1 >= len(nums) { break }
		switch op {
		case '+': res += nums[i+1]
		case '-': res -= nums[i+1]
		case '*': res *= nums[i+1]
		case '/': 
			if nums[i+1] != 0 { res /= nums[i+1] } else { return "DIV0" }
		}
	}
	return fmt.Sprintf("%.4f", res)
}

// evaluate recursively solves nested parentheses
func evaluate(expr string) string {
	for parenRegex.MatchString(expr) {
		expr = parenRegex.ReplaceAllStringFunc(expr, func(m string) string {
			return solveBase(strings.Trim(m, "()"))
		})
	}
	return solveBase(expr)
}

func (s *AppState) transform(tpl string, c, n, v uint8, p uint16) string {
	res := tpl
	res = strings.ReplaceAll(res, "$c", strconv.Itoa(int(c)))
	res = strings.ReplaceAll(res, "$n", strconv.Itoa(int(n)))
	res = strings.ReplaceAll(res, "$v", strconv.Itoa(int(v)))
	res = strings.ReplaceAll(res, "$p", strconv.Itoa(int(p)))
	return mathRegex.ReplaceAllStringFunc(res, func(match string) string {
		return evaluate(strings.Trim(match, "{}"))
	})
}

func (s *AppState) flash() {
	fyne.Do(func() {
		s.indicator.FillColor = color.NRGBA{R: 0, G: 255, B: 0, A: 255}
		s.indicator.Refresh()
	})
	go func() {
		time.Sleep(time.Millisecond * 100)
		fyne.Do(func() {
			s.indicator.FillColor = color.NRGBA{R: 80, G: 80, B: 80, A: 255}
			s.indicator.Refresh()
		})
	}()
}

func main() {
	a := app.NewWithID("com.sk8r.midi-udp")
	a.Settings().SetTheme(customTheme{theme.LightTheme()})
	w := a.NewWindow("midi-sk8")
	w.SetIcon(fyne.NewStaticResource("icon.png", iconBytes))

	s := &AppState{
		midiLog: widget.NewMultiLineEntry(), udpLog: widget.NewMultiLineEntry(),
		addrEntry: widget.NewEntry(), portEntry: widget.NewEntry(),
		midiSelect: widget.NewSelect([]string{}, nil),
		noteOnTpl: widget.NewEntry(), noteOffTpl: widget.NewEntry(), pbTpl: widget.NewEntry(),
		manualEntry: widget.NewEntry(), indicator: canvas.NewCircle(color.NRGBA{80, 80, 80, 255}),
	}
	s.indicator.Resize(fyne.NewSize(14, 14))
	s.midiLog.TextStyle = fyne.TextStyle{Monospace: true}
	s.udpLog.TextStyle = fyne.TextStyle{Monospace: true}

	s.addrEntry.SetText("127.0.0.1"); s.portEntry.SetText("60440")
	s.noteOnTpl.SetText("v$c n$n l{$v/127}"); s.noteOffTpl.SetText("v$c n$n l0")
	s.pbTpl.SetText("v$c p{($p-8192)/8192}")
	s.manualEntry.SetPlaceHolder("Manual UDP Command...")

	// MIDI Port Discovery
	refreshPorts := func() {
		var names []string
		for _, port := range midi.GetInPorts() {
			names = append(names, port.String())
		}
		s.midiSelect.Options = names
		if len(names) > 0 && s.midiSelect.Selected == "" {
			s.midiSelect.SetSelected(names[0])
		}
		s.midiSelect.Refresh()
	}
	refreshPorts()

	// Configuration Forms
	configForm := widget.NewForm(
		widget.NewFormItem("udp-addr", s.addrEntry),
		widget.NewFormItem("udp-port", s.portEntry),
		widget.NewFormItem("midi-in", container.NewBorder(nil, nil, nil, 
			widget.NewButtonWithIcon("", theme.ViewRefreshIcon(), refreshPorts), s.midiSelect)),
	)
	configForm.Hide()

	tplForm := widget.NewForm(
		widget.NewFormItem("note-on", s.noteOnTpl),
		widget.NewFormItem("note-off", s.noteOffTpl),
		widget.NewFormItem("pitch-bend", s.pbTpl),
	)
	tplForm.Hide()

	themeBtn := widget.NewButtonWithIcon("", theme.ColorPaletteIcon(), func() {
		if s.isDark { a.Settings().SetTheme(customTheme{theme.LightTheme()}); s.isDark = false
		} else { a.Settings().SetTheme(customTheme{theme.DarkTheme()}); s.isDark = true }
	})

	settingsToggle := widget.NewButtonWithIcon("", theme.SettingsIcon(), func() {
		if configForm.Hidden { configForm.Show(); tplForm.Show() } else { configForm.Hide(); tplForm.Hide() }
	})

	pauseBtn := widget.NewButtonWithIcon("", theme.MediaPauseIcon(), func() { s.isPaused = !s.isPaused })
	clearBtn := widget.NewButtonWithIcon("", theme.DeleteIcon(), func() { s.midiLog.SetText(""); s.udpLog.SetText("") })

	sendManual := func() {
		if s.manualEntry.Text != "" && s.udpConn != nil {
			s.udpConn.Write([]byte(s.manualEntry.Text))
			fyne.Do(func() { s.udpLog.SetText(s.udpLog.Text + "> " + s.manualEntry.Text + "\n"); s.manualEntry.SetText("") })
			s.flash()
		}
	}
	s.manualEntry.OnSubmitted = func(_ string) { sendManual() }
	manualBox := container.NewBorder(nil, nil, nil, widget.NewButtonWithIcon("", theme.MailSendIcon(), sendManual), s.manualEntry)

	startBtn := widget.NewButtonWithIcon("Connect", theme.CheckButtonCheckedIcon(), func() {
		if s.stopMidi != nil { s.stopMidi() }
		
		conn, err := net.Dial("udp", s.addrEntry.Text+":"+s.portEntry.Text)
		if err != nil { return }
		s.udpConn = conn

		var in drivers.In
		// 1. Try to find the port selected in the dropdown
		for _, p := range midi.GetInPorts() {
			if p.String() == s.midiSelect.Selected {
				in = p
				break
			}
		}

		// 2. Fallback: On Linux, if it's a specific name, try creating a Virtual Port
		if in == nil {
			drv := drivers.Get()
			if vDrv, ok := drv.(interface{ OpenVirtualIn(string) (drivers.In, error) }); ok {
				in, _ = vDrv.OpenVirtualIn("sk8-bridge-1")
			}
		}

		if in == nil {
			s.midiLog.SetText("Error: Port not found. Please refresh and select a valid input.\n")
			return
		}

		s.midiLog.SetText(fmt.Sprintf("Listening to: %s\n", in.String()))

		stop, _ := midi.ListenTo(in, func(msg midi.Message, ts int32) {
			var ch, key, vel uint8
			var bend int16
			var abs uint16
			var out string
			switch {
			case msg.GetNoteOn(&ch, &key, &vel): out = s.transform(s.noteOnTpl.Text, ch, key, vel, 0)
			case msg.GetNoteOff(&ch, &key, &vel): out = s.transform(s.noteOffTpl.Text, ch, key, vel, 0)
			case msg.GetPitchBend(&ch, &bend, &abs): out = s.transform(s.pbTpl.Text, ch, 0, uint8(abs>>7), abs)
			}
			if out != "" && s.udpConn != nil { s.udpConn.Write([]byte(out)) }
			s.flash()
			if !s.isPaused {
				hex := ""
				for _, b := range msg.Bytes() { hex += fmt.Sprintf("%02X ", b) }
				fyne.Do(func() {
					if len(s.midiLog.Text) > 2000 { s.midiLog.SetText(s.midiLog.Text[1000:]) }
					s.midiLog.SetText(s.midiLog.Text + strings.TrimSpace(hex) + "\n")
					s.midiLog.CursorRow = len(strings.Split(s.midiLog.Text, "\n"))
					if out != "" {
						if len(s.udpLog.Text) > 2000 { s.udpLog.SetText(s.udpLog.Text[1000:]) }
						s.udpLog.SetText(s.udpLog.Text + out + "\n")
						s.udpLog.CursorRow = len(strings.Split(s.udpLog.Text, "\n"))
					}
				})
			}
		})
		s.stopMidi = stop
	})

	indicatorBox := container.NewStack(container.NewGridWrap(fyne.NewSize(14, 14), s.indicator))
	header := container.NewBorder(nil, nil, container.NewHBox(themeBtn, settingsToggle), container.NewHBox(indicatorBox, pauseBtn, clearBtn), widget.NewLabelWithStyle("MIDI-SK8", fyne.TextAlignCenter, fyne.TextStyle{Bold: true}))
	
	logStack := container.NewVSplit(
		container.NewBorder(widget.NewLabelWithStyle("MIDI IN", 0, fyne.TextStyle{Italic: true}), nil, nil, nil, s.midiLog),
		container.NewBorder(widget.NewLabelWithStyle("UDP OUT", 0, fyne.TextStyle{Italic: true}), nil, nil, nil, s.udpLog),
	)
	logStack.SetOffset(0.5)

	topArea := container.NewVBox(header, configForm, tplForm, startBtn, manualBox)
	w.SetContent(container.NewBorder(topArea, nil, nil, nil, logStack))
	w.Resize(fyne.NewSize(640, 720))
	w.ShowAndRun()
}
