#!/bin/bash
#This script converts the flyby help text to an appropriate manpage format.

../build/flyby --help | sed -r 's/\s+([\-|\-\-]\S+)\s+/\n\\fB\1\\fP\n/'
