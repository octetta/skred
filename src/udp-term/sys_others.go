//go:build !windows

package main

import "os/exec"

func setPlatformSpecificAttrs(cmd *exec.Cmd) {}
func setConsoleAttrs(cmd *exec.Cmd)          {}
func hideSelf()                              {}
func setupTerminal()                         {}
