package main

import (
	"bufio"
	"encoding/json"
	"io"
	"os"
	"os/exec"
	"runtime"
	"strings"
	"syscall"
	"time"

	"fyne.io/fyne/v2"
	"fyne.io/fyne/v2/app"
	"fyne.io/fyne/v2/container"
	"fyne.io/fyne/v2/dialog"
	"fyne.io/fyne/v2/layout"
	"fyne.io/fyne/v2/theme"
	"fyne.io/fyne/v2/widget"
)

const configFile = "sk8-repl-cfg.json"

// Config stores persistent app settings
type Config struct {
	Binary  string `json:"binary"`
	WorkDir string `json:"work_dir"`
	Args    string `json:"args"`
	IsDark  bool   `json:"is_dark"`
}

// LogEntry is a custom widget that allows selection/copy but blocks typing
type LogEntry struct {
	widget.Entry
}

func NewLogEntry() *LogEntry {
	e := &LogEntry{}
	e.MultiLine = true
	e.Wrapping = fyne.TextTruncate
	e.TextStyle = fyne.TextStyle{Monospace: true}
	e.ExtendBaseWidget(e)
	return e
}

// Override TypedKey/Rune to create a "selectable read-only" mode
func (e *LogEntry) TypedKey(k *fyne.KeyEvent) { e.Entry.TypedKey(k) }
func (e *LogEntry) TypedRune(r rune)          {}

// HistoryEntry handles Up/Down arrow navigation for command history
type HistoryEntry struct {
	widget.Entry
	history []string
	index   int
}

func NewHistoryEntry() *HistoryEntry {
	e := &HistoryEntry{index: -1}
	e.ExtendBaseWidget(e)
	return e
}

func (e *HistoryEntry) TypedKey(k *fyne.KeyEvent) {
	if k.Name == fyne.KeyUp {
		if len(e.history) > 0 && e.index < len(e.history)-1 {
			e.index++
			e.SetText(e.history[len(e.history)-1-e.index])
		}
		return
	}
	if k.Name == fyne.KeyDown {
		if e.index > 0 {
			e.index--
			e.SetText(e.history[len(e.history)-1-e.index])
		} else {
			e.index = -1
			e.SetText("")
		}
		return
	}
	e.Entry.TypedKey(k)
}

type ShellApp struct {
	window    fyne.Window
	cmdPath   *widget.Entry
	workDir   *widget.Entry
	argsEntry *widget.Entry
	outputLog *LogEntry
	input     *HistoryEntry
	process   *exec.Cmd
	stdin     io.WriteCloser
	app       fyne.App
}

func (s *ShellApp) saveConfig() {
	isDark := s.app.Settings().ThemeVariant() == theme.VariantDark
	cfg := Config{
		Binary:  s.cmdPath.Text,
		WorkDir: s.workDir.Text,
		Args:    s.argsEntry.Text,
		IsDark:  isDark,
	}
	data, _ := json.MarshalIndent(cfg, "", "  ")
	_ = os.WriteFile(configFile, data, 0644)
}

func (s *ShellApp) loadConfig() {
	data, err := os.ReadFile(configFile)
	if err != nil { return }
	var cfg Config
	if err := json.Unmarshal(data, &cfg); err == nil {
		s.cmdPath.SetText(cfg.Binary)
		s.workDir.SetText(cfg.WorkDir)
		s.argsEntry.SetText(cfg.Args)
		if cfg.IsDark {
			s.app.Settings().SetTheme(theme.DarkTheme())
		} else {
			s.app.Settings().SetTheme(theme.LightTheme())
		}
	}
}

func (s *ShellApp) appendLog(text string, isInput bool) {
	ts := time.Now().Format("15:04:05")
	prefix := "[" + ts + "] "
	if isInput { prefix += "> " }
	s.outputLog.SetText(s.outputLog.Text + prefix + text + "\n")
	s.outputLog.CursorRow = len(strings.Split(s.outputLog.Text, "\n"))
	s.outputLog.Refresh()
}

func (s *ShellApp) runCommand() {
	// Kill existing process if running
	if s.process != nil && s.process.Process != nil {
		_ = s.process.Process.Kill()
	}
	s.saveConfig()

	args := strings.Fields(s.argsEntry.Text)
	s.process = exec.Command(s.cmdPath.Text, args...)
	
	// Apply Working Directory
	if s.workDir.Text != "" {
		s.process.Dir = s.workDir.Text
	}

	// Windows Specific: Hide the black console window
	if runtime.GOOS == "windows" {
		s.process.SysProcAttr = &syscall.SysProcAttr{
			HideWindow:    true,
			CreationFlags: 0x08000000, // CREATE_NO_WINDOW
		}
	}

	var err error
	s.stdin, err = s.process.StdinPipe()
	stdout, _ := s.process.StdoutPipe()
	stderr, _ := s.process.StderrPipe()

	if err != nil {
		s.appendLog("Failed to open Stdin: "+err.Error(), false)
		return
	}

	// Read output in a background goroutine
	go func() {
		reader := io.MultiReader(stdout, stderr)
		scanner := bufio.NewScanner(reader)
		for scanner.Scan() {
			line := scanner.Text()
			fyne.Do(func() { s.appendLog(line, false) })
		}
	}()

	if err := s.process.Start(); err != nil {
		s.appendLog("Error starting: "+err.Error(), false)
		return
	}
	s.appendLog("Started: "+s.cmdPath.Text, false)
}

func main() {
	myApp := app.NewWithID("com.sk8r.shell.repl")
	w := myApp.NewWindow("SK8-SHELL")

	s := &ShellApp{
		app:       myApp,
		window:    w,
		cmdPath:   widget.NewEntry(),
		workDir:   widget.NewEntry(),
		argsEntry: widget.NewEntry(),
		outputLog: NewLogEntry(),
		input:     NewHistoryEntry(),
	}

	// Initialize with Current Working Directory
	pwd, _ := os.Getwd()
	s.workDir.SetText(pwd)

	// File/Folder Pickers
	pickFileBtn := widget.NewButtonWithIcon("", theme.FolderOpenIcon(), func() {
		dialog.ShowFileOpen(func(reader fyne.URIReadCloser, err error) {
			if reader != nil { s.cmdPath.SetText(reader.URI().Path()) }
		}, w)
	})

	pickDirBtn := widget.NewButtonWithIcon("", theme.FolderIcon(), func() {
		dialog.ShowFolderOpen(func(list fyne.ListableURI, err error) {
			if list != nil { s.workDir.SetText(list.Path()) }
		}, w)
	})

	// Form & Settings UI
	configFields := widget.NewForm(
		widget.NewFormItem("Executable", container.NewBorder(nil, nil, nil, pickFileBtn, s.cmdPath)),
		widget.NewFormItem("Work Dir", container.NewBorder(nil, nil, nil, pickDirBtn, s.workDir)),
		widget.NewFormItem("Args", s.argsEntry),
	)

	themeBtn := widget.NewButtonWithIcon("Theme", theme.ColorPaletteIcon(), func() {
		if s.app.Settings().ThemeVariant() == theme.VariantDark {
			s.app.Settings().SetTheme(theme.LightTheme())
		} else {
			s.app.Settings().SetTheme(theme.DarkTheme())
		}
		s.saveConfig()
	})

	clearBtn := widget.NewButtonWithIcon("Clear", theme.DeleteIcon(), func() { s.outputLog.SetText("") })

	configContainer := container.NewVBox(
		configFields, 
		container.NewHBox(themeBtn, clearBtn), 
		widget.NewSeparator(),
	)
	configContainer.Hide()

	configToggle := widget.NewButtonWithIcon("Settings", theme.SettingsIcon(), func() {
		if configContainer.Hidden { configContainer.Show() } else { configContainer.Hide() }
	})

	startBtn := widget.NewButtonWithIcon("Launch", theme.MediaPlayIcon(), s.runCommand)
	stopBtn := widget.NewButtonWithIcon("Stop", theme.MediaStopIcon(), func() {
		if s.process != nil && s.process.Process != nil {
			_ = s.process.Process.Kill()
			s.appendLog("Process terminated by user.", false)
		}
	})

	// Layout Assembly
	w.SetContent(container.NewBorder(
		container.NewVBox(container.NewHBox(configToggle, startBtn, stopBtn, layout.NewSpacer()), configContainer),
		s.input, nil, nil,
		s.outputLog,
	))

	// Input Submission
	s.input.OnSubmitted = func(val string) {
		if val == "" || s.stdin == nil { return }
		s.appendLog(val, true)
		_, _ = io.WriteString(s.stdin, val+"\n")
		s.input.history = append(s.input.history, val)
		s.input.index = -1
		s.input.SetText("")
	}

	s.loadConfig()
	w.Resize(fyne.NewSize(800, 600))
	w.ShowAndRun()
}
