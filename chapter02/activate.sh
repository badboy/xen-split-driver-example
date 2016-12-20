#!/bin/bash

DOMU_ID=$1

if [ -z "$DOMU_ID"  ]; then
  echo "Usage: $0 [domU ID]]"
  echo
  echo "Connects the new device, with dom0 as backend, domU as frontend"
  exit 1
fi

DEVICE=mydevice
DOMU_KEY=/local/domain/$DOMU_ID/device/$DEVICE/0
DOM0_KEY=/local/domain/0/backend/$DEVICE/$DOMU_ID/0

# Tell the domU about the new device and its backend
xenstore-write $DOMU_KEY/backend-id 0
xenstore-write $DOMU_KEY/backend "/local/domain/0/backend/$DEVICE/$DOMU_ID/0"

# Tell the dom0 about the new device and its frontend
xenstore-write $DOM0_KEY/frontend-id $DOMU_ID
xenstore-write $DOM0_KEY/frontend "/local/domain/$DOMU_ID/device/$DEVICE/0"

# Make sure the domU can read the dom0 data
xenstore-chmod $DOM0_KEY r0 b$DOMU_ID
xenstore-chmod $DOMU_KEY r$DOMU_ID b0

# Activate the device, dom0 needs to be activated last
xenstore-write $DOMU_KEY/state 1
xenstore-write $DOM0_KEY/state 1
