package main

import (
	_ "embed"
	"encoding/json"
	"fmt"
	"net"
	"os"
	"strconv"
	"time"

	"fyne.io/fyne/v2"
	"fyne.io/fyne/v2/app"
	"fyne.io/fyne/v2/container"
	"fyne.io/fyne/v2/dialog"
	"fyne.io/fyne/v2/layout"
	"fyne.io/fyne/v2/storage"
	"fyne.io/fyne/v2/theme"
	"fyne.io/fyne/v2/widget"
)

//go:embed icon.png
var iconBytes []byte

var (
	activeWindows []*WindowInstance
	globalApp     fyne.App
)

type monitorTheme struct {
	fyne.Theme 
}

func (m *monitorTheme) Size(name fyne.ThemeSizeName) float32 {
	if name == theme.SizeNameText { return 11 }
	return m.Theme.Size(name)
}

func newMonitorTheme() fyne.Theme {
	return &monitorTheme{Theme: theme.DefaultTheme()}
}

type doubleClickEntry struct {
	widget.Entry
	onDoubleClick func()
	lastTap       time.Time
}

func newDoubleClickEntry(fn func()) *doubleClickEntry {
	e := &doubleClickEntry{onDoubleClick: fn}
	e.ExtendBaseWidget(e)
	return e
}

func (e *doubleClickEntry) Tapped(ev *fyne.PointEvent) {
	now := time.Now()
	if now.Sub(e.lastTap) < 300*time.Millisecond {
		e.onDoubleClick()
	}
	e.lastTap = now
	e.Entry.Tapped(ev)
}

type WinData struct {
	Title       string   `json:"title"`
	ActiveVoice int      `json:"active_voice"`
	SliderVal   float64  `json:"slider_val"`
	Params      []string `json:"params"`
	IsMinimized bool     `json:"is_minimized"`
	WidthFull   float32  `json:"width_full"`
	WidthMin    float32  `json:"width_min"`
}

type WindowInstance struct {
	win          fyne.Window
	conn         net.Conn
	activeVoice  int
	slider       *widget.Slider
	udpMonitor   *widget.Label
	titleEntry   *doubleClickEntry
	voiceButtons []*widget.Button
	min, max, step, wire, addr, port *widget.Entry
	
	widthFull    float32
	widthMin     float32
	isMinimized  bool
	mainMenu     *fyne.MainMenu
	refreshUI    func() 
}

func serializeSession() []WinData {
	var session []WinData
	for _, w := range activeWindows {
		session = append(session, WinData{
			Title:       w.win.Title(),
			ActiveVoice: w.activeVoice,
			SliderVal:   w.slider.Value,
			Params:      []string{w.min.Text, w.max.Text, w.step.Text, w.wire.Text, w.addr.Text, w.port.Text},
			IsMinimized: w.isMinimized,
			WidthFull:   w.widthFull,
			WidthMin:    w.widthMin,
		})
	}
	return session
}

func quickSave() {
	if len(activeWindows) == 0 { return }
	data := serializeSession()
	file, _ := os.Create("autosave.sk8")
	defer file.Close()
	json.NewEncoder(file).Encode(data)
}

func saveAll(parent fyne.Window) {
	session := serializeSession()
	d := dialog.NewFileSave(func(writer fyne.URIWriteCloser, err error) {
		if writer == nil || err != nil { return }
		defer writer.Close()
		json.NewEncoder(writer).Encode(session)
	}, parent)
	d.SetFilter(storage.NewExtensionFileFilter([]string{".sk8"}))
	d.SetFileName("session.sk8") 
	d.Show()
}

func loadSession(parent fyne.Window) {
	d := dialog.NewFileOpen(func(reader fyne.URIReadCloser, err error) {
		if reader == nil || err != nil { return }
		defer reader.Close()
		var session []WinData
		if err := json.NewDecoder(reader).Decode(&session); err != nil { return }
		
		// Close current windows before loading new session
		for len(activeWindows) > 0 {
			activeWindows[0].win.Close()
		}

		for _, item := range session {
			inst := &WindowInstance{}
			inst.widthFull = item.WidthFull
			inst.widthMin = item.WidthMin
			inst.isMinimized = item.IsMinimized
			
			inst.spawn(item.Params)
			inst.win.SetTitle(item.Title)
			inst.updateVoice(item.ActiveVoice)
			inst.slider.SetValue(item.SliderVal)
			inst.refreshUI() 
		}
	}, parent)
	d.SetFilter(storage.NewExtensionFileFilter([]string{".sk8"}))
	d.Show()
}

func (w *WindowInstance) updateVoice(id int) {
	for i, btn := range w.voiceButtons {
		if i == id {
			w.activeVoice = id
			btn.Importance = widget.HighImportance
		} else {
			btn.Importance = widget.MediumImportance
		}
		btn.Refresh()
	}
	if id < 0 { w.activeVoice = -1 }
}

func (w *WindowInstance) sendUDP(msg string) {
	if w.activeVoice >= 0 { msg = fmt.Sprintf("v%d %s", w.activeVoice, msg) }
	w.udpMonitor.SetText(msg)
	if w.conn != nil { w.conn.Write([]byte(msg)) }
}

func (w *WindowInstance) spawn(p []string) {
	w.win = globalApp.NewWindow("sk8r")
	if len(iconBytes) > 0 { w.win.SetIcon(fyne.NewStaticResource("icon.png", iconBytes)) }
	activeWindows = append(activeWindows, w)
	w.activeVoice = -1
	
	if w.widthFull == 0 { w.widthFull = 700 }
	if w.widthMin == 0 { w.widthMin = 400 }

	w.win.SetOnClosed(func() {
		for i, v := range activeWindows {
			if v == w {
				activeWindows = append(activeWindows[:i], activeWindows[i+1:]...)
				break
			}
		}
	})

	w.min = widget.NewEntry(); w.min.SetText(p[0])
	w.max = widget.NewEntry(); w.max.SetText(p[1])
	w.step = widget.NewEntry(); w.step.SetText(p[2])
	w.wire = widget.NewEntry(); w.wire.SetText(p[3])
	w.addr = widget.NewEntry(); w.addr.SetText(p[4])
	w.port = widget.NewEntry(); w.port.SetText(p[5])

	w.udpMonitor = widget.NewLabelWithStyle("READY", fyne.TextAlignLeading, fyne.TextStyle{Monospace: true})
	manualEntry := widget.NewEntry(); manualEntry.SetPlaceHolder("Cmd...")
	manualBox := container.NewBorder(nil, nil, nil, widget.NewButton("Send", func() { w.sendUDP(manualEntry.Text) }), manualEntry)

	w.slider = widget.NewSlider(0, 100)
	w.slider.OnChanged = func(v float64) {
		w.sendUDP(fmt.Sprintf(w.wire.Text, strconv.FormatFloat(v, 'f', 4, 64)))
	}

	applySettings := func() {
		if w.conn != nil { w.conn.Close() }
		c, _ := net.Dial("udp", w.addr.Text+":"+w.port.Text)
		w.conn = c
		minV, _ := strconv.ParseFloat(w.min.Text, 64)
		maxV, _ := strconv.ParseFloat(w.max.Text, 64)
		w.slider.Min, w.slider.Max = minV, maxV
		w.slider.Step, _ = strconv.ParseFloat(w.step.Text, 64)
		w.slider.Refresh()
	}
	applySettings()

	w.voiceButtons = make([]*widget.Button, 64)
	grid := container.New(layout.NewGridLayout(16))
	for i := 0; i < 64; i++ {
		id := i
		btn := widget.NewButton(fmt.Sprintf("v%d", id), func() {
			if w.activeVoice == id { w.updateVoice(-1) } else { w.updateVoice(id) }
		})
		w.voiceButtons[id] = btn
		grid.Add(btn)
	}

	configPanel := container.NewVBox(
		container.New(layout.NewFormLayout(), 
			widget.NewLabel("Net"), container.NewGridWithColumns(2, w.addr, w.port),
			widget.NewLabel("Range"), container.NewGridWithColumns(3, w.min, w.max, w.step),
			widget.NewLabel("Wire"), w.wire),
		widget.NewButton("Apply", applySettings),
	)
	configPanel.Hide()

	w.mainMenu = fyne.NewMainMenu(fyne.NewMenu("File",
		fyne.NewMenuItem("Quick Save", quickSave),
		fyne.NewMenuItem("Save Session As...", func() { saveAll(w.win) }),
		fyne.NewMenuItem("Open Session...", func() { loadSession(w.win) }),
	))

	minMaxBtn := widget.NewButtonWithIcon("", theme.ViewRefreshIcon(), nil)
	
	w.refreshUI = func() {
		targetWidth := w.widthFull
		if w.isMinimized { targetWidth = w.widthMin }

		settingsBtn := widget.NewButtonWithIcon("", theme.SettingsIcon(), func() {
			if configPanel.Hidden { configPanel.Show() } else { configPanel.Hide() }
			w.win.Resize(fyne.NewSize(w.win.Canvas().Size().Width, w.win.Content().MinSize().Height))
		})
		
		top := container.NewBorder(nil, nil, 
			widget.NewButtonWithIcon("", theme.ContentCopyIcon(), func() { (&WindowInstance{}).spawn(p) }),
			settingsBtn, w.titleEntry)
		
		sliderRow := container.NewBorder(nil, nil, minMaxBtn, nil, w.slider)
		monitorContainer := container.NewThemeOverride(w.udpMonitor, newMonitorTheme())

		if w.isMinimized {
			minMaxBtn.SetIcon(theme.ViewFullScreenIcon())
			w.win.SetMainMenu(nil)
			footer := container.NewVBox(monitorContainer, sliderRow)
			w.win.SetContent(container.NewPadded(footer))
		} else {
			minMaxBtn.SetIcon(theme.ViewRefreshIcon())
			w.win.SetMainMenu(w.mainMenu)
			footer := container.NewVBox(widget.NewSeparator(), monitorContainer, sliderRow)
			mainBody := container.NewVBox(top, configPanel, grid, manualBox)
			w.win.SetContent(container.NewBorder(mainBody, footer, nil, nil, nil))
		}

		w.win.Resize(fyne.NewSize(0, 0))
		w.win.Content().Refresh()
		w.win.Resize(fyne.NewSize(targetWidth, w.win.Content().MinSize().Height))
	}

	toggle := func() {
		if w.isMinimized { w.widthMin = w.win.Canvas().Size().Width
		} else { w.widthFull = w.win.Canvas().Size().Width }
		w.isMinimized = !w.isMinimized
		w.refreshUI()
	}

	minMaxBtn.OnTapped = toggle
	w.titleEntry = newDoubleClickEntry(toggle)
	w.titleEntry.OnSubmitted = func(s string) { w.win.SetTitle(s) }

	w.refreshUI()
	w.win.Show()
}

func main() {
	globalApp = app.NewWithID("com.sk8r.multi")
	defaults := []string{"0", "880", "0.0001", "f%s", "127.0.0.1", "60440"}
	(&WindowInstance{}).spawn(defaults)
	globalApp.Run()
}