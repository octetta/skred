//go:build windows

package main

import (
	"os/exec"
	"syscall"
  "unsafe"
)

var (
	user32             = syscall.NewLazyDLL("user32.dll")
	kernel32           = syscall.NewLazyDLL("kernel32.dll")
	procShowWindow     = user32.NewProc("ShowWindow")
	procGetConsoleW    = kernel32.NewProc("GetConsoleWindow")
	procSetConsoleMode = kernel32.NewProc("SetConsoleMode")
	procGetConsoleMode = kernel32.NewProc("GetConsoleMode")
)

func setPlatformSpecificAttrs(cmd *exec.Cmd) {
	if cmd.SysProcAttr == nil {
		cmd.SysProcAttr = &syscall.SysProcAttr{}
	}
	cmd.SysProcAttr.HideWindow = true
	cmd.SysProcAttr.CreationFlags = 0x08000000 // CREATE_NO_WINDOW
}

func setConsoleAttrs(cmd *exec.Cmd) {
	if cmd.SysProcAttr == nil {
		cmd.SysProcAttr = &syscall.SysProcAttr{}
	}
	cmd.SysProcAttr.CreationFlags = 0x00000010 // CREATE_NEW_CONSOLE
}

func hideSelf() {
	hwnd, _, _ := procGetConsoleW.Call()
	if hwnd != 0 {
		// 0 = SW_HIDE
		procShowWindow.Call(hwnd, 0)
	}
}

// setupTerminal enables ANSI escape sequences in the Windows console
func setupTerminal() {
	handle, err := syscall.GetStdHandle(syscall.STD_OUTPUT_HANDLE)
	if err != nil {
		return
	}

	var mode uint32
	// Call GetConsoleMode via DLL
	ret, _, _ := procGetConsoleMode.Call(uintptr(handle), uintptr(unsafe.Pointer(&mode)))
	if ret == 0 {
		return
	}

	// 0x0004 is ENABLE_VIRTUAL_TERMINAL_PROCESSING
	mode |= 0x0004
	
	// Call SetConsoleMode via DLL
	procSetConsoleMode.Call(uintptr(handle), uintptr(mode))
}
