package main

import (
	_ "embed"
	"encoding/json"
	"fmt"
	"net"
	"os"
	"strconv"

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
	isDark        = true
	activeWindows []*WindowInstance
	globalApp     fyne.App
)

// Custom Compact Theme Logic
type compactTheme struct{ fyne.Theme }

func (m compactTheme) Size(name fyne.ThemeSizeName) float32 {
	if name == theme.SizeNameText { return 10 }
	if name == theme.SizeNamePadding { return 2 }
	if name == theme.SizeNameInlineIcon { return 12 }
	return theme.DefaultTheme().Size(name)
}

type WinData struct {
	Title       string   `json:"title"`
	ActiveVoice int      `json:"active_voice"`
	Data        []string `json:"data"`
}

type WindowInstance struct {
	win          fyne.Window
	conn         net.Conn
	activeVoice  int
	slider       *widget.Slider
	udpMonitor   *widget.Label
	titleEntry   *widget.Entry
	voiceButtons []*widget.Button
	min, max, step, wire, addr, port *widget.Entry
}

func saveAll() {
	var session []WinData
	for _, w := range activeWindows {
		session = append(session, WinData{
			Title:       w.win.Title(),
			ActiveVoice: w.activeVoice,
			Data:        []string{w.min.Text, w.max.Text, w.step.Text, w.wire.Text, w.addr.Text, w.port.Text},
		})
	}
	d := dialog.NewFileSave(func(writer fyne.URIWriteCloser, err error) {
		if writer == nil || err != nil { return }
		defer writer.Close()
		json.NewEncoder(writer).Encode(session)
	}, activeWindows[0].win)
	d.SetFileName("session.sk8")
	d.SetFilter(storage.NewExtensionFileFilter([]string{".sk8"}))
	d.Show()
}

func loadSession() {
	if len(activeWindows) == 0 { return }
	d := dialog.NewFileOpen(func(reader fyne.URIReadCloser, err error) {
		if reader == nil || err != nil { return }
		defer reader.Close()
		var session []WinData
		if err := json.NewDecoder(reader).Decode(&session); err != nil { return }
		for _, item := range session {
			inst := &WindowInstance{}
			inst.spawn(item.Data)
			inst.win.SetTitle(item.Title)
			inst.updateVoice(item.ActiveVoice)
		}
	}, activeWindows[0].win)
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
	if w.activeVoice >= 0 {
		msg = fmt.Sprintf("v%d %s", w.activeVoice, msg)
	}
	w.udpMonitor.SetText("TX: " + msg)
	if w.conn != nil { w.conn.Write([]byte(msg)) }
}

func (w *WindowInstance) spawn(initialParams []string) {
	w.win = globalApp.NewWindow("sk8r")
	w.win.SetIcon(fyne.NewStaticResource("icon.png", iconBytes))

	activeWindows = append(activeWindows, w)
	w.activeVoice = -1

	w.win.SetOnClosed(func() {
		for i, v := range activeWindows {
			if v == w {
				activeWindows = append(activeWindows[:i], activeWindows[i+1:]...)
				break
			}
		}
	})

	fileMenu := fyne.NewMenu("File",
		fyne.NewMenuItem("New Window", func() { (&WindowInstance{}).spawn(initialParams) }),
		fyne.NewMenuItemSeparator(),
		fyne.NewMenuItem("Save Session", saveAll),
		fyne.NewMenuItem("Open Session", loadSession),
	)
	viewMenu := fyne.NewMenu("View",
		fyne.NewMenuItem("Normal Size", func() { globalApp.Settings().SetTheme(theme.DefaultTheme()) }),
		fyne.NewMenuItem("Compact Size", func() { globalApp.Settings().SetTheme(compactTheme{theme.DefaultTheme()}) }),
	)
	w.win.SetMainMenu(fyne.NewMainMenu(fileMenu, viewMenu))

	w.titleEntry = widget.NewEntry()
	w.titleEntry.SetPlaceHolder("Note / Title...")
	w.titleEntry.OnSubmitted = func(s string) { w.win.SetTitle(s) }

	cloneBtn := widget.NewButtonWithIcon("", theme.ContentCopyIcon(), func() {
		newInst := &WindowInstance{}
		newInst.spawn([]string{w.min.Text, w.max.Text, w.step.Text, w.wire.Text, w.addr.Text, w.port.Text})
	})

	w.min = widget.NewEntry(); w.min.SetText(initialParams[0])
	w.max = widget.NewEntry(); w.max.SetText(initialParams[1])
	w.step = widget.NewEntry(); w.step.SetText(initialParams[2])
	w.wire = widget.NewEntry(); w.wire.SetText(initialParams[3])
	w.addr = widget.NewEntry(); w.addr.SetText(initialParams[4])
	w.port = widget.NewEntry(); w.port.SetText(initialParams[5])

	w.udpMonitor = widget.NewLabelWithStyle("Ready...", fyne.TextAlignCenter, fyne.TextStyle{Italic: true})

	manualEntry := widget.NewEntry()
	manualEntry.SetPlaceHolder("Manual command...")
	manualSendBtn := widget.NewButtonWithIcon("Send", theme.MailSendIcon(), func() { w.sendUDP(manualEntry.Text) })
	manualEntry.OnSubmitted = func(s string) { w.sendUDP(s) }
	manualBox := container.NewBorder(nil, nil, nil, manualSendBtn, manualEntry)

	minV, _ := strconv.ParseFloat(w.min.Text, 64)
	maxV, _ := strconv.ParseFloat(w.max.Text, 64)
	w.slider = widget.NewSlider(minV, maxV)
	w.slider.Step, _ = strconv.ParseFloat(w.step.Text, 64)
	w.slider.OnChanged = func(val float64) {
		msg := fmt.Sprintf(w.wire.Text, strconv.FormatFloat(val, 'f', 4, 64))
		w.sendUDP(msg)
	}

	applySettings := func() {
		if w.conn != nil { w.conn.Close() }
		c, _ := net.Dial("udp", w.addr.Text+":"+w.port.Text)
		w.conn = c
		newMin, _ := strconv.ParseFloat(w.min.Text, 64)
		newMax, _ := strconv.ParseFloat(w.max.Text, 64)
		w.slider.Min, w.slider.Max = newMin, newMax
		w.slider.Step, _ = strconv.ParseFloat(w.step.Text, 64)
		w.slider.Refresh()
	}
	applySettings()

	w.voiceButtons = make([]*widget.Button, 64)
	grid := container.New(layout.NewGridLayout(16))
	for i := 0; i < 64; i++ {
		id := i
		btn := widget.NewButton(fmt.Sprintf("v%d", id), nil)
		btn.OnTapped = func() {
			if w.activeVoice == id { w.updateVoice(-1) } else { w.updateVoice(id) }
		}
		w.voiceButtons[id] = btn
		grid.Add(btn)
	}

	themeBtn := widget.NewButtonWithIcon("Theme", theme.ColorPaletteIcon(), func() {
		if isDark { globalApp.Settings().SetTheme(theme.LightTheme()); isDark = false
		} else { globalApp.Settings().SetTheme(theme.DarkTheme()); isDark = true }
	})

	configPanel := container.NewVBox(
		container.New(layout.NewFormLayout(),
			widget.NewLabel("Addr/Port"), container.NewGridWithColumns(2, w.addr, w.port),
			widget.NewLabel("Min/Max"), container.NewGridWithColumns(2, w.min, w.max),
			widget.NewLabel("Step/Wire"), container.NewGridWithColumns(2, w.step, w.wire),
		),
		container.NewGridWithColumns(2, widget.NewButtonWithIcon("Apply", theme.ConfirmIcon(), applySettings), themeBtn),
	)
	configPanel.Hide()

	settingsToggle := widget.NewButtonWithIcon("", theme.SettingsIcon(), func() {
		if configPanel.Hidden { configPanel.Show() } else { configPanel.Hide() }
		w.win.Resize(w.win.Content().MinSize())
	})

	topBar := container.NewBorder(nil, nil, cloneBtn, settingsToggle, w.titleEntry)
	bottomArea := container.NewVBox(widget.NewSeparator(), w.udpMonitor)
	
	w.win.SetContent(container.NewBorder(topBar, bottomArea, nil, nil, container.NewVBox(configPanel, grid, manualBox, w.slider)))
	w.win.Resize(w.win.Content().MinSize())
	w.win.Show()
}

func main() {
	globalApp = app.NewWithID("com.sk8r.firecontroller")
	isDark = globalApp.Settings().ThemeVariant() != theme.VariantLight
	
	first := &WindowInstance{}
	defaults := []string{"0", "880", "0.0001", "f%s", "127.0.0.1", "60440"}
	for i := 1; i < len(os.Args) && i <= 4; i++ { defaults[i-1] = os.Args[i] }
	
	first.spawn(defaults)
	globalApp.Run()
}
