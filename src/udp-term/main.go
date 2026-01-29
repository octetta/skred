package main

import (
	"encoding/hex"
	"encoding/json"
	"flag"
	"fmt"
	"net"
	"os"
	"os/exec"
	"path/filepath"
	"runtime"
	"strings"
	"sync"

	_ "image/jpeg"
	_ "image/png"

	"fyne.io/fyne/v2"
	"fyne.io/fyne/v2/app"
	"fyne.io/fyne/v2/container"
	"fyne.io/fyne/v2/driver/desktop"
	"fyne.io/fyne/v2/widget"
	"github.com/chzyer/readline"
	"github.com/gofrs/flock"
)

type Task struct {
	Name    string   `json:"name"`
	Exec    string   `json:"exec"`
	Args    []string `json:"args"`
	Mode    string   `json:"mode"`
	WorkDir string   `json:"work_dir"`
}

type Config struct {
	TargetAddr    string   `json:"target_addr"`
	RecentTargets []string `json:"recent_targets"`
	Prompt        string   `json:"prompt"`
	HexMode       bool     `json:"hex_mode"`
	ExternalTools []Task   `json:"external_tools"`
}

var (
	appConfig  Config
	configMu   sync.RWMutex
	hexMode    bool
	udpConn    *net.UDPConn
	connMu     sync.Mutex
	targetAddr string
	fileLock   *flock.Flock
	globalApp  fyne.App
)

func getDefaultConfig() Config {
	return Config{
		TargetAddr:    "127.0.0.1:60440",
		RecentTargets: []string{"127.0.0.1:60440"},
		Prompt:        "\033[32mUDP-Shell>\033[0m ",
		HexMode:       false,
		ExternalTools: []Task{
			{Name: "Synth Engine", Exec: "synth", Args: []string{"-v"}, Mode: "console"},
		},
	}
}

func main() {
	isCLI := flag.Bool("cli", false, "internal flag for console mode")
	isDaemon := flag.Bool("daemon", false, "internal flag for detached mode")
	flag.Parse()

	if *isCLI {
		// Enable ANSI/VT support for Windows cmd.exe
		setupTerminal()
		loadConfig()
		runUDPShell()
		return
	}

	if runtime.GOOS == "windows" {
		hideSelf()
	}

	lockPath := filepath.Join(filepath.Dir(getConfigPath()), "udp-term.lock")
	fileLock = flock.New(lockPath)
	locked, _ := fileLock.TryLock()
	if !locked {
		os.Exit(0)
	}
	defer fileLock.Unlock()

	if runtime.GOOS != "windows" && !*isDaemon {
		self, _ := os.Executable()
		exec.Command(self, "--daemon").Start()
		os.Exit(0)
	}

	loadConfig()
	globalApp = app.NewWithID("com.udp.terminal.tool")
	globalApp.SetIcon(resourceIconPng)

	setupTray(globalApp)
	globalApp.Run()
}

func setupTray(a fyne.App) {
	desk, ok := a.(desktop.App)
	if !ok { return }

	configMu.RLock()
	tools := appConfig.ExternalTools
	recent := appConfig.RecentTargets
	configMu.RUnlock()

	menuItems := []*fyne.MenuItem{
		fyne.NewMenuItem("Open Shell", func() { launchSelfInTerminal() }),
	}

	if len(recent) > 0 {
		var subItems []*fyne.MenuItem
		for _, r := range recent {
			addr := r
			subItems = append(subItems, fyne.NewMenuItem(addr, func() {
				connectUDP(addr)
			}))
		}
		recentMenu := fyne.NewMenuItem("Recent Targets", nil)
		recentMenu.ChildMenu = fyne.NewMenu("", subItems...)
		menuItems = append(menuItems, recentMenu)
	}

	if len(tools) > 0 {
		menuItems = append(menuItems, fyne.NewMenuItemSeparator())
		for _, t := range tools {
			task := t
			menuItems = append(menuItems, fyne.NewMenuItem(task.Name, func() {
				runTask(task)
			}))
		}
	}

	menuItems = append(menuItems, fyne.NewMenuItemSeparator())
	menuItems = append(menuItems, fyne.NewMenuItem("Reload Config", func() {
		loadConfig()
		setupTray(a)
	}))
	menuItems = append(menuItems, fyne.NewMenuItem("About", func() { showAbout(a) }))
	menuItems = append(menuItems, fyne.NewMenuItemSeparator())
	menuItems = append(menuItems, fyne.NewMenuItem("Quit", func() { a.Quit() }))

	desk.SetSystemTrayMenu(fyne.NewMenu("UDP Tool", menuItems...))
}

func runTask(t Task) {
	binDir, _ := os.Executable()
	basePath := filepath.Dir(binDir)
	execPath := t.Exec
	if !filepath.IsAbs(execPath) {
		execPath = filepath.Join(basePath, t.Exec)
	}
	wd := t.WorkDir
	if wd == "" || !filepath.IsAbs(wd) {
		wd = filepath.Join(basePath, wd)
	}

	var cmd *exec.Cmd
	mode := strings.ToLower(t.Mode)

	if runtime.GOOS == "windows" {
		if mode == "console" {
			fullArgs := strings.Join(t.Args, " ")
			cmd = exec.Command("cmd.exe", "/c", "start", "", execPath, fullArgs)
			setConsoleAttrs(cmd)
		} else {
			cmd = exec.Command(execPath, t.Args...)
			if mode == "background" {
				setPlatformSpecificAttrs(cmd)
			}
		}
	} else {
		if mode == "console" {
			term := findTerminal()
			termArgs := append([]string{"--", execPath}, t.Args...)
			cmd = exec.Command(term, termArgs...)
		} else {
			cmd = exec.Command(execPath, t.Args...)
		}
	}

	if cmd != nil {
		cmd.Dir = wd
		_ = cmd.Start()
	}
}

func findTerminal() string {
	terms := []string{"ptyxis", "gnome-terminal", "gnome-console", "konsole", "xfce4-terminal", "xterm"}
	for _, t := range terms {
		if _, err := exec.LookPath(t); err == nil { return t }
	}
	return "sh"
}

func getConfigPath() string {
	dir, _ := os.UserConfigDir()
	path := filepath.Join(dir, "udp-term")
	os.MkdirAll(path, 0755)
	return filepath.Join(path, "config.json")
}

func getHistoryPath() string {
	home, _ := os.UserHomeDir()
	return filepath.Join(home, ".udp_shell_history")
}

func openConfigFolder() {
	path := filepath.Dir(getConfigPath())
	var cmd *exec.Cmd
	switch runtime.GOOS {
	case "windows": cmd = exec.Command("explorer", path)
	case "darwin": cmd = exec.Command("open", path)
	default: cmd = exec.Command("xdg-open", path)
	}
	if cmd != nil { _ = cmd.Start() }
}

func loadConfig() {
	configMu.Lock()
	defer configMu.Unlock()
	cPath := getConfigPath()
	if _, err := os.Stat(cPath); os.IsNotExist(err) {
		appConfig = getDefaultConfig()
		data, _ := json.MarshalIndent(appConfig, "", "  ")
		os.WriteFile(cPath, data, 0644)
	} else {
		data, err := os.ReadFile(cPath)
		if err == nil { json.Unmarshal(data, &appConfig) }
	}
	targetAddr = appConfig.TargetAddr
	hexMode = appConfig.HexMode
}

func saveConfig() {
	configMu.Lock()
	appConfig.TargetAddr = targetAddr
	appConfig.HexMode = hexMode
	newRecent := []string{targetAddr}
	for _, r := range appConfig.RecentTargets {
		if r != targetAddr { newRecent = append(newRecent, r) }
	}
	if len(newRecent) > 5 { newRecent = newRecent[:5] }
	appConfig.RecentTargets = newRecent
	data, _ := json.MarshalIndent(appConfig, "", "  ")
	os.WriteFile(getConfigPath(), data, 0644)
	configMu.Unlock()
	if globalApp != nil { setupTray(globalApp) }
}

func launchSelfInTerminal() {
	self, _ := os.Executable()
	runTask(Task{Exec: self, Args: []string{"--cli"}, Mode: "console"})
}

func connectUDP(addrStr string) error {
	connMu.Lock()
	defer connMu.Unlock()
	if udpConn != nil { udpConn.Close() }
	addr, err := net.ResolveUDPAddr("udp", addrStr)
	if err != nil { return err }
	conn, err := net.DialUDP("udp", nil, addr)
	if err != nil { return err }
	udpConn = conn
	targetAddr = addrStr
	saveConfig()
	return nil
}

func runUDPShell() {
	configMu.RLock()
	currentPrompt := appConfig.Prompt
	configMu.RUnlock()
	rl, err := readline.NewEx(&readline.Config{
		Prompt:      currentPrompt,
		HistoryFile: getHistoryPath(),
	})
	if err != nil { return }
	defer rl.Close()
	connectUDP(targetAddr)
	go func() {
		buf := make([]byte, 8192)
		for {
			connMu.Lock()
			currConn := udpConn
			connMu.Unlock()
			if currConn == nil { continue }
			n, _, err := currConn.ReadFromUDP(buf)
			if err != nil { continue }
			var output string
			if hexMode {
				output = fmt.Sprintf("\033[36m[HEX]: %s\033[0m", hex.EncodeToString(buf[:n]))
			} else {
				output = strings.TrimRight(string(buf[:n]), "\r\n")
			}
			fmt.Printf("\r\033[K%s\n", output)
			rl.Refresh()
		}
	}()
	fmt.Printf("Shell Active. Target: %s\n", targetAddr)
	for {
		line, err := rl.Readline()
		if err != nil { break }
		line = strings.TrimSpace(line)
		if line == "" { continue }
		if strings.HasPrefix(line, "^") {
			fmt.Printf("\033[33m%s\033[0m\n", myLocalFunction(strings.TrimPrefix(line, "^"), rl))
			continue
		}
		connMu.Lock()
		if udpConn != nil {
			udpConn.Write([]byte(line + "\n"))
		}
		connMu.Unlock()
	}
}

func myLocalFunction(input string, rl *readline.Instance) string {
	parts := strings.Fields(input)
	if len(parts) == 0 { return "" }
	cmd := strings.ToLower(parts[0])
	switch cmd {
	case "ip":
		addrs, _ := net.InterfaceAddrs()
		var res []string
		for _, a := range addrs {
			if ipnet, ok := a.(*net.IPNet); ok && !ipnet.IP.IsLoopback() && ipnet.IP.To4() != nil {
				res = append(res, ipnet.IP.String())
			}
		}
		return "Local IPs: " + strings.Join(res, ", ")
	case "target":
		if len(parts) < 2 { return "Usage: ^target <ip>:<port>" }
		if err := connectUDP(parts[1]); err != nil { return err.Error() }
		return "Target set to " + targetAddr
	case "hex":
		hexMode = !hexMode
		saveConfig()
		status := "OFF"; if hexMode { status = "ON" }
		return "Hex view is now " + status
	case "prompt":
		newPrompt := strings.Join(parts[1:], " ")
		p := fmt.Sprintf("\033[35m%s\033[0m", strings.Trim(newPrompt, "\"'"))
		rl.SetPrompt(p)
		configMu.Lock()
		appConfig.Prompt = p
		configMu.Unlock()
		saveConfig()
		return "Prompt updated"
	case "cls":
		fmt.Print("\033[H\033[2J"); return ""
	case "version":
		return "UDP-Term v1.8.17"
	case "help":
		return "Commands: ^ip, ^target, ^prompt, ^hex, ^cls, ^version, ^exit"
	case "exit":
		os.Exit(0)
	}
	return "Unknown command: " + input
}

func showAbout(a fyne.App) {
	w := a.NewWindow("About UDP-Term")
	w.SetContent(container.NewVBox(
		widget.NewLabelWithStyle("UDP-Term v1.8.17", fyne.TextAlignCenter, fyne.TextStyle{Bold: true}),
		widget.NewLabel("Target: "+targetAddr),
		widget.NewLabel("History: "+getHistoryPath()),
		widget.NewButton("Open Config Folder", func() { openConfigFolder() }),
		widget.NewButton("Close", func() { w.Close() }),
	))
	w.Show()
}
