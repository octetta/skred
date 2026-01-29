package main

import (
	"encoding/hex"
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
	"fyne.io/fyne/v2/driver/desktop"
	"github.com/chzyer/readline"
)

var (
	hexMode   = false
	udpConn   *net.UDPConn
	connMu    sync.Mutex
	targetAddr string = "127.0.0.1:60440"
)

func main() {
	isCLI := flag.Bool("cli", false, "internal flag for console mode")
	flag.Parse()

	if *isCLI {
		runUDPShell()
		return
	}

	a := app.NewWithID("com.udp.terminal.tool")
	a.SetIcon(resourceIconPng)

	if desk, ok := a.(desktop.App); ok {
		menu := fyne.NewMenu("UDP Tool",
			fyne.NewMenuItem("Open Shell", func() {
				launchSelfInTerminal()
			}),
			fyne.NewMenuItemSeparator(),
			fyne.NewMenuItem("Quit", func() {
				a.Quit()
			}),
		)
		desk.SetSystemTrayMenu(menu)
	}
	a.Run()
}

func launchSelfInTerminal() {
	self, _ := os.Executable()
	var cmd *exec.Cmd

	switch runtime.GOOS {
	case "windows":
		cmd = exec.Command("cmd.exe", "/c", "start", self, "--cli")
	case "darwin":
		cmd = exec.Command("open", "-a", "Terminal", self, "--args", "--cli")
	case "linux":
		terminals := []string{"ptyxis", "gnome-terminal", "gnome-console", "konsole", "xfce4-terminal", "xterm"}
		for _, term := range terminals {
			if _, err := exec.LookPath(term); err == nil {
				cmd = exec.Command(term, "--", self, "--cli")
				break
			}
		}
	}
	if cmd != nil {
		_ = cmd.Start()
	}
}

func connectUDP(addrStr string) error {
	connMu.Lock()
	defer connMu.Unlock()

	if udpConn != nil {
		udpConn.Close()
	}

	addr, err := net.ResolveUDPAddr("udp", addrStr)
	if err != nil {
		return err
	}

	conn, err := net.DialUDP("udp", nil, addr)
	if err != nil {
		return err
	}

	udpConn = conn
	targetAddr = addrStr
	return nil
}

func runUDPShell() {
	home, _ := os.UserHomeDir()
	historyPath := filepath.Join(home, ".udp_shell_history")

	rl, err := readline.NewEx(&readline.Config{
		Prompt:      "\033[32mUDP-Shell>\033[0m ",
		HistoryFile: historyPath,
	})
	if err != nil {
		return
	}
	defer rl.Close()

	if err := connectUDP(targetAddr); err != nil {
		fmt.Printf("\033[31mInitial Connection Error: %v\033[0m\n", err)
	}

	// Receiver Loop
	go func() {
		buf := make([]byte, 8192)
		for {
			connMu.Lock()
			currConn := udpConn
			connMu.Unlock()

			if currConn == nil {
				continue
			}

			n, _, err := currConn.ReadFromUDP(buf)
			if err != nil {
				continue 
			}

			raw := buf[:n]
			var output string
			if hexMode {
				output = fmt.Sprintf("\033[36m[HEX]: %s\033[0m", hex.EncodeToString(raw))
			} else {
				output = strings.TrimRight(string(raw), "\r\n")
			}

			fmt.Printf("\r\033[K%s\n", output)
			rl.Refresh()
			os.Stdout.Sync()
		}
	}()

	fmt.Printf("Shell Active. Target: %s. Type ^help for commands.\n", targetAddr)

	for {
		line, err := rl.Readline()
		if err != nil {
			break
		}

		line = strings.TrimSpace(line)
		if line == "" {
			continue
		}

		if strings.HasPrefix(line, "^") {
			cmdBody := strings.TrimPrefix(line, "^")
			response := myLocalFunction(cmdBody, rl)
			if response != "" {
				fmt.Printf("\033[33m%s\033[0m\n", response)
				os.Stdout.Sync()
			}
			continue
		}

		connMu.Lock()
		if udpConn != nil {
			_, _ = udpConn.Write([]byte(line + "\n"))
		}
		connMu.Unlock()
	}
}

func getLocalIPs() string {
	addrs, err := net.InterfaceAddrs()
	if err != nil {
		return "Error retrieving IPs"
	}
	var sb strings.Builder
	sb.WriteString("Local IPv4 Addresses:\n")
	for _, addr := range addrs {
		if ipnet, ok := addr.(*net.IPNet); ok && !ipnet.IP.IsLoopback() {
			if ipnet.IP.To4() != nil {
				sb.WriteString(fmt.Sprintf("  - %s\n", ipnet.IP.String()))
			}
		}
	}
	return sb.String()
}

func myLocalFunction(input string, rl *readline.Instance) string {
	parts := strings.Fields(input)
	if len(parts) == 0 {
		return ""
	}
	cmd := strings.ToLower(parts[0])

	switch cmd {
	case "ip":
		return getLocalIPs()

	case "target":
		if len(parts) < 2 {
			return "Usage: ^target <ip>:<port>"
		}
		newAddr := parts[1]
		if err := connectUDP(newAddr); err != nil {
			return fmt.Sprintf("Failed to switch target: %v", err)
		}
		return "Switched target to: " + newAddr

	case "hex":
		hexMode = !hexMode
		status := "OFF"
		if hexMode { status = "ON" }
		return "Hex view is now " + status

	case "prompt":
		// Re-joining in case the user didn't use quotes but has spaces
		content := strings.Join(parts[1:], " ")
		content = strings.Trim(content, "\"'")
		if content == "" {
			return "Usage: ^prompt \"# \""
		}
		rl.SetPrompt(fmt.Sprintf("\033[35m%s\033[0m", content))
		return "Prompt updated."

	case "help":
		return "Local Commands:\n" +
			"  ^ip            - Show local machine IP addresses\n" +
			"  ^target <addr> - Change UDP destination (e.g. ^target 192.168.1.5:60440)\n" +
			"  ^prompt \"text\" - Change prompt\n" +
			"  ^hex           - Toggle hex-dump\n" +
			"  ^cls           - Clear screen\n" +
			"  ^exit          - Exit shell"
	case "cls":
		fmt.Print("\033[H\033[2J")
		return ""
	case "version":
		return "UDP-Term v1.5.0 (Network Tools Edition)"
	case "exit":
		os.Exit(0)
	}

	return "Unknown command: " + input
}
