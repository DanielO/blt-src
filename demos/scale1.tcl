package require BLT

blt::palette create bluegreen -colorformat name -cdata {
    "navyblue"  "lightblue" 
} 
blt::scale .s -orient horizontal -markcolor red -hide all \
    -show "title ticks ticklabels grip colorbar arrows mark" \
    -tickdirection out \
    -title fred -axislinewidth 5 \
    -colorbarthickness 20 -palette bluegreen -rangecolor white \
    -width 5i
blt::table . \
    0,0 .s -fill both

