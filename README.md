This is a command line utility to easily fade Xorg desktop backgrounds

### Requirements
- gcc
- Xlib, libXrender, libXrandr
- A connection to an X server with the Xrender and Xrandr extensions

### Building 
    git clone https://github.com/jackyliao123/bgfade
    make
### Examples
Set all monitor backgrounds randomly

    ./bgfade *.jpg

Set a solid colour background

    ./bgfade -c "#ff00ff"

Change fill mode to fit, to not cut off any parts of the image

    ./bgfade -x fit *.jpg

Change fade duration and framerate

    ./bgfade -f 144 -d 0.5 *.jpg

Disable fade

    ./bgfade -d 0 *.jpg

Per-image settings (`a.jpg` will match width of target, `b.jpg` and `c.jpg` will match height of target. In addition, `c.jpg` will use nearest neighbor filtering when scaling)

    ./bgfade -x match-width a.jpg -x match-height b.jpg -filter nearest c.jpg

Set `a.jpg` onto output `DP-1`, `b.jpg` onto output `DP-2` and `c.jpg` onto output `DP-3` (2 ways)

    ./bgfade -m DP-1 a.jpg -n -m DP-2 b.jpg -n DP-3 c.jpg

    ./bgfade -a seq -m DP-1 -m DP-2 -m DP-3 a.jpg b.jpg c.jpg

Display random images from one set onto `DP-1` and another onto `DP-2`

    ./bgfade -m DP-1 a/*.jpg -n -m DP-2 b/*.jpg

More detailed documentation can be found in [help.txt](help.txt).
