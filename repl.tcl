#!/usr/bin/env tclsh
package require Tk

# Your custom language evaluator - replace this with your actual implementation
# Should return a dict with keys: status (ok/error), result (the output text)
proc evaluate_custom_language {cmd} {
    # Example: just echo back with a prefix
    # Replace this with your actual language interpreter
    
    # Simulate an error for demo purposes
    if {[string match "error*" $cmd]} {
        return [dict create status error result "ERROR: Something went wrong!"]
    }
    
    return [dict create status ok result "RESULT: $cmd"]
}

# Global preferences
set ::preferences(font_family) "Courier"
set ::preferences(font_size) 10
set ::preferences(theme) "light"
set ::preferences(prompt_text) ">> "
set ::preferences(window_title) "Custom Language REPL"

# Theme-specific colors
array set ::themes {
    light,bg white
    light,fg black
    light,prompt_color blue
    light,input_color black
    light,output_color green
    light,error_color red
    light,status_bg #f0f0f0
    light,status_fg black
    dark,bg #1e1e1e
    dark,fg #d4d4d4
    dark,prompt_color #4ec9b0
    dark,input_color #d4d4d4
    dark,output_color #6a9955
    dark,error_color #f48771
    dark,status_bg #2d2d2d
    dark,status_fg #cccccc
}

# Get current theme color
proc theme_color {key} {
    return $::themes($::preferences(theme),$key)
}

# Status bar state
set ::status(indicator) "green"
set ::status(text) "okay"

# Background message channel (set to empty if not using)
set ::bg_channel ""

# REPL Implementation
proc setup_repl {{title ""} {prompt ""}} {
    # Set window title (use parameter or default from preferences)
    if {$title ne ""} {
        set ::preferences(window_title) $title
    }
    wm title . $::preferences(window_title)
    
    # Set prompt text (use parameter or default from preferences)
    if {$prompt ne ""} {
        set ::preferences(prompt_text) $prompt
    }
    
    # Create menu bar
    create_menus
    
    # Create text widget with scrollbar
    frame .f
    text .f.text -yscrollcommand {.f.scroll set} -wrap word -height 24 -width 80 \
        -font [list $::preferences(font_family) $::preferences(font_size)] \
        -background [theme_color bg] -foreground [theme_color fg] \
        -insertbackground [theme_color fg]
    scrollbar .f.scroll -command {.f.text yview}
    pack .f.scroll -side right -fill y
    pack .f.text -side left -fill both -expand 1
    pack .f -fill both -expand 1
    
    # Create status bar
    frame .status -relief sunken -bd 1 -background [theme_color status_bg]
    canvas .status.indicator -width 16 -height 16 -background [theme_color status_bg] -highlightthickness 0
    .status.indicator create oval 2 2 14 14 -fill $::status(indicator) -tags dot
    label .status.text -textvariable ::status(text) -background [theme_color status_bg] \
        -foreground [theme_color status_fg] -anchor w
    pack .status.indicator -side left -padx 5 -pady 2
    pack .status.text -side left -fill x -expand 1 -pady 2
    pack .status -side bottom -fill x
    
    # Configure tags for styling
    .f.text tag configure prompt -foreground [theme_color prompt_color]
    .f.text tag configure input -foreground [theme_color input_color]
    .f.text tag configure output -foreground [theme_color output_color]
    .f.text tag configure error -foreground [theme_color error_color]
    
    # Initialize history
    set ::history [list]
    set ::history_pos -1
    
    # Show initial prompt
    show_prompt
    
    # Bind Return key to evaluate command
    bind .f.text <Return> {
        evaluate_line
        return -code break
    }
    
    # Bind Up/Down for history navigation
    bind .f.text <Up> {
        history_prev
        return -code break
    }
    
    bind .f.text <Down> {
        history_next
        return -code break
    }
    
    # Prevent editing before the prompt
    bind .f.text <Key> {
        if {[.f.text compare insert < prompt_end]} {
            .f.text mark set insert end
        }
    }
    
    bind .f.text <BackSpace> {
        if {[.f.text compare insert <= prompt_end]} {
            return -code break
        }
    }
    
    bind .f.text <ButtonRelease-1> {
        if {[.f.text compare insert < prompt_end]} {
            .f.text mark set insert end
        }
    }
    
    # Keep cursor at the end
    .f.text mark set insert end
    .f.text see insert
    focus .f.text
}

proc setup_background_channel {channel} {
    # Call this with an open file channel to receive background messages
    # Example: set ::bg_channel [open "| your_program" r]
    #          setup_background_channel $::bg_channel
    set ::bg_channel $channel
    fconfigure $channel -blocking 0 -buffering line
    fileevent $channel readable [list handle_background_message $channel]
}

proc handle_background_message {channel} {
    if {[eof $channel]} {
        catch {close $channel}
        set ::bg_channel ""
        return
    }
    
    if {[gets $channel line] >= 0} {
        display_background_message $line
    }
}

proc display_background_message {msg} {
    # Save current input if any exists
    set current_input ""
    if {[.f.text compare prompt_end < end]} {
        set current_input [.f.text get prompt_end "end-1c"]
    }
    
    # Remove the current prompt and input
    .f.text delete prompt_end end
    
    # Insert the background message with a visual separator
    .f.text insert end "\n[" prompt
    .f.text insert end "BACKGROUND" output
    .f.text insert end "] " prompt
    .f.text insert end "$msg\n" output
    
    # Restore the prompt
    show_prompt
    
    # Restore any input that was in progress
    if {$current_input ne ""} {
        .f.text insert end $current_input input
    }
    
    # Scroll to show the new content
    .f.text see end
}

proc create_menus {} {
    menu .menubar
    . configure -menu .menubar
    
    # File menu
    menu .menubar.file -tearoff 0
    .menubar add cascade -label "File" -menu .menubar.file -underline 0
    .menubar.file add command -label "Open..." -command file_open -accelerator "Ctrl+O"
    .menubar.file add command -label "Save..." -command file_save -accelerator "Ctrl+S"
    .menubar.file add separator
    .menubar.file add command -label "Clear Console" -command clear_console
    .menubar.file add separator
    .menubar.file add command -label "Exit" -command exit -accelerator "Ctrl+Q"
    
    # Edit menu
    menu .menubar.edit -tearoff 0
    .menubar add cascade -label "Edit" -menu .menubar.edit -underline 0
    .menubar.edit add command -label "Cut" -command {event generate .f.text <<Cut>>} -accelerator "Ctrl+X"
    .menubar.edit add command -label "Copy" -command {event generate .f.text <<Copy>>} -accelerator "Ctrl+C"
    .menubar.edit add command -label "Paste" -command {event generate .f.text <<Paste>>} -accelerator "Ctrl+V"
    .menubar.edit add separator
    .menubar.edit add command -label "Select All" -command {.f.text tag add sel 1.0 end} -accelerator "Ctrl+A"
    
    # Preferences menu
    menu .menubar.prefs -tearoff 0
    .menubar add cascade -label "Preferences" -menu .menubar.prefs -underline 0
    .menubar.prefs add command -label "Font..." -command choose_font
    .menubar.prefs add separator
    .menubar.prefs add radiobutton -label "Light Theme" -variable ::preferences(theme) \
        -value "light" -command apply_theme
    .menubar.prefs add radiobutton -label "Dark Theme" -variable ::preferences(theme) \
        -value "dark" -command apply_theme
    .menubar.prefs add separator
    .menubar.prefs add command -label "Customize Colors..." -command choose_colors
    .menubar.prefs add separator
    .menubar.prefs add command -label "Prompt Text..." -command set_prompt_text
    
    # Bind keyboard shortcuts
    bind . <Control-o> file_open
    bind . <Control-s> file_save
    bind . <Control-q> exit
}

proc file_open {} {
    set filename [tk_getOpenFile -title "Open File" \
        -filetypes {{"Text Files" .txt} {"All Files" *}}]
    
    if {$filename ne ""} {
        if {[catch {open $filename r} fh]} {
            tk_messageBox -icon error -title "Error" \
                -message "Could not open file: $fh"
            return
        }
        
        set content [read $fh]
        close $fh
        
        .f.text delete 1.0 end
        .f.text insert end $content
        show_prompt
    }
}

proc file_save {} {
    set filename [tk_getSaveFile -title "Save File" \
        -filetypes {{"Text Files" .txt} {"All Files" *}} \
        -defaultextension .txt]
    
    if {$filename ne ""} {
        if {[catch {open $filename w} fh]} {
            tk_messageBox -icon error -title "Error" \
                -message "Could not save file: $fh"
            return
        }
        
        set content [.f.text get 1.0 end-1c]
        puts -nonewline $fh $content
        close $fh
        
        tk_messageBox -icon info -title "Success" \
            -message "File saved successfully"
    }
}

proc clear_console {} {
    .f.text delete 1.0 end
    show_prompt
}

proc choose_font {} {
    # Create font chooser dialog
    set w .fontdlg
    if {[winfo exists $w]} {
        destroy $w
    }
    
    toplevel $w
    wm title $w "Choose Font"
    wm transient $w .
    
    # Font family
    frame $w.f1
    label $w.f1.l -text "Font Family:"
    set families [lsort [font families]]
    
    # Create listbox with scrollbar for font families
    frame $w.f1.list
    listbox $w.f1.list.lb -height 10 -yscrollcommand "$w.f1.list.sb set" \
        -exportselection 0
    scrollbar $w.f1.list.sb -command "$w.f1.list.lb yview"
    pack $w.f1.list.sb -side right -fill y
    pack $w.f1.list.lb -side left -fill both -expand 1
    
    foreach f $families {
        $w.f1.list.lb insert end $f
    }
    
    # Select current font
    set idx [lsearch $families $::preferences(font_family)]
    if {$idx >= 0} {
        $w.f1.list.lb selection set $idx
        $w.f1.list.lb see $idx
    }
    
    pack $w.f1.l -side top -anchor w
    pack $w.f1.list -side top -fill both -expand 1
    pack $w.f1 -side top -fill both -expand 1 -padx 5 -pady 5
    
    # Font size
    frame $w.f2
    label $w.f2.l -text "Font Size:"
    spinbox $w.f2.spin -from 6 -to 48 -width 5 \
        -textvariable ::temp_font_size
    set ::temp_font_size $::preferences(font_size)
    pack $w.f2.l -side left
    pack $w.f2.spin -side left -padx 5
    pack $w.f2 -side top -anchor w -padx 5 -pady 5
    
    # Preview
    frame $w.preview
    label $w.preview.l -text "Preview:"
    text $w.preview.t -height 3 -width 40
    $w.preview.t insert end "The quick brown fox jumps over the lazy dog\n0123456789"
    pack $w.preview.l -side top -anchor w
    pack $w.preview.t -side top -fill x
    pack $w.preview -side top -fill x -padx 5 -pady 5
    
    # Update preview when selection changes
    proc update_preview {} {
        set w .fontdlg
        set sel [$w.f1.list.lb curselection]
        if {$sel ne ""} {
            set family [$w.f1.list.lb get $sel]
            $w.preview.t configure -font [list $family $::temp_font_size]
        }
    }
    
    bind $w.f1.list.lb <<ListboxSelect>> update_preview
    bind $w.f2.spin <KeyRelease> update_preview
    update_preview
    
    # Buttons
    frame $w.buttons
    button $w.buttons.ok -text "OK" -command "apply_font $w"
    button $w.buttons.cancel -text "Cancel" -command "destroy $w"
    pack $w.buttons.ok $w.buttons.cancel -side left -padx 5
    pack $w.buttons -side bottom -pady 10
    
    # Center the dialog
    update idletasks
    set x [expr {[winfo x .] + ([winfo width .] - [winfo width $w]) / 2}]
    set y [expr {[winfo y .] + ([winfo height .] - [winfo height $w]) / 2}]
    wm geometry $w +$x+$y
}

proc apply_font {w} {
    set sel [$w.f1.list.lb curselection]
    if {$sel ne ""} {
        set ::preferences(font_family) [$w.f1.list.lb get $sel]
        set ::preferences(font_size) $::temp_font_size
        .f.text configure -font [list $::preferences(font_family) $::preferences(font_size)]
    }
    destroy $w
}

proc choose_colors {} {
    set w .colordlg
    if {[winfo exists $w]} {
        destroy $w
    }
    
    toplevel $w
    wm title $w "Customize Colors"
    wm transient $w .
    
    label $w.info -text "Customizing colors for: [string totitle $::preferences(theme)] Theme" \
        -font {TkDefaultFont 10 bold}
    pack $w.info -side top -pady 10
    
    set row 0
    foreach {label key} {
        "Background:" bg
        "Foreground:" fg
        "Prompt:" prompt_color
        "Input:" input_color
        "Output:" output_color
        "Error:" error_color
        "Status Bar BG:" status_bg
        "Status Bar FG:" status_fg
    } {
        frame $w.f$row
        label $w.f$row.l -text $label -width 15 -anchor w
        set color [theme_color $key]
        button $w.f$row.b -text "Choose" -width 10 \
            -command "pick_theme_color $key $w.f$row.sample"
        canvas $w.f$row.sample -width 40 -height 20 -background $color
        pack $w.f$row.l -side left -padx 5
        pack $w.f$row.b -side left -padx 5
        pack $w.f$row.sample -side left -padx 5
        pack $w.f$row -side top -pady 5 -fill x
        incr row
    }
    
    # Buttons
    frame $w.buttons
    button $w.buttons.ok -text "OK" -command "apply_theme; destroy $w"
    button $w.buttons.cancel -text "Cancel" -command "destroy $w"
    pack $w.buttons.ok $w.buttons.cancel -side left -padx 5
    pack $w.buttons -side bottom -pady 10
    
    # Center the dialog
    update idletasks
    set x [expr {[winfo x .] + ([winfo width .] - [winfo width $w]) / 2}]
    set y [expr {[winfo y .] + ([winfo height .] - [winfo height $w]) / 2}]
    wm geometry $w +$x+$y
}

proc pick_theme_color {key sample} {
    set theme_key "$::preferences(theme),$key"
    set color [tk_chooseColor -initialcolor $::themes($theme_key) -title "Choose Color"]
    if {$color ne ""} {
        set ::themes($theme_key) $color
        $sample configure -background $color
    }
}

proc apply_theme {} {
    # Update text widget colors
    .f.text configure -background [theme_color bg] \
        -foreground [theme_color fg] \
        -insertbackground [theme_color fg]
    
    # Update text tags
    .f.text tag configure prompt -foreground [theme_color prompt_color]
    .f.text tag configure input -foreground [theme_color input_color]
    .f.text tag configure output -foreground [theme_color output_color]
    .f.text tag configure error -foreground [theme_color error_color]
    
    # Update status bar
    .status configure -background [theme_color status_bg]
    .status.indicator configure -background [theme_color status_bg]
    .status.text configure -background [theme_color status_bg] \
        -foreground [theme_color status_fg]
}

proc set_prompt_text {} {
    set w .promptdlg
    if {[winfo exists $w]} {
        destroy $w
    }
    
    toplevel $w
    wm title $w "Set Prompt Text"
    wm transient $w .
    
    frame $w.f
    label $w.f.l -text "Prompt:"
    entry $w.f.e -textvariable ::temp_prompt -width 20
    set ::temp_prompt $::preferences(prompt_text)
    pack $w.f.l -side left -padx 5
    pack $w.f.e -side left -padx 5
    pack $w.f -side top -pady 10
    
    # Buttons
    frame $w.buttons
    button $w.buttons.ok -text "OK" -command {
        set ::preferences(prompt_text) $::temp_prompt
        destroy .promptdlg
    }
    button $w.buttons.cancel -text "Cancel" -command "destroy $w"
    pack $w.buttons.ok $w.buttons.cancel -side left -padx 5
    pack $w.buttons -side bottom -pady 10
    
    # Center the dialog
    update idletasks
    set x [expr {[winfo x .] + ([winfo width .] - [winfo width $w]) / 2}]
    set y [expr {[winfo y .] + ([winfo height .] - [winfo height $w]) / 2}]
    wm geometry $w +$x+$y
    
    focus $w.f.e
}

proc set_status {indicator text} {
    set ::status(indicator) $indicator
    set ::status(text) $text
    .status.indicator itemconfigure dot -fill $indicator
}

proc show_prompt {} {
    .f.text insert end $::preferences(prompt_text) prompt
    .f.text mark set prompt_end insert
    .f.text mark gravity prompt_end left
    .f.text see insert
}

proc evaluate_line {} {
    # Get the current line (after the prompt)
    set line [.f.text get prompt_end "insert lineend"]
    set line [string trim $line]
    
    # Don't evaluate empty lines
    if {$line eq ""} {
        .f.text insert end "\n"
        show_prompt
        return
    }
    
    # Add to history
    lappend ::history $line
    set ::history_pos -1
    
    # Tag the input line with input color
    .f.text tag add input prompt_end "insert lineend"
    
    # Move to new line
    .f.text insert end "\n"
    
    # Evaluate the command
    set_status "yellow" "Evaluating..."
    update idletasks
    
    if {[catch {evaluate_custom_language $line} result_dict]} {
        # Exception occurred during evaluation
        .f.text insert end "EXCEPTION: $result_dict\n" error
        set_status "red" "Exception occurred"
    } else {
        # Check the status returned by the evaluator
        set status [dict get $result_dict status]
        set result [dict get $result_dict result]
        
        if {$status eq "error"} {
            .f.text insert end "$result\n" error
            set_status "red" "Error"
        } else {
            .f.text insert end "$result\n" output
            set_status "green" "Ready"
        }
    }
    
    # Show new prompt
    show_prompt
    
    # Scroll to bottom
    .f.text see insert
}

proc history_prev {} {
    if {[llength $::history] == 0} return
    
    # Initialize position if needed
    if {$::history_pos == -1} {
        set ::history_pos [llength $::history]
    }
    
    if {$::history_pos > 0} {
        incr ::history_pos -1
        replace_current_line [lindex $::history $::history_pos]
    }
}

proc history_next {} {
    if {$::history_pos == -1} return
    
    incr ::history_pos
    if {$::history_pos >= [llength $::history]} {
        set ::history_pos -1
        replace_current_line ""
    } else {
        replace_current_line [lindex $::history $::history_pos]
    }
}

proc replace_current_line {text} {
    .f.text delete prompt_end "insert lineend"
    .f.text insert prompt_end $text
    .f.text see insert
}

# Start the REPL
# You can customize the title and prompt at startup:
# setup_repl "My Language Shell" "$ "
# Or use defaults:

package require udp

# --- Configuration ---
set addr 127.0.0.1
set port 60440

# --- UDP Socket Setup ---
set sock [udp_open]
fconfigure $sock -buffering none -translation binary
proc dest {addr port} {
  fconfigure $::sock -remote [list $addr $port]
}
dest $addr $port

# --- Modified wire procedure (logs and sends) ---
proc wire {msg} {
  # log_udp $msg
  puts -nonewline $::sock $msg
  # For debugging, also print to console
  # puts "SENT -> $msg"
  return SENT
}

# Your custom language evaluator - replace this with your actual implementation
# Should return a dict with keys: status (ok/error), result (the output text)
proc evaluate_custom_language {cmd} {
    set r [wire $cmd]
    
    return [dict create status ok result $r]
}

setup_repl "SkREPL" "# "