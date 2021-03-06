#!../src/bltwish

package require BLT

set graph .graph

option add *x.Title			"X Axis"
option add *y.Title			"Y Axis"
option add *LineMarker.Foreground	yellow

blt::htext .header -text \
{   This is an example of the barchart widget.  The barchart has 
    many components; x and y axis, legend, crosshairs, elements, etc.  
    To create a postscript file "bar.ps", press the %%
    set w $htext(widget)
    button $w.print -text {Print} -command {
	$graph postscript output bar.ps 
    } 
    $w append $w.print

%% button.  
}
blt::barchart $graph -background white 
$graph axis configure x -stepsize 0 

blt::htext .footer -text {    Hit the %%
    set im [image create picture -file ./images/stopsign.gif]
    button $htext(widget).quit -image $im -command { exit }
    $htext(widget) append $htext(widget).quit -pady 2
%% button when you've seen enough. %%
    label $htext(widget).logo -bitmap BLT
    $htext(widget) append $htext(widget).logo 
%%}

set attributes { 
    red	
    orange
    yellow
    green
    blue
    cyan
    magenta
    violetred
    purple
    lightblue
}

set count 0
foreach color $attributes {
    $graph pen create pen$count \
	-fill ${color}1 -outline ${color}4 -relief solid
    lappend styles [list pen$count $count $count]
    incr count
}

blt::vector x y w

x linspace 0 800 400
y expr sin(x)*90.0
w expr round(y*20.0)%$count
y expr y+10.0

$graph element create data -label {} \
    -x x -y y -weight w -styles $styles

blt::table . \
    0,0 .header -fill x  \
    1,0 .graph -fill both \
    2,0 .footer -fill x

blt::table configure . r0 r2 -resize none
	
wm min . 0 0

Blt_ZoomStack $graph
Blt_Crosshairs $graph
Blt_ActiveLegend $graph
Blt_ClosestPoint $graph

