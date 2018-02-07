# -*- mode: tcl; indent-tabs-mode: nil -*- 
#
# bltScale.tcl
#
# Bindings for the BLT scale widget.
#
# Copyright 2018 George A. Howlett. All rights reserved.  
#
#   Redistribution and use in source and binary forms, with or without
#   modification, are permitted provided that the following conditions are
#   met:
#
#   1) Redistributions of source code must retain the above copyright
#      notice, this list of conditions and the following disclaimer.
#   2) Redistributions in binary form must reproduce the above copyright
#      notice, this list of conditions and the following disclaimer in the
#      documentation and/or other materials provided with the
#      distribution.
#   3) Neither the name of the authors nor the names of its contributors
#      may be used to endorse or promote products derived from this
#      software without specific prior written permission.
#   4) Products derived from this software may not be called "BLT" nor may
#      "BLT" appear in their names without specific prior written
#      permission from the author.
#
#   THIS SOFTWARE IS PROVIDED ''AS IS'' AND ANY EXPRESS OR IMPLIED
#   WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
#   MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
#   DISCLAIMED. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
#   LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
#   CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
#   SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
#   BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
#   WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
#   OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
#   IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#
# Indicates whether to activate (highlight) tabs when the mouse passes
# over them.  This is turned off during scan operations.
#

namespace eval blt {
    namespace eval Scale {
	variable _private
	array set _private {
	}
    }
}

bind BltScale <Enter> {
    %W configure -state active
}
bind BltScale <Leave> {
    %W configure -state normal
}
bind BltScale <B1-Enter> {
}
bind BltScale <B1-Leave> {
}

bind BltScale <KeyPress-Left> {
    if { [%W cget -orientation] == "horizontal" &&
         [%W cget -resolution] > 0.0 } {
        %W set [expr [%W get mark] - [%W cget -resolution]]
    }
}
bind BltScale <KeyPress-Right> {
    if { [%W cget -orientation] == "horizontal" &&
         [%W cget -resolution] > 0.0 } {
        %W set [expr [%W get mark] + [%W cget -resolution]]
    }
}
bind BltScale <KeyPress-Down> {
    if { [%W cget -orientation] == "vertical" &&
         [%W cget -resolution] > 0.0 } {
        %W set [expr [%W get mark] - [%W cget -resolution]]
    }
}
bind BltScale <KeyPress-Up> {
    if { [%W cget -orientation] == "vertical" &&
         [%W cget -resolution] > 0.0 } {
        %W set [expr [%W get mark] + [%W cget -resolution]]
    }
}
bind BltScale <KeyPress-Home> {
    %W set [%W get rmin]
}
bind BltScale <KeyPress-End> {
    %W set [%W get rmax]
}

# ----------------------------------------------------------------------
#
# Init
#
#	Invoked from C whenever a new tabset widget is created.
#	Sets up the default bindings for the all tab entries.  
#	These bindings are local to the widget, so they can't be 
#	set through the usual widget class bind tags mechanism.
#
#	<Enter>		Activates the tab.
#	<Leave>		Deactivates all tabs.
#	<ButtonPress-1>	Selects the tab and invokes its command.
#	<Control-ButtonPress-1>	
#			Toggles the tab tearoff.  If the tab contains
#			a embedded widget, it is placed inside of a
#			toplevel window.  If the widget has already
#			been torn off, the widget is replaced back
#			in the tab.
#
# Arguments:	
#	widget		tabset widget
#
# ----------------------------------------------------------------------

proc blt::Scale::Init { w } {
    $w bind minarrow <Enter> {
        %W activate minarrow
    }
    $w bind minarrow <Leave> {
        %W deactivate minarrow
    }
    $w bind maxarrow <Enter> {
        %W activate maxarrow
    }
    $w bind maxarrow <Leave> {
        %W deactivate maxarrow
    }
    $w bind grip <Enter> {
        %W activate grip
    }
    $w bind grip <Leave> {
        %W deactivate grip
    }
    $w bind grip <ButtonPress-1> {
        %W configure -griprelief sunken
    }
    $w bind grip <ButtonRelease-1> {
        %W configure -griprelief raised
    }
    $w bind grip <B1-Motion> {
        %W set [%W invtransform %x %y]
    }
    $w bind minarrow <B1-Motion> {
        %W configure -rangemin [%W invtransform %x %y]
    }
    $w bind maxarrow <B1-Motion> {
        %W configure -rangemax [%W invtransform %x %y]
    }
}

