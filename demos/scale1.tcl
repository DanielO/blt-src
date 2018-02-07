package require BLT

set normalBg [blt::background create linear \
		  -highcolor grey75 -lowcolor grey98 -jitter 3]
blt::palette create bluegreen -colorformat name -cdata {
    white  green2
} 
blt::scale .s -orient horizontal \
    -loose no -min -23.3 -max 43.2 \
    -title fred  -show value \
    -palette bluegreen  \
    -width 5i -bg $normalBg -resolution 1.0

blt::table . \
    0,0 .s -fill both

proc EditValue { w } {
    if { ![winfo exists .s.editor] } {
	blt::comboeditor .s.editor -exportselection yes
    }
    foreach { x1 y1 x2 y2 } [$w bbox value] break
    incr x1 [winfo rootx $w] 
    incr y1 [winfo rooty $w]
    incr x2 [winfo rootx $w]
    incr y2 [winfo rooty $w]
    .s.editor post -align right -box [list $x1 $y1 $x2 $y2] \
        -text [$w get mark]
    focus -force .s.editor
}
.s bind value <ButtonPress-1> {
    EditValue %W
}    
focus .s
