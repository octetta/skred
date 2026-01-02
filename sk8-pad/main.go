package main

import (
	"encoding/json"
	"net"
	"os"
	"strconv"
	"time"

	"fyne.io/fyne/v2"
	"fyne.io/fyne/v2/app"
	"fyne.io/fyne/v2/container"
	"fyne.io/fyne/v2/driver/desktop"
	"fyne.io/fyne/v2/theme"
	"fyne.io/fyne/v2/widget"
)

type InstantButton struct {
	widget.Button
	OnPress func()
}

func NewInstantButton(label string, onPress func()) *InstantButton {
	b := &InstantButton{OnPress: onPress}
	b.Text = label
	b.Importance = widget.HighImportance
	b.ExtendBaseWidget(b)
	return b
}

func (b *InstantButton) MouseDown(e *desktop.MouseEvent) {
	if b.OnPress != nil { b.OnPress() }
	fyne.Do(func() {
		orig := b.Importance
		b.Importance = widget.SuccessImportance
		b.Refresh()
		go func() {
			time.Sleep(time.Millisecond * 100)
			fyne.Do(func() {
				b.Importance = orig
				b.Refresh()
			})
		}()
	})
}

func (b *InstantButton) MouseUp(e *desktop.MouseEvent) {}

type Config struct {
	Addr     string   `json:"addr"`
	Port     string   `json:"port"`
	Commands []string `json:"commands"`
}

type PadApp struct {
	addrEntry *widget.Entry
	portEntry *widget.Entry
	entries   []*widget.Entry
}

func (p *PadApp) saveConfig() {
	cfg := Config{Addr: p.addrEntry.Text, Port: p.portEntry.Text}
	for _, e := range p.entries { cfg.Commands = append(cfg.Commands, e.Text) }
	data, _ := json.Marshal(cfg)
	_ = os.WriteFile("sk8-pad-cfg.json", data, 0644)
}

func (p *PadApp) loadConfig() {
	data, err := os.ReadFile("sk8-pad-cfg.json")
	if err != nil { return }
	var cfg Config
	if err := json.Unmarshal(data, &cfg); err == nil {
		p.addrEntry.SetText(cfg.Addr)
		p.portEntry.SetText(cfg.Port)
		for i, val := range cfg.Commands {
			if i < len(p.entries) { p.entries[i].SetText(val) }
		}
	}
}

func (p *PadApp) sendUDP(msg string) {
	if msg == "" { return }
	addr := p.addrEntry.Text + ":" + p.portEntry.Text
	conn, err := net.Dial("udp", addr)
	if err != nil { return }
	defer conn.Close()
	_, _ = conn.Write([]byte(msg))
}

func main() {
	a := app.NewWithID("com.sk8r.pad")
	w := a.NewWindow("SK8-PAD")

	p := &PadApp{
		addrEntry: widget.NewEntry(),
		portEntry: widget.NewEntry(),
	}
	p.addrEntry.SetText("127.0.0.1")
	p.portEntry.SetText("60440")

	configForm := widget.NewForm(
		widget.NewFormItem("UDP Address", p.addrEntry),
		widget.NewFormItem("UDP Port", p.portEntry),
	)
	configForm.Hide()

	settingsToggle := widget.NewButtonWithIcon("Config", theme.SettingsIcon(), func() {
		if configForm.Hidden { configForm.Show() } else { configForm.Hide(); p.saveConfig() }
	})

	grid := container.NewGridWithColumns(4)

	for i := 1; i <= 16; i++ {
		cmdEntry := widget.NewEntry()
		cmdEntry.SetPlaceHolder("Msg...")
		p.entries = append(p.entries, cmdEntry)

		btn := NewInstantButton(strconv.Itoa(i), func() {
			p.sendUDP(cmdEntry.Text)
		})

		// Border fills the grid cell. btn takes the center (expanding space)
		cell := container.NewBorder(nil, cmdEntry, nil, nil, btn)
		grid.Add(cell)
	}

	p.loadConfig()

	header := container.NewBorder(nil, nil, nil, settingsToggle, 
		widget.NewLabelWithStyle("SK8-PAD", fyne.TextAlignLeading, fyne.TextStyle{Bold: true}))
	
	content := container.NewBorder(
		container.NewVBox(header, configForm), 
		nil, nil, nil, 
		container.NewPadded(grid),
	)

	w.SetContent(content)
	
	// Set a minimum size so the layout doesn't collapse
	w.SetFixedSize(false) 
	w.Resize(fyne.NewSize(600, 600))
	
	w.ShowAndRun()
}
