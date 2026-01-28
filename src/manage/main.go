package main

import (
	"fmt"
	"log"
	"time"

	"fyne.io/systray"
	"fyne.io/systray/example/icon"
  "github.com/gofrs/flock"

  "os"
  "os/exec"
  "path/filepath"
)

func main() {
	onExit := func() {
		now := time.Now()
		fmt.Println("Exit at", now.String())
	}

  // 1. Define a lock file path in the Temp directory
  lockPath := filepath.Join(os.TempDir(), "my_app_tray.lock")
  fileLock := flock.New(lockPath)

  // 2. Try to lock the file
  locked, err := fileLock.TryLock()
  if err != nil || !locked {
    fmt.Println("App is already running. Exiting.")
    os.Exit(0)
  }

  //
	// systray.Run(onReady, onExit)
	// fmt.Println("Finished quitting")
  //
  // Check for a custom flag to see if we are the "child"
  if len(os.Args) > 1 && os.Args[1] == "--child" {
    systray.Run(onReady, onExit)
    return
  }

  // Otherwise, we are the parent: Start the child and exit
  cmd := exec.Command(os.Args[0], "--child")
  cmd.Start() // Start() is non-blocking
  os.Exit(0)  // Exit the parent, returning control to the shell
  //
}

func addQuitItem() {
	mQuit := systray.AddMenuItem("Quit", "Quit the whole app")
	go func() {
		for range mQuit.ClickedCh {
			fmt.Println("Requesting quit")
			systray.Quit()
		}
	}()
}

func onReady() {
	systray.SetTemplateIcon(icon.Data, icon.Data)
	systray.SetTitle("Awesome App")
	systray.SetTooltip("Lantern")
	addQuitItem()
	systray.AddSeparator()

	systray.SetOnSecondaryTapped(func() {
		log.Println("Custom right click!")
	})

	// We can manipulate the systray in other goroutines
	go func() {
		systray.SetTemplateIcon(icon.Data, icon.Data)
		systray.SetTitle("Awesome App")
		systray.SetTooltip("Pretty awesome棒棒嗒")
		trayOpenedCount := 0
		mOpenedCount := systray.AddMenuItem("Tray opened count", "Tray opened count")
		mChange := systray.AddMenuItem("Change Me", "Change Me")
		mAllowRemoval := systray.AddMenuItem("Allow removal", "macOS only: allow removal of the icon when cmd is pressed")
		mChecked := systray.AddMenuItemCheckbox("Checked", "Check Me", true)
		mEnabled := systray.AddMenuItem("Enabled", "Enabled")
		// Sets the icon of a menu item. Only available on Mac.
		mEnabled.SetTemplateIcon(icon.Data, icon.Data)

		systray.AddMenuItem("Ignored", "Ignored")

		subMenuTop := systray.AddMenuItem("SubMenuTop", "SubMenu Test (top)")
		subMenuMiddle := subMenuTop.AddSubMenuItem("SubMenuMiddle", "SubMenu Test (middle)")
		subMenuBottom := subMenuMiddle.AddSubMenuItemCheckbox("SubMenuBottom - Toggle Panic!", "SubMenu Test (bottom) - Hide/Show Panic!", false)
		subMenuMiddle.AddSeparator()
		subMenuBottom2 := subMenuMiddle.AddSubMenuItem("SubMenuBottom - Panic!", "SubMenu Test (bottom)")

		systray.AddSeparator()
		mToggle := systray.AddMenuItem("Toggle", "Toggle some menu items")
		shown := true
		toggle := func() {
			if shown {
				subMenuBottom.Check()
				subMenuBottom2.Hide()
				mEnabled.Hide()
				shown = false
			} else {
				subMenuBottom.Uncheck()
				subMenuBottom2.Show()
				mEnabled.Show()
				shown = true
			}
		}
		mReset := systray.AddMenuItem("Reset", "Reset all items")

		go func() {
			for range mChange.ClickedCh {
				mChange.SetTitle("I've Changed")
			}
		}()
		go func() {
			for range mChecked.ClickedCh {
				if mChecked.Checked() {
					mChecked.Uncheck()
					mChecked.SetTitle("Unchecked")
				} else {
					mChecked.Check()
					mChecked.SetTitle("Checked")
				}
			}
		}()
		go func() {
			for range mAllowRemoval.ClickedCh {
				systray.SetRemovalAllowed(true)
				go func() {
					time.Sleep(5 * time.Second)
					fmt.Printf("Time's up! setting back to no-removal-allowed on macOS.\n")
					systray.SetRemovalAllowed(false)
				}()
			}
		}()
		go func() {
			for range mEnabled.ClickedCh {
				mEnabled.SetTitle("Disabled")
				mEnabled.Disable()
			}
		}()
		go func() {
			for range subMenuBottom2.ClickedCh {
				panic("panic button pressed")
			}
		}()
		go func() {
			for range subMenuBottom.ClickedCh {
				toggle()
			}
		}()
		go func() {
			for range mReset.ClickedCh {
				systray.ResetMenu()
				addQuitItem()
			}
		}()
		go func() {
			for range mToggle.ClickedCh {
				toggle()
			}
		}()
		go func() {
			for range systray.TrayOpenedCh {
				trayOpenedCount++
				mOpenedCount.SetTitle(fmt.Sprintf("Tray opened count: %d", trayOpenedCount))
			}
		}()
	}()
}
