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

type Config struct {
	TargetAddr string `json:"target_addr"`
	Prompt     string `json:"prompt"`
	HexMode    bool   `json:"hex_mode"`
}

var (
	appConfig  Config
	hexMode    = false
	udpConn    *net.UDPConn
	connMu     sync.Mutex
	targetAddr string = "127.0.0.1:60440"
	fileLock   *flock.Flock
)

func main() {
	isCLI := flag.Bool("cli", false, "internal flag for console mode")
	isDaemon := flag.Bool("daemon", false, "internal flag for detached mode")
	flag.Parse()

	if *isCLI {
		loadConfig()
		runUDPShell()
		return
	}

	// Single Instance Lock
	lockPath := filepath.Join(filepath.Dir(getConfigPath()), "udp-term.lock")
	fileLock = flock.New(lockPath)
	locked, err := fileLock.TryLock()
	if err != nil || !locked {
		fmt.Println("UDP-Term is already running.")
		os.Exit(0)
	}
	defer fileLock.Unlock()

	if runtime.GOOS != "windows" && !*isDaemon {
		self, _ := os.Executable()
		cmd := exec.Command(self, "--daemon")
		cmd.Start()
		os.Exit(0)
	}

	loadConfig()
	a := app.NewWithID("com.udp.terminal.tool")
	a.SetIcon(resourceIconPng)

	if desk, ok := a.(desktop.App); ok {
		menu := fyne.NewMenu("UDP Tool",
			fyne.NewMenuItem("Open Shell", func() { launchSelfInTerminal() }),
			fyne.NewMenuItem("About", func() { showAbout(a) }),
			fyne.NewMenuItemSeparator(),
			fyne.NewMenuItem("Quit", func() { a.Quit() }),
		)
		desk.SetSystemTrayMenu(menu)
	}
	a.Run()
}

func showAbout(a fyne.App) {
	w := a.NewWindow("About UDP-Term")
	info := fmt.Sprintf("Version: 1.6.3\nOS: %s\nConfig: %s\nTarget: %s", 
		runtime.GOOS, getConfigPath(), targetAddr)
	
	content := container.NewVBox(
		widget.NewLabelWithStyle("UDP-Term", fyne.TextAlignCenter, fyne.TextStyle{Bold: true}),
		widget.NewLabel(info),
		widget.NewButton("Close", func() { w.Close() }),
	)
	w.SetContent(content)
	w.Show()
}

func getConfigPath() string {
	dir, _ := os.UserConfigDir()
	path := filepath.Join(dir, "udp-term")
	os.MkdirAll(path, 0755)
	return filepath.Join(path, "config.json")
}

func loadConfig() {
	appConfig = Config{TargetAddr: "127.0.0.1:60440", Prompt: "\033[32mUDP-Shell>\033[0m ", HexMode: false}
	data, err := os.ReadFile(getConfigPath())
	if err == nil {
		json.Unmarshal(data, &appConfig)
	}
	targetAddr = appConfig.TargetAddr
	hexMode = appConfig.HexMode
}

func saveConfig() {
	appConfig.TargetAddr = targetAddr
	appConfig.HexMode = hexMode
	data, _ := json.MarshalIndent(appConfig, "", "  ")
	os.WriteFile(getConfigPath(), data, 0644)
}

func launchSelfInTerminal() {
	self, _ := os.Executable()
	var cmd *exec.Cmd
	switch runtime.GOOS {
	case "windows":
		cmd = exec.Command("cmd.exe", "/c", "start", self, "--cli")
	case "linux":
		terminals := []string{"ptyxis", "gnome-terminal", "gnome-console", "konsole", "xterm"}
		for _, term := range terminals {
			if _, err := exec.LookPath(term); err == nil {
				cmd = exec.Command(term, "--", self, "--cli")
				break
			}
		}
	}
	if cmd != nil { _ = cmd.Start() }
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
	home, _ := os.UserHomeDir()
	rl, err := readline.NewEx(&readline.Config{
		Prompt:      appConfig.Prompt,
		HistoryFile: filepath.Join(home, ".udp_shell_history"),
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
			os.Stdout.Sync()
		}
	}()

	fmt.Printf("Shell Active. Target: %s\n", targetAddr)
	for {
		line, err := rl.Readline()
		if err != nil { break }
		line = strings.TrimSpace(line)
		if line == "" { continue }

		if strings.HasPrefix(line, "^") {
			cmdBody := strings.TrimPrefix(line, "^")
			response := myLocalFunction(cmdBody, rl)
			if response != "" { fmt.Printf("\033[33m%s\033[0m\n", response) }
			continue
		}

		connMu.Lock()
		if udpConn != nil { udpConn.Write([]byte(line + "\n")) }
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
		status := "OFF"
		if hexMode { status = "ON" }
		return "Hex view is now " + status
	case "prompt":
		newPrompt := strings.Join(parts[1:], " ")
		newPrompt = strings.Trim(newPrompt, "\"'")
		if newPrompt == "" { return "Usage: ^prompt \"text\"" }
		p := fmt.Sprintf("\033[35m%s\033[0m", newPrompt)
		rl.SetPrompt(p)
		appConfig.Prompt = p
		saveConfig()
		return "Prompt updated"
	case "cls":
		fmt.Print("\033[H\033[2J")
		return ""
	case "help":
		return "Local Commands:\n" +
			"  ^ip            - Show local machine IP addresses\n" +
			"  ^target <addr> - Change UDP destination\n" +
			"  ^prompt \"text\" - Change prompt\n" +
			"  ^hex           - Toggle hex-dump\n" +
			"  ^cls           - Clear screen\n" +
			"  ^version       - Show version info\n" +
			"  ^exit          - Exit shell"
	case "version":
		return "UDP-Term v1.6.3 (Full Feature Restoration)"
	case "exit":
		os.Exit(0)
	}
	return "Unknown command: " + input
}
