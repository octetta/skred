#!/usr/bin/env wish8

# Force scaling: Match your desktop scale (e.g., 1.5 for 150%, 2.0 for 200%)
tk scaling 5

# Now your slider will be larger globally
ttk::style theme use clam
ttk::scale .s -orient horizontal -from 0 -to 100
pack .s -fill x -padx 20 -pady 20

# Optional: Label for value
set ::val 50
.s configure -variable ::val
pack [label .l -textvariable ::val]
