package require BLT

set normalBg [blt::background create linear \
		  -highcolor grey75 -lowcolor grey98 -jitter 3]
blt::palette create bluegreen -colorformat name -cdata {
    white  green2
} 
blt::scale .s -orient vertical -markcolor grey50 -hide all \
    -show "title ticks ticklabels colorbar arrows mark grip" \
    -tickdirection out -loose yes -min -23.3 -max 43.2 \
    -title fred -axislinewidth 5 -ticklabelcolor grey0 \
    -minarrowcolor 0x80FF0000 \
    -maxarrowcolor 0x800000FF \
    -axislinecolor grey30 \
    -palette bluegreen -rangecolor white -titlejustify l \
    -height 5i -tickfont "Arial 8" -tickangle 0 -bg $normalBg

blt::table . \
    0,0 .s -fill both

bind .s <Enter> {
    .s configure -show grip
}
bind .s <Leave> {
    .s configure -hide grip
}

.s bind minarrow <Enter> {
    puts stderr "activate minarrow"
    .s activate minarrow
}
.s bind minarrow <Leave> {
    puts stderr "deactivate minarrow"
    .s deactivate minarrow
}
.s bind maxarrow <Enter> {
    puts stderr "activate maxarrow"
    .s activate maxarrow
}
.s bind maxarrow <Leave> {
    puts stderr "deactivate maxarrow"
    .s deactivate maxarrow
}
.s bind grip <Enter> {
    puts stderr "activate grip"
    .s activate grip
}
.s bind grip <Leave> {
    puts stderr "deactivate grip"
    .s deactivate grip
}
.s bind grip <ButtonPress-1> {
    puts stderr "pressed on grip"
}
.s bind grip <B1-Motion> {
    puts stderr "moved grip"
    .s set [.s invtransform %x %y]
}
.s bind grip <ButtonRelease-1> {
    puts stderr "unpresseed grip"
}

.s bind minarrow <ButtonPress-1> {
    puts stderr "pressed on minarrow"
}
.s bind minarrow <B1-Motion> {
    puts stderr "moved minarrow"
    .s configure -rangemin [.s invtransform %x %y]
}
.s bind minarrow <ButtonRelease-1> {
    puts stderr "unpresseed minarrow"
}

.s bind maxarrow <ButtonPress-1> {
    puts stderr "pressed on maxarrow"
}
.s bind maxarrow <B1-Motion> {
    puts stderr "moved maxarrow"
    .s configure -rangemax [.s invtransform %x %y]
}
.s bind maxarrow <ButtonRelease-1> {
    puts stderr "unpresseed maxarrow"
}
