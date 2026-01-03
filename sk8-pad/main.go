package main

import (
	"encoding/json"
	"fmt"
	"net"
	"os"
	"strconv"
	"sync"
	"time"

	"fyne.io/fyne/v2"
	"fyne.io/fyne/v2/app"
	"fyne.io/fyne/v2/container"
	"fyne.io/fyne/v2/layout"
	"fyne.io/fyne/v2/theme"
	"fyne.io/fyne/v2/widget"
)

const configFile = "sk8-pad-cfg.json"

type Config struct {
	Addr string   `json:"addr"`
	Port string   `json:"port"`
	Cmds []string `json:"cmds"`
	Dark bool     `json:"dark"`
	Keys bool     `json:"keys"`
}

var (
	conn     net.Conn
	connLock sync.Mutex
)

func reconnect(addrStr string) {
	connLock.Lock()
	defer connLock.Unlock()
	if conn != nil {
		conn.Close()
	}
	c, err := net.Dial("udp", addrStr)
	if err == nil {
		conn = c
	}
}

type HistoryEntry struct {
	widget.Entry
	history  []string
	pos      int
	tempText string
}

func NewHistoryEntry() *HistoryEntry {
	e := &HistoryEntry{history: []string{}, pos: -1}
	e.ExtendBaseWidget(e)
	return e
}

func (e *HistoryEntry) TypedKey(k *fyne.KeyEvent) {
	if len(e.history) == 0 {
		e.Entry.TypedKey(k)
		return
	}
	switch k.Name {
	case fyne.KeyUp:
		if e.pos == -1 { e.tempText = e.Text }
		if e.pos < len(e.history)-1 {
			e.pos++
			e.SetText(e.history[len(e.history)-1-e.pos])
		}
	case fyne.KeyDown:
		if e.pos > 0 {
			e.pos--
			e.SetText(e.history[len(e.history)-1-e.pos])
		} else if e.pos == 0 {
			e.pos = -1
			e.SetText(e.tempText)
		}
	default:
		e.Entry.TypedKey(k)
	}
}

func main() {
	myApp := app.NewWithID("com.sk8r.pad")
	w := myApp.NewWindow("SK8-PAD")

	var shortcutsOn bool
	var fieldsVisible = true
	entries := make([]*widget.Entry, 16)
	buttons := make([]*widget.Button, 16)

	addrEntry := widget.NewEntry()
	addrEntry.SetText("127.0.0.1")
	portEntry := widget.NewEntry()
	portEntry.SetText("60440")

	clearFocus := func() { w.Canvas().Focus(nil) }

	sendUDP := func(msg string) {
		if msg == "" { return }
		connLock.Lock()
		defer connLock.Unlock()
		if conn != nil {
			_, _ = conn.Write([]byte(msg))
		}
	}

	keyMapping := []string{"1", "2", "3", "4", "5", "6", "7", "8", "A", "S", "D", "F", "H", "J", "K", "L"}

	refreshButtonLabels := func() {
		for i := 0; i < 16; i++ {
			if shortcutsOn {
				buttons[i].SetText(fmt.Sprintf("%d [%s]", i+1, keyMapping[i]))
			} else {
				buttons[i].SetText(strconv.Itoa(i + 1))
			}
		}
	}

	grid := container.New(layout.NewGridLayout(4))
	for i := 0; i < 16; i++ {
		idx := i
		e := widget.NewEntry()
		e.SetPlaceHolder("Msg " + strconv.Itoa(idx+1))
		e.OnSubmitted = func(_ string) { clearFocus() }
		entries[idx] = e

		b := widget.NewButton(strconv.Itoa(idx+1), nil)
		b.Importance = widget.HighImportance
		b.OnTapped = func() {
			go sendUDP(e.Text)
			b.Importance = widget.SuccessImportance
			b.Refresh()
			go func() {
				time.Sleep(time.Millisecond * 100)
				b.Importance = widget.HighImportance
				b.Refresh()
			}()
		}
		buttons[idx] = b
		grid.Add(container.NewBorder(nil, e, nil, nil, b))
	}

	adhocEntry := NewHistoryEntry()
	adhocEntry.SetPlaceHolder("Ad-hoc (Enter to send, Up/Down for history)...")
	adhocSend := func() {
		txt := adhocEntry.Text
		if txt == "" { return }
		sendUDP(txt)
		if len(adhocEntry.history) == 0 || adhocEntry.history[len(adhocEntry.history)-1] != txt {
			adhocEntry.history = append(adhocEntry.history, txt)
		}
		adhocEntry.SetText("")
		adhocEntry.pos = -1
		clearFocus()
	}
	adhocEntry.OnSubmitted = func(_ string) { adhocSend() }
	adhocBtn := widget.NewButtonWithIcon("Send", theme.MailSendIcon(), adhocSend)
	adhocBar := container.NewBorder(nil, nil, nil, adhocBtn, adhocEntry)

	saveLabel := widget.NewLabel("")
	saveLabel.Hide()

	save := func() {
		cfg := Config{
			Addr: addrEntry.Text, Port: portEntry.Text,
			Dark: myApp.Settings().ThemeVariant() == theme.VariantDark,
			Keys: shortcutsOn,
		}
		for _, e := range entries { cfg.Cmds = append(cfg.Cmds, e.Text) }
		d, _ := json.Marshal(cfg)
		_ = os.WriteFile(configFile, d, 0644)
		reconnect(addrEntry.Text + ":" + portEntry.Text)
		saveLabel.SetText("Saved!")
		saveLabel.Show()
		go func() { time.Sleep(time.Second); saveLabel.Hide() }()
	}

	saveBtn := widget.NewButtonWithIcon("", theme.DocumentSaveIcon(), func() { save(); clearFocus() })
	
	modeBtn := widget.NewButton("Keys: OFF", nil)
	modeBtn.OnTapped = func() {
		shortcutsOn = !shortcutsOn
		if shortcutsOn {
			clearFocus()
			modeBtn.SetText("Keys: ON")
			modeBtn.Importance = widget.SuccessImportance
		} else {
			modeBtn.SetText("Keys: OFF")
			modeBtn.Importance = widget.MediumImportance
		}
		refreshButtonLabels()
		modeBtn.Refresh()
	}

	viewBtn := widget.NewButton("Text: ON", nil)
	viewBtn.Importance = widget.HighImportance 
	viewBtn.OnTapped = func() {
		fieldsVisible = !fieldsVisible
		if fieldsVisible {
			viewBtn.SetText("Text: ON")
			viewBtn.Importance = widget.HighImportance
			for _, e := range entries { e.Show() }
			adhocBar.Show()
		} else {
			viewBtn.SetText("Text: OFF")
			viewBtn.Importance = widget.WarningImportance 
			for _, e := range entries { e.Hide() }
			adhocBar.Hide()
		}
		viewBtn.Refresh()
	}

	w.Canvas().SetOnTypedKey(func(k *fyne.KeyEvent) {
		if k.Name == fyne.KeyEscape { clearFocus(); return }
		if !shortcutsOn || w.Canvas().Focused() != nil { return }
		for idx, keyName := range keyMapping {
			if string(k.Name) == keyName {
				if buttons[idx].OnTapped != nil { buttons[idx].OnTapped() }
				break
			}
		}
	})

	configToggle := widget.NewButtonWithIcon("", theme.SettingsIcon(), nil)
	applyBtn := widget.NewButtonWithIcon("Apply & Close", theme.ConfirmIcon(), nil)
	configFields := widget.NewForm(widget.NewFormItem("IP", addrEntry), widget.NewFormItem("Port", portEntry))
	configContainer := container.NewVBox(configFields, applyBtn, widget.NewSeparator())
	configContainer.Hide()

	applyBtn.OnTapped = func() { save(); clearFocus(); configContainer.Hide() }
	configToggle.OnTapped = func() {
		if configContainer.Hidden { configContainer.Show() } else { configContainer.Hide(); save(); clearFocus() }
	}

	if d, err := os.ReadFile(configFile); err == nil {
		var c Config
		if json.Unmarshal(d, &c) == nil {
			if c.Addr != "" { addrEntry.SetText(c.Addr) }
			if c.Port != "" { portEntry.SetText(c.Port) }
			shortcutsOn = c.Keys
			for i, v := range c.Cmds { if i < 16 { entries[i].SetText(v) } }
			if shortcutsOn { 
				modeBtn.SetText("Keys: ON"); modeBtn.Importance = widget.SuccessImportance 
				refreshButtonLabels()
			}
		}
	}

	reconnect(addrEntry.Text + ":" + portEntry.Text)

	header := container.NewVBox(
		container.NewHBox(
			saveBtn, 
			widget.NewLabelWithStyle("SK8-PAD", fyne.TextAlignLeading, fyne.TextStyle{Bold: true}), 
			saveLabel, 
			layout.NewSpacer(), 
			modeBtn, 
			viewBtn, 
			configToggle,
		),
		configContainer,
	)

	w.SetContent(container.NewBorder(
		header, 
		container.NewPadded(adhocBar), 
		nil, 
		nil, 
		container.NewPadded(grid),
	))

	w.Resize(fyne.NewSize(620, 700))
	w.ShowAndRun()
}
