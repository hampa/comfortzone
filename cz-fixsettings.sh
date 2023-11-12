#!/bin/sh

FILE=/Users/$USER/Documents/Rack2/settings.json

gron $FILE | grep -v Comfortzone > 1.gron  || exit 1
echo "json.moduleWhitelist.Comfortzone = true;" >> 1.gron || exit 1
gron -u 1.gron > $FILE || exit 1
echo done
