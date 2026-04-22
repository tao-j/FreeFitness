#! /bin/bash
# --------------------------------------------------------
# Script simulates a Keiser M3i bike using HCI device 0.
# --------------------------------------------------------
# Two broadcast rates are available, depending on the 
# version of bike that is to be simulated.
# --------------------------------------------------------

#RATE="80 0C" # 2 Seconds - For bikes < 6.30
RATE="37 02" # .357375 Seconds - For bikes > 6.30

MAJOR="06"
MINOR="24"
DATATYPE="00"
RPM="00 00"
HR="00 00"
POWER="00 00"
KCAL="00 00"
MINUTES="00"
SECS="00"
TRIP="00 00"
GEAR="01"

BIKEID="01"

DECPOWER=0

function init {
    hciconfig hci0 down > /dev/null;
    hciconfig hci0 up > /dev/null;
    hcitool -i hci0 cmd 0x08 0x000A 00 > /dev/null;
    hcitool -i hci0 cmd 0x08 0x0006 $RATE  $RATE  03  00  00  00 00 00 00 00 00 07 00 > /dev/null;
    set_broadcast;
    hcitool -i hci0 cmd 0x08 0x000A 01 > /dev/null;
}

function set_broadcast {
    hcitool -i hci0 cmd 0x08 0x0008 1C 03 09 4D 33 02 01 04 14 FF 02 01 $MAJOR $MINOR $DATATYPE $BIKEID $RPM $HR $POWER $KCAL $MINUTES $SECS $TRIP $GEAR > /dev/null;
}

function run {
    init
    while true; do
        sleep 1.99
        DECPOWER=$[$DECPOWER + 2]
        if [ "$DECPOWER" -ge 255 ]; then
            DECPOWER=0;
        fi
        POWER="$(printf "%02x" $DECPOWER) 00"
        set_broadcast
    done
}

run;