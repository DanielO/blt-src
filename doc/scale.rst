================
blt::scale
================

------------------------------------------
Create and manipulate scale widgets
------------------------------------------

.. include:: man.rst
.. include:: toc.rst

SYNOPSIS
--------

**blt::scale** *pathName* ?\ *option value* ... ?

DESCRIPTION
-----------

The **blt::scale** command creates and manages *scale* widgets.
A *scale* widget displays button, that when pressed, posts a
**blt::combomenu** widget.

SYNTAX
------

The **blt::scale** command creates a new window using the *pathName*
argument and makes it into a *scale* widget.  The command has the
following form.

  **blt::scale** *pathName* ?\ *option value* ... ?

Additional options may be specified on the command line or in the option
database to configure aspects of the scale such as its background color
or scrollbars. The **blt::scale** command returns its *pathName*
argument.  There must not already exist a window named *pathName*, but
*pathName*'s parent must exist.

The normal size of the *scale* is the computed size to display the
text. You can specify the width and height of the *scale* window
using the **-width** and **-height** widget options.

SCALE PARTS
-----------

A scale is a widget that displays a collection of item arranged in one or
more columns.  There exist several different types of items (specified by
the item's **-type** option), each with different properties.  Items of
different types may be combined in a single menu.  Parts are not
distinct widgets; the entire *scale* is one widget.

The scale is composed of the following parts: 

**axis**
	A rectangle representing the axis line of the scale.
	
**colorbar**
	A gradient color bar displayed along side the axis.
	
**grip**
	A 3D rectangle use to move the mark.
	
**mark**
	A thin rectangle displaying the current value of the scale.
	
**maxarrow**
	A triangle representing the minimum value of the inner range. 

**minarrow**
	A triangle representing the maximum value of the inner range. 

**title**
	Text displayed at the top of the scale.

**value**
	Text of the current value value displayed next to the mark.
	
OPERATIONS
----------

All *scale* operations are invoked by specifying the widget's
pathname, the operation, and any arguments that pertain to that
operation.  The general form is:

  *pathName operation* ?\ *arg arg ...*\ ?

*Operation* and the *arg*\ s determine the exact behavior of the
command.  The following operations are available for *scale* widgets:

*pathName* **activate** *partName*
  Redisplays *partName* using its active colors and relief.  This typically
  is used by widget bindings to highlight items when the pointer is moved
  over parts of the scale.  *PartName* must be one of the following.

  **grip**
	Activates the grip.  The grip is redrawn in its active
	background color and relief. See the **-activegripbackground**
	option.

  **maxarrow**
	Activates the arrow representing the range maximum.  The arrow is
	redrawn in its active color. See the **-activemaxarrowcolor**
	option.

  **minarrow**
	Activates the arrow representing the range minimum.  The arrow is
	redrawn in its active color. See the **-activeminarrowcolor**
	option.

  **value**
	Activates the text displaying the current value of the scale.  The
	text is redrawn in its active foreground and background colors. See
	the **-activevaluebackground** and **-activevalueforeground**
	options.

*pathName* **bbox** *partName*  ?\ *switches* ... ?
  Returns the bounding box of *partName*.  *PartName* must be **colorbar**,
  **grip**, **maxarrow**, **minarrow**, or **value**.

  This command returns a list of 4 numbers that represent the coordinates
  of the upper-left and lower-right corners of the bounding box of the
  part.  By default the coordinates are relative to the scale widget.
  *Switches* may be one of the following.

  **-root**
    Return the bounding box of the part in root coordinates.

*pathName* **bind** *partName* ?\ *sequence*\ ? ?\ *cmdString*\ ?
  Associates *cmdString* with *partName* such that whenever the event
  sequence given by *sequence* occurs for the part, *cmdString* will be
  invoked.  The syntax is similar to the **bind** command except that it
  operates on scale parts, rather than widgets. See the **bind** manual
  entry for complete details on *sequence* and the substitutions performed
  on *cmdString*.

  *PartName* must be **axis**, **colorbar**, **grip**, **mark**,
  **maxarrow**, **minarrow**, **title** or **value**.

  If all arguments are specified then a new binding is created, replacing
  any existing binding for the same *sequence* and *partName*.  If the first
  character of *cmdString* is "+" then *cmdString* augments an existing
  binding rather than replacing it.  If no *cmdString* argument is provided
  then the command currently associated with *partName* and *sequence* (it's
  an error occurs if there's no such binding) is returned.  If both
  *cmdString* and *sequence* are missing then a list of all the event
  sequences for which bindings have been defined for *partName*.

*pathName* **cget** *option*  
  Returns the current value of the widget configuration option given by
  *option*. *Option* may have any of the values accepted by the
  **configure** operation. They are described in the **configure**
  operation below.

*pathName* **configure** ?\ *option*\ ? ?\ *value*? ?\ *option value ...*\ ?
  Queries or modifies the configuration options of the widget.  If no
  *option* is specified, this command returns a list describing all the
  available options for *pathName* (see **Tk_ConfigureInfo** for
  information on the format of this list).  If *option* is specified with
  no *value*, then a list describing the one named option (this list will
  be identical to the corresponding sublist of the value returned if no
  *option* is specified) is returned.  If one or more *option-value* pairs
  are specified, then this command modifies the given widget option(s) to
  have the given value(s); in this case the command returns an empty
  string.  *Option* and *value* are described below.

  Widget configuration options may be set either by the **configure**
  operation or the Tk **option** command.  The resource class is
  **BltScale**.  The resource name is the name of the widget::

    option add *BltScale.anchor n
    option add *BltScale.Anchor e

  The following widget options are available\:

  **-activebackground** *bgName*
    Specifies the background color of *pathName* when it is active.
    *bgName** may be a color name or the name of a background object
    created by the **blt::background** command.  The default is "grey90".

  **-activegripbackground** *bgName*
    Specifies the background color of the scale's grip when it is active.
    *bgName** may be a color name or the name of a background object
    created by the **blt::background** command.  The default is "grey90".

  **-activemaxarrowcolor** *colorSpec*
    Specifies the background color of the arrow representing scale's
    maximum range when it is active.  *ColorSpec** is be a color name or
    pixel value. The default is "grey90".

  **-activeminarrowcolor** *colorSpec*
    Specifies the background color of the arrow representing scale's
    minimum range when it is active.  *ColorSpec** is be a color name or
    pixel value. The default is "grey90".

  **-activegripbackground** *bgName* 
    Specifies the background color of *pathName* when it is active.
    *bgName** may be a color name or the name of a background object
    created by the **blt::background** command.  The default is "white".

  **-activerelief** *reliefName* 
    Specifies the relief color of *pathName* when it's state is active.
    This determines the 3-D effect for the widget.  *ReliefName* indicates
    how the widget should appear relative its background. Acceptable
    values are **raised**, **sunken**, **flat**, **ridge**, **solid**, and
    **groove**. For example, "raised" means the scale should appear to
    protrude.  The default is "raised".
    
  **-activevaluebackground** *bgName*
    Specifies the background color of the scale's value when it is active.
    *BgName** may be a color name or the name of a background object
    created by the **blt::background** command.  The default is "white".

  **-activevalueforeground** *colorName*
    Specifies the color of the text displaying the current value when it
    is active. *ColorName** is be a color name.  The default is "black".

  **-axislinecolor** *colorName*
    Specifies the color of the rectangle for the scale's axis.
    *ColorName** is be a color name.  The default is "grey30".

  **-axislinewidth** *numPixels* 
    Specifies the width of the line for the scale's axis.  *NumPixels* is a
    non-negative value indicating the axis.  *NumPixels* may have any of
    the forms acceptable to **Tk_GetPixels**.  The default is "0.03i".

  **-arrowon** *boolean* 
    Indicates if an arrow is to be displayed on the right side of the text.
    The default is "0".

  **-background** *bgName* 
    Specifies the background color of *pathName*.  *BgName** may be a
    color name or the name of a background object created by the
    **blt::background** command.  The default is "grey85".
    
  **-borderwidth** *numPixels* 
    Specifies the borderwidth of *pathName*.  *NumPixels* is a non-negative
    value indicating the width of the 3-D border drawn around the widget.
    *NumPixels* may have any of the forms acceptable to **Tk_GetPixels**.
    The default is "2".

  **-colorbarthickness** *numPixels* 
    Specifies the colorbar of *pathName*.  *NumPixels* is a non-negative
    value indicating the width of the color bar drawn aside the axis.
    *NumPixels* may have any of the forms acceptable to **Tk_GetPixels**.
    The default is "0.15i".

  **-command** *string* 
    Specifies a TCL command to be executed when the current value is changed.
    This typically done moving the grip or using the **set** operation.
    If *string* is "", then no command is invoked. The default is "".

  **-cursor** *cursorName* 
    Specifies the cursor to be used for the widget. *CursorName* may have
    any of the forms acceptable to **Tk_GetCursor**.  If *cursorName* is "",
    this indicates that the widget should defer to its parent for cursor
    specification.  The default is "".

  **-cursor** *cursorName* 
    Specifies the cursor to be used for the widget. *CursorName* may have
    any of the forms acceptable to **Tk_GetCursor**.  If *cursorName* is "",
    this indicates that the widget should defer to its parent for cursor
    specification.  The default is "".

  **-decreasing** *boolean*
    Indicates whether the values along the axis are monotonically
    increasing or decreasing.  If *boolean* is true the axis values will be
    decreasing.  The default is "0".

  **-disabledbackground** *bgName* 
    Specifies the background of *pathName* when it is disabled.  *BgName*
    may be a color name or the name of a background object created by the
    **blt::background** command.  The default is "grey90".

  **-disabledforeground** *colorName* 
    Specifies the color of the scale when *pathName* is disabled.  The
    default is "grey70".

  **-divisions** *numMajorTicks*
    Specifies the number of major ticks to be displayed.

  **-formatcommand** *cmdPrefix*
    Specifies a TCL command to be invoked when formatting the axis tick
    labels. *CmdPrefix* is a string containing the name of a TCL proc and
    any extra arguments for the procedure.  This command is invoked for
    each major tick on the axis.  Two additional arguments are passed to
    the procedure: the *pathName* and the current the numeric value of the
    tick.  The procedure returns the formatted tick label.  If "" is
    returned, no label will appear next to the tick.  You can get the
    standard tick labels again by setting *cmdPrefix* to "".  The default
    is "".

  **-gripbackground** *bgName*
    Specifies the normal background color of the scale's grip.
    *BgName** may be a color name or the name of a background object
    created by the **blt::background** command.  The default is "grey85".

  **-gripborderwidth** *numPixels* 
    Specifies the borderwidth of the grip.  *NumPixels* is a non-negative
    value indicating the width of the 3-D border drawn around the grip.
    *NumPixels* may have any of the forms acceptable to **Tk_GetPixels**.
    The default is "2".

  **-griprelief** *reliefName*
    Specifies the relief color of the scale's grip.  This determines the
    3-D effect for the grip.  *ReliefName* indicates how the grip should
    appear relative *pathName*. Acceptable values are **raised**,
    **sunken**, **flat**, **ridge**, **solid**, and **groove**. For
    example, "raised" means the grip should appear to protrude.  The
    default is "raised".

  **-height** *numPixels* 
    Specifies the height of the widget.  *NumPixels* is a non-negative
    value indicating the height of *pathName* in pixels.  It may have any of
    the forms accept able to **Tk_GetPixels**, such as "200" or "2.4i".  If
    *numPixels* is 0, then the height is computed from the size of the
    scale. The default is "0".

  **-hide** *partsList* 
    Specifies the parts named in *partsList* are hidden.  *PartsList* is a
    TCL list of part names.  Parts not listed are unaffected.  Use the
    **-show** option to display parts.
    The default is "".

  **-highlightbackground** *bgName*
    Specifies the color of the traversal highlight region when *pathName*
    does not have the input focus.  *BgName* may be a color name or the
    name of a background object created by the **blt::background** command.
    The default is "grey85".

  **-highlightcolor** *bgName*
    Specifies the color of the traversal highlight region when *pathName*
    has input focus.  *BgName* may be a color name or the name of a
    background object created by the **blt::background** command. The
    default is "black".

  **-highlightthickness** *numPixels*
    Specifies a non-negative value for the width of the highlight rectangle
    to drawn around the outside of the widget.  *NumPixels* may have any of
    the forms acceptable to **Tk_GetPixels**.  If *numPixels* is "0", no
    focus highlight is drawn around the widget.  The default is "0".

  **-labeloffset** *boolean* 

  **-loose** *boolean*
    Indicates whether the limits of the axis should fit the data points
    tightly, at the outermost data points, or loosely, at the outer tick
    intervals.  If the axis limit is set with the -min or -max option, the
    axes are displayed tightly.  If *boolean* is true, the axis range is
    "loose".  The default is "0".

  **-majorticks** *tickList*
    Specifies where to display major axis ticks.  You can use this option
    to display ticks at non-uniform intervals.  *TickList* is a list of
    coordinates along the axis designating where major ticks will be drawn.
    No minor ticks are drawn.  If *tickList* is "", major ticks will be
    automatically computed. The default is "".

  **-max** *maxValue*
    Specifies the maximum limit of *axisName*, clipping elements using
    *axisName*.  Any data point greater than *maxValue* is not displayed.
    If *maxValue* is "", the maximum limit is calculated using the largest
    value of all the elements mapped to *axisName*.  The default is "".

  **-min** *minValue*
    Specifies the minimum limit of *axisName*, clipping elements using
    *axisName*. Any data point less than *minValue* is not displayed.  If
    *minValue* is "", the minimum limit is calculated using the smallest
    value of all the elements mapped to *axisName*.  The default is "".

  **-minorticks** *tickList*
    Specifies where to display minor axis ticks.  You can use this option
    to display minor ticks at non-uniform intervals. *TickList* is a list
    of real values, ranging from 0.0 to 1.0, designating the placement of a
    minor tick.  No minor ticks are drawn if the **-majortick** option is
    also set.  If *tickList* is "" then the minor ticks are automatically
    computed. The default is "".


    Specifies an image to be displayed as an icon on the left of the text
    in *pathName*.  *ImageName* is the name of an Tk photo or BLT picture.
    If *imageName* is "", no icon is displayed. The default is "".

  **-palette** *paletteName*
    FIXME

  **-rangecolor** *colorName*
    Specifies the color of the rectangle for the scale's axis.
    *ColorName** is be a color name.  The default is "grey30".

  **-rangemax** *maxValue*
    Specifies the maximum limit of the range.  The current value of the
    scale can not be greater that this value.  If *maxValue* is "", the
    maximum of the scale (see the **-max** option).  The default is "".

  **-rangemin** *minValue*
    Specifies the minimum limit of the range.  The current value of the
    scale can not be less that this value.  If *minValue* is "", the
    minximum of the scale (see the **-min** option).  The default is "".

  **-relief** *reliefName* 
    Specifies the 3-D effect for *pathName*.  *ReliefName* indicates how
    the button should appear relative to the window it's packed
    into. Acceptable values are **raised**, **sunken**, **flat**,
    **ridge**, **solid**, and **groove**. For example, "raised" means
    *pathName* should appear to protrude.  The default is "raised".

  **-resolution** *value* 
    Specifies the resolution for the scale.  *Value* is a real number.  If
    this value is greater than 0 then the scale's value will always be
    rounded to an even multiple of this value, as will the minimum and
    maximum values of the range.  If the value is 0 or less, then no
    rounding occurs.  The default is "0".

  **-scale** *scaleValue*
    Specifies the scale of *axisName*. *ScaleValue* can be one of the
    following.

    **linear**
      Indicates that the scale of the axis is linear.  

    **log**
      Indicates that the scale of the axis is logarithmic.  

    **time**
      Indicates that the axis scale is time.  The data values on the axis
      are in assumed to be in seconds.  The tick values will be in
      displayed in a date or time format (years, months, days, hours,
      minutes, or seconds).

  **-show** *partsList* 
    Specifies the parts named in *partsList* are displayed.  *PartsList* is
    a TCL list of part names.  Parts not listed are unaffected.  Use the
    **-hide** option to hide parts.  The default is "".

  **-state** *stateName*
    Specifies one of three states for *pathName*: 

    **active**
      The scale associated with *pathName* is active.  *pathName* is
      displayed according to the **-activeforeground**, the
      **-activebackground**, and **-activerelief** options.

    **disabled**
      Disabled state means that *pathName* should be insensitive: the
      default bindings will not activate or invoke the scale.  In this
      state *pathName* is displayed according to the
      **-disabledforeground** and the **-disabledbackground** options.

    **normal**
      In normal state *pathName* is displayed using the **-foreground**
      **-background**, and **-relief**  options.

    The default is "normal".

  **-stepsize** *stepValue*
    Specifies the interval between major axis ticks.  If *stepValue* isn't
    a valid interval (it must be less than the axis range), the request is
    ignored and the step size is automatically calculated.

  **-subdivisions** *numDivisions*
    Indicates how many minor axis ticks are to be drawn.  For example, if
    *numDivisons* is two, only one minor tick is drawn.  If *numDivisions*
    is one, no minor ticks are displayed.  The default is "2".

  **-takefocus** *boolean*
    Provides information used when moving the focus from window to window
    via keyboard traversal (e.g., Tab and Shift-Tab).  If *boolean* is "0",
    this means that this window should be skipped entirely during keyboard
    traversal.  "1" means that the this window should always receive the
    input focus.  An empty value means that the traversal scripts make the
    decision whether to focus on the window.  The default is "".

  **-tickangle** *numDegrees*
    Specifies the how many degrees to rotate the tick labels.  *NumDegrees*
    is a real value representing the number of degrees to rotate the tick
    labels.  The default is "0.0".

  **-tickcolor** *colorName* 
    Specifies the color of the tick lines.  *ColorName** is be a color
    name.  The default is "black".

  **-tickdirection** *direction*
    Indicates whether the ticks are interior to the colorbar or exterior.
    *Direction* can be any of the following.
    
    **in**
      The ticks extend from the axis line into the colorbar.
    **out**
      The ticks extend away from the axis and colorbar.

    The default is "out".
    
  **-tickfont** *fontName*
    Specifies the font for tick labels. The default is "{Sans Serif} 9".

  **-ticklabelcolor** *colorName* 
    Specifies the color of the tick labels.  *ColorName** is be a color
    name.  The default is "black".

  **-ticklength** *numPixels*
    Specifies the length of major and minor ticks (minor ticks are 2/3 the
    length of major ticks). *NumPixels* may have any of the forms
    acceptable to **Tk_GetPixels**. The default is "8".

  **-ticklinewidth** *numPixels*
    Specifies the width of major and minor tick lines.  *NumPixels* may
    have any of the forms acceptable to **Tk_GetPixels**. The default is
    "1".

  **-title** *titleString*
    Specifies the title of *pathName*. If *titleString* is "", no title
    will be displayed.  The default is the "".

  **-titlecolor** *colorName*
    Specifies the color of the axis title. The default is "black".

  **-titlefont** *fontName*
    Specifies the font for axis title. The default is "{Sans Serif} 9".

  **-titlejustify**  *justifyName*
    Specifies how the title should be justified.  This matters only when
    the title is shorter than the width of *pathName*. *JustifyName* must
    be "left", "right", or "center".  The default is "center".

  **-valueangle** *numDegrees* 
    Specifies the how many degrees to rotate the value text.  *NumDegrees*
    is a real value representing the number of degrees to rotate the 
    text.  The default is "0.0".

  **-valuecolor** *colorName* 
    Specifies the color of the text displaying the current value.
    *ColorName** is be a color name.  The default is "black".

  **-valuefont** *fontName* 
    Specifies the font for the text displaying the current value. The
    default is "{Sans Serif} 6".

  **-variable** *varName* 
    Specifies the name of a global TCL variable that will be set to the
    current value of *pathName*.  If *varName* is set to another value, the
    current value in *pathName* will change accordingly. If *varName* is
    "", no variable is used. The default is "".

  **-width** *numPixels*
   Specifies the width of the widget*.  *NumPixels* is a non-negative value
   indicating the width of *pathName*. The value may have any of the forms
   accept able to **Tk_GetPixels**, such as "200" or "2.4i".  If
   *numPixels* is "0", the width of *pathName* is computed from its text.
   The default is "".

*pathName* **deactivate** 
  Redisplays *pathName* using its normal colors.  This typically is used by
  widget bindings to un-highlight *pathName* as the pointer is 
  moved away from the button.

*pathName* **get** *partName* *value*
  Gets the value of *partName*.  **PartName** may be **mark**,
  **maxarrow**, or **minarrow**.
  
*pathName* **identify**  *x* *y*  ?\ *switches* ... ?
  Returns the name of the part under the point given by *x* and *y* (such
  as **grip** or **minarrow**), or an empty string if the point does not
  lie in any part of the scale.  *X* and *y* are screen coordinates
  relative to the scale widget.  *Switches* may be any of the following.

  **-root**
    Indicates that *x* and *y* are root coordinates.  By default *x* and
    *y* are considered relative to the upper-left corner of *pathName*.

*pathName* **invtransform**  *x* *y*
  Transforms screen coordinates into a scale value.  *X* and *y* are
  integers representing a coordinate on the screen.  Returns a number
  representing the scale value at that coordinate.
  
*pathName* **limits** 
  Returns a list of the minimum and maximum values for *pathName*.  
  
*pathName* **set** *valueName*
  Invokes the TCL command specified by the **-command** option. 

*pathName* **set** *valueName*
  Sets the current value of the scale. *ValueName* is a real number or
  one of the following.

  **maxarrow**
    Set the current value to the range maximum.

  **minarrow**
    Set the current value to the range minumum.

*pathName* **transform** *value* 
  Transforms scale coordinate into a X or Y screen coordinates.  *Value* is
  a double precision number representing a value on the scale.  Returns a
  the X or Y screen coordinate, depending upon the orientation of the scale
  (see the **-orient** option).

DEFAULT BINDINGS
----------------

There are several default class bindings for *scale* widgets.

**<Enter>** 
  The button activates whenever the pointer passes over the button window.
**<Leave>**
  The button deactivates whenever the pointer leaves the button window.
**<ButtonPress-1>**
  Pressing button 1 over the scale posts its associated combomenu
  if one is specified. The relief  of the button  changes to raised and
  its associated menu is posted under the scale.

**<B1-Motion>**
  If the mouse is dragged down into the menu with the button still down,
  and if the mouse button is then released over an entry in the menu, the
  scale is unposted and the menu item is invoked.

  If button 1 is pressed over a scale and then dragged over some
  other scale, the original scale unposts itself and the new
  scale posts.

**ButtonRelease-1**
  If button 1 is pressed over a scale and then released over that
  scale, the scale stays posted: you can still move the mouse
  over the menu and click button 1 on an entry to invoke it.  Once a menu
  item has been invoked, the scale unposts itself.

  If button 1 is pressed over a scale and released outside any
  scale or menu, the scale unposts without invoking any menu
  item.

**Alt-KeyPress-**\ *key*
  If the **-underline** option has been specified then keyboard traversal
  may be used to post the scale's menu: Alt+\ *key*, where *key* is the
  underlined character (or its lower-case or upper-case equivalent), may be
  typed in any window under the scale's toplevel to post the
  scale's menu.

**KeyPress-Space** or  **KeyPress-Return**
   If a scale has the input focus, the space and  return  keys
   post the scale's menu.

The behavior of scales can be changed by defining new bindings for
individual widgets or by redefining the class bindings.

EXAMPLE
-------

You create a *scale* widget with the **blt::scale** command.

 ::

    package require BLT

    # Create a new scale and combomenu.

    blt::scale .file \
	-textvariable "myTextVar" \
	-iconvariable "myIconVar" \
	-menu .file.m 

    blt::combomenu .file.m  \
	-textvariable "myTextVar" \
	-iconvariable "myIconVar"
    .file.m add -text "New" -accelerator "Ctrl+N" -underline 0 \
        -icon $image(new_window)

Please note the following:

1. The  **blt::combomenu** widget doesn't have to already exist when you
   specify the **-menu** option.

2. Both the **blt::combomenu** and **blt::scale** widgets have
   **-textvariable** and **-iconvariable** options that let them change
   the text and icon through TCL variables.

3. You can't use a Tk **menu** with *scale* widgets.  The menu must
   be a **blt::combomenu** widget.

KEYWORDS
--------

scale, widget

COPYRIGHT
---------

2015 George A. Howlett. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

 1) Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
 2) Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in
    the documentation and/or other materials provided with the distribution.
 3) Neither the name of the authors nor the names of its contributors may
    be used to endorse or promote products derived from this software
    without specific prior written permission.
 4) Products derived from this software may not be called "BLT" nor may
    "BLT" appear in their names without specific prior written permission
    from the author.


THIS SOFTWARE IS PROVIDED ''AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
