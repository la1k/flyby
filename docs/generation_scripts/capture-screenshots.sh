#!/bin/bash

#Script for starting flyby and simulating keypresses in the right sequence,
#so that the captured screenshots correspond to the screenshots displayed
#in the usage guide.

#Not very robuts and might be subject to change in keypress sequence, but should be apparent
#where the wrong screen is captured by comparing the results against the images
#in ../usage_images/. In this case, locate filename of wrong screen within the script
#and check the written steps before the screen against the manual, correct steps for producing the same screen.

#Start flyby in a new terminal window and get the window ID
function start_flyby() {
	x-terminal-emulator -e flyby $1 &
	pid=$!
	sleep 2
	winid=$(xdotool search --onlyvisible --pid $pid)
	echo $winid
}

#Resize the window, used for having a slightly larger terminal for transponder editor
#and whitelist editor.
function resize_window() {
	xdotool windowsize --usehints $winid 80 30
}

#Kill flyby
function kill_flyby() {
	sleep 1
	killall flyby
}

tempdir=$(mktemp -d)

#Capture screenshot
function capture_screenshot() {
	filename="$1"
	xdotool windowactivate $winid
	sleep 1
	scrot -u $filename
	sleep 1
}

#set XDG_DATA_HOME to unlikely directory, so that no TLEs are defined
export XDG_DATA_HOME="/home/maxmekker/.local/share"

#empty directory as config directory, so that we are ensured that whitelist and qth file is non-existing
export XDG_CONFIG_HOME="$tempdir"

#use current flyby settings as system-wide config
export XDG_CONFIG_DIRS="$HOME/.config"

winid=$(start_flyby)

#capture initial screens during first time use
capture_screenshot ground_station.png
xdotool key Escape

#multitrack with no satellites
capture_screenshot multitrack_nosat.png
xdotool key W

#whitelisting with no satellites
capture_screenshot enabledisable_nosat.png

kill_flyby

#Note that after this, we have a defined QTH in our XDG_CONFIG_HOME, so
#flyby won't ask for new QTH coordinates. If we comment out the above steps (or
#subsequent steps), flyby will ask for coordinates again.

#capture screens with satellites enabled
export XDG_DATA_HOME="$tempdir"

#get some TLEs and transponder entries
wget http://www.celestrak.com/NORAD/elements/amateur.txt -P /tmp/
flyby-satnogs-fetcher /tmp/satnogs-db
mkdir -p $tempdir/flyby/tles
cp /tmp/amateur.txt $tempdir/flyby/tles
winid=$(start_flyby)
resize_window
xdotool key W

#whitelisting with satellites
capture_screenshot enabledisable.png
xdotool key a
capture_screenshot enabledisable_2.png
xdotool key q
xdotool key q #quit flyby in order to display multitrack with default terminal size

#multitrack with satellites
winid=$(start_flyby)
xdotool key Down
xdotool key Down
capture_screenshot multitrack.png

#multitrack settings, input 40 as elevation threshold
xdotool key M
xdotool key Down
xdotool key Down
xdotool key 4
xdotool key 0
capture_screenshot multitrack_options.png
xdotool key Return

#multitrack with filtering
capture_screenshot multitrack_filtering.png

#select single track
xdotool key Right
capture_screenshot multitrack_submenu.png
xdotool key Return
capture_screenshot singletrack_basic.png

#select transponder editor
xdotool key q
xdotool key Right
xdotool key Down
xdotool key Down
xdotool key Down
xdotool key Down
capture_screenshot multitrack_submenu_2.png
xdotool key Return
capture_screenshot transponder_selector.png
kill_flyby

#cheat and use satnogs database as transponder database
flyby-transponder-dbutil --silent --force-changes --add-transponder-file=/tmp/satnogs-db
winid=$(start_flyby)

#display transponder editor with entries
resize_window
xdotool key Down
xdotool key Down
xdotool key Right
xdotool key Down
xdotool key Down
xdotool key Down
xdotool key Down
xdotool key Return
xdotool key Return
capture_screenshot transponder_editor.png
xdotool key Escape
capture_screenshot transponder_display.png
xdotool key q
kill_flyby
winid=$(start_flyby)

#display singletrack with transponders
xdotool key T
capture_screenshot singletrack_transponders.png
kill_flyby

#start with rotctld, display program info and singletrack window
rotctld -m 1 &
winid=$(start_flyby -Alocalhost)
xdotool key I
capture_screenshot program_info.png
xdotool key q
xdotool key T
capture_screenshot singletrack_autotracking.png

kill_flyby
rm -rf $tempdir
