#!/usr/bin/env sh


lock() {
sketchybar --bar y_offset=-64 \
				 margin=-500 \
				 color=002e3440 \
		   --set spot_logo y_offset=64 
}

unlock() {
sketchybar --animate sin 30 --bar y_offset=0 \
           --animate sin 80 --bar color=0xff2e3440 \
		   --animate sin 40 --bar margin=0 \
		   --animate sin 60 --set spot_logo y_offset=0
}

case "$SENDER" in
  "lock") lock
  ;;
  "unlock") unlock
  ;;
esac
