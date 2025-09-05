#!/usr/bin/env wish

# Global variables for slider configuration

set minVal 0.0
set maxVal 1.1
set stepVal 0.0001
set currentVal 0.5
set formatString "c1,%s"

if {$argc > 0} {set minVal [lindex $argv 0]}
if {$argc > 1} {set maxVal [lindex $argv 1]}
if {$argc > 2} {set stepVal [lindex $argv 2]}
if {$argc > 3} {set formatString [lindex $argv 3]}
if {$argc > 4} {set currentString [lindex $argv 4]}

package require Tk
package require udp

set addr 127.0.0.1
set port 60440

set sock [udp_open]
fconfigure $sock -buffering none -translation binary

proc dest {addr port} {
    # puts "-> $addr $port"
    fconfigure $::sock -remote [list $addr $port]
}

dest $addr $port

proc wire {msg} {
    # puts $msg
    puts -nonewline $::sock $msg
}

# Procedure to be called when slider changes
proc fire {value} {
    global currentVal formatString
    set currentVal $value
    set formattedOutput [format $formatString $value]
    # puts $formattedOutput
    wire $formattedOutput
}

# Procedure to update slider configuration
proc updateSlider {} {
    global minVal maxVal stepVal currentVal
    
    # Validate inputs
    if {![string is double $minVal] || ![string is double $maxVal] || ![string is double $stepVal]} {
        tk_messageBox -type ok -icon error -title "Error" -message "Please enter valid numeric values"
        return
    }
    
    if {$stepVal <= 0} {
        tk_messageBox -type ok -icon error -title "Error" -message "Step value must be greater than 0"
        return
    }
    
    if {$minVal >= $maxVal} {
        tk_messageBox -type ok -icon error -title "Error" -message "Minimum value must be less than maximum value"
        return
    }
    
    # Temporarily remove the command to prevent fire from being called
    .slider configure -command {}
    
    # Update slider configuration
    .slider configure -from $minVal -to $maxVal -resolution $stepVal
    
    # Only adjust current value if it's outside the new range
    if {$currentVal < $minVal || $currentVal > $maxVal} {
        set currentVal $minVal
    }
    
    # Restore the command after updating
    .slider configure -command {fire}
    
    # puts "Slider updated: Min=$minVal, Max=$maxVal, Step=$stepVal"
}

# Create main window
wm title . "fire"
wm geometry . ;# 400x350

# Create frame for input fields
frame .inputs ;# -relief raised -bd 1
pack .inputs -side top -fill x -padx 4 -pady 4

# Create labels and entry fields for A, B, C, and Format
label .inputs.labelA -text "min"
entry .inputs.entryA -textvariable minVal -width 10
grid .inputs.labelA .inputs.entryA -sticky w -padx 5 -pady 2

label .inputs.labelB -text "max"
entry .inputs.entryB -textvariable maxVal -width 10
grid .inputs.labelB .inputs.entryB -sticky w -padx 5 -pady 2

label .inputs.labelC -text "step"
entry .inputs.entryC -textvariable stepVal -width 10
grid .inputs.labelC .inputs.entryC -sticky w -padx 5 -pady 2

label .inputs.labelFormat -text "wire"
entry .inputs.entryFormat -textvariable formatString -width 20
grid .inputs.labelFormat .inputs.entryFormat -sticky w -padx 5 -pady 2

# Update button
button .inputs.update -text "update" -command updateSlider
grid .inputs.update -columnspan 2 -pady 2

# Bind Enter key to update slider for each entry field
bind .inputs.entryA <Return> updateSlider
bind .inputs.entryB <Return> updateSlider
bind .inputs.entryC <Return> updateSlider
bind .inputs.entryFormat <Return> updateSlider

# Create frame for slider
frame .sliderFrame ;# -relief raised -bd 1
pack .sliderFrame -side top -fill both -expand true -padx 4 -pady 4

# Create slider label
# label .sliderFrame.label -text "value"
# pack .sliderFrame.label -side top -anchor w

# Create the slider
scale .slider -from $minVal -to $maxVal -resolution $stepVal \
    -orient horizontal -variable currentVal -command {fire}
pack .slider -in .sliderFrame -side top -fill x -pady 10

# Create current value display
# label .sliderFrame.current -textvariable currentVal -relief sunken -bd 1
# pack .sliderFrame.current -side top -anchor w

# Initialize display

###
proc backgroundSelf {} {
    global argv0 argv
    
    if {![info exists ::env(WISH_DETACHED)]} {
        set ::env(WISH_DETACHED) 1
        
        # Try to background using platform-appropriate method
        if {$::tcl_platform(platform) eq "windows"} {
            exec wish $argv0 {*}$argv &
        } else {
            exec nohup wish $argv0 {*}$argv > /dev/null 2>&1 &
        }
        exit 0
    }
}

# Call background function at startup
backgroundSelf
