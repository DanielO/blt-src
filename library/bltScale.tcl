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
            editor ""
            focus ""
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
    $w bind value <Enter> {
        %W activate value
    }
    $w bind value <Leave> {
        %W deactivate value
    }
    $w bind value <ButtonPress-1> {
        blt::Scale::PostEditor %W
    }    
    $w bind value <ButtonRelease-1> { 
        blt::Scale::UnpostEditor %W 
    }
}

#
# ImportFromEditor --
#
#   This is called whenever a editor text changes (via the -command
#   callback from the invoke operation of the editor).  Gets the edited
#   text from the editor and sets the corresponding table cell to it.
#
proc blt::Scale::ImportFromEditor { w what value } {
    $w set $value
}

#
# PostEditor --
#
#   Posts the editor at the location of the scale requesting it.  The
#   editor is initialized to the current value and we bind to the editor's
#   <<Value>> event to know if the text was edited.
#
#   The most important part is that we set a grab on the editor.  This will
#   force <ButtonRelease> events to be interpreted by the editor instead of
#   the scale widget.
#
proc blt::Scale::PostEditor { w } {
    if { ![winfo exists $w.editor] } {
	blt::comboeditor $w.editor -exportselection yes
    }
    foreach { x1 y1 x2 y2 } [$w bbox value -root] break
    $w.editor post -align right -box [list $x1 $y1 $x2 $y2] \
        -command [list blt::Scale::ImportFromEditor $w value] \
        -text [$w get mark]
    blt::grab push $w.editor
    set _private(focus) [focus]
    focus -force $w.editor
    bind $w.editor <Unmap> [list blt::Scale::UnpostEditor $w]
}

#
# UnpostEditor --
#
#   Unposts the editor.  Note that the current value isn't set here.  This
#   is done via -command callback.  We don't know if we're unposting the
#   editor because the text was changed or if the user clicked outside of
#   the editor to cancel the operation.
#
proc ::blt::Scale::UnpostEditor { w } {
    variable _private

    catch { focus $_private(focus) }
    # Release grab, if any, and restore the previous grab, if there was
    # one.
    set grab [blt::grab current]
    if { $grab != "" } {
        blt::grab pop $grab
    }
}

