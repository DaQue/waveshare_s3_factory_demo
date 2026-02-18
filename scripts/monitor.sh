#!/bin/bash
# Interactive serial monitor for the ESP32-S3 weather station.
# Exit: Ctrl+A then X
# If the port is busy: killall minicom

PORT="${1:-/dev/ttyACM0}"
BAUD="${2:-115200}"

if [ ! -e "$PORT" ]; then
    echo "Port $PORT not found. Is the device connected?"
    exit 1
fi

# Create a temp minicom config with echo + add-linefeed
TMPRC=$(mktemp /tmp/minirc.XXXXXX)
cat > "$TMPRC" <<EOF
pu port             $PORT
pu baudrate         $BAUD
pu addlinefeed      Yes
pu localecho        Yes
pu linewrap         Yes
EOF

echo "Connecting to $PORT at $BAUD baud (echo=on, add-lf=on)"
echo "Type 'help' for commands. Exit: Ctrl+A then X"
echo ""

minicom -c on -D "$PORT" -b "$BAUD" -w -8 2>/dev/null
# Note: if echo/LF don't work automatically, inside minicom:
#   Ctrl+A then E = toggle local echo
#   Ctrl+A then A = toggle add linefeed

rm -f "$TMPRC"
