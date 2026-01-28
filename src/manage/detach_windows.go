//go:build windows
package main

import (
	"os/exec"
	"syscall"
  "golang.org/x/sys/windows" // Use this for Windows-specific constants
)

func detachProcess(cmd *exec.Cmd) {
	cmd.SysProcAttr = &syscall.SysProcAttr{
		CreationFlags: windows.DETACHED_PROCESS | windows.CREATE_NO_WINDOW,
	}
}
