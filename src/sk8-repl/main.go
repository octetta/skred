package main

import (
	"os"
	"os/exec"
	"strings"

	"fyne.io/fyne/v2"
	"fyne.io/fyne/v2/app"
	"fyne.io/fyne/v2/container"
	"fyne.io/fyne/v2/theme"
	"fyne.io/fyne/v2/widget"
	"github.com/creack/pty"
	"github.com/fyne-io/terminal"
)

func main() {
	myApp := app.New()
	window := myApp.NewWindow("UDP Netcat Terminal")

	term := terminal.New()
	var currentPty *os.File

	// UI Setup
	exeEntry := widget.NewEntry()
	exeEntry.SetText("nc -u 127.0.0.1 60440")
	
	settings := container.NewVBox(
		widget.NewForm(widget.NewFormItem("Command", exeEntry)),
	)
	settings.Hide()

	// Logic to start the process
	playBtn := widget.NewButtonWithIcon("", theme.MediaPlayIcon(), func() {
		settings.Hide()
		
		parts := strings.Fields(exeEntry.Text)
		if len(parts) == 0 { return }

		// Use the OS to start the command inside a PTY
		cmd := exec.Command(parts[0], parts[1:]...)
		f, err := pty.Start(cmd)
		if err != nil {
			term.Write([]byte("!! Error: " + err.Error() + "\r\n"))
			return
		}
		currentPty = f

		// Connect the terminal to the PTY file
		go term.RunWithConnection(f, f)

		term.Write([]byte(">>> Started: " + exeEntry.Text + "\r\n"))
		
		go func() {
			cmd.Wait()
			term.Write([]byte("\r\n>>> Process Exited\r\n"))
			currentPty = nil
		}()
		
		window.Canvas().Focus(term)
	})

	stopBtn := widget.NewButtonWithIcon("", theme.MediaStopIcon(), func() {
		if currentPty != nil {
			currentPty.Close()
		}
	})

	cogBtn := widget.NewButtonWithIcon("", theme.SettingsIcon(), func() {
		if settings.Hidden { settings.Show() } else { settings.Hide() }
	})

	// Ensure clean exit when closing the window
	window.SetOnClosed(func() {
		if currentPty != nil {
			currentPty.Close()
		}
	})

	top := container.NewHBox(cogBtn, playBtn, stopBtn)
	window.SetContent(container.NewBorder(container.NewVBox(top, settings), nil, nil, nil, term))
	window.Resize(fyne.NewSize(800, 600))
	window.ShowAndRun()
}
