#!/bin/bash
export TEXTDOMAIN=raindrop

. gettext.sh

zenity --password --title "$(gettext "Password Required")"

