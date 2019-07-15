#!/usr/bin/python3
#-*-coding:utf-8-*-

import json
import sys
import numpy as np
from tablet import *

#Check length of the command line arguments
if len(sys.argv) != 2:
    print("Run ./extractCHI20Data.py <jsonPath>")
    sys.exit(0)

print("Opening {}".format(sys.argv[1]))

tabletData = [TabletData(0, None), TabletData(1, None)]

try:
    with open(sys.argv[1]) as f:
        try:
            jsonData = json.load(f) #The JSON data

            currentTrial          = -1   #current trial ID
            currentTargetPosition = None #current target position for this trial
            currentTabletID       = -1   #current tablet ID for this trial
            currentTrialOffset    = 0    #time offset for when this trial started

            #Look over all the json object saved by the server
            for obj in jsonData["data"]:
                name      = obj["type"]
                offset    = obj["timeOffset"]
                sender    = obj["sender"]
                headsetIP = obj["headsetIP"].split(':')

                #Track the tablets' ID 
                if name == "HeadsetBindingInfo" and headsetIP[1] == "Tablet":
                    if bool(obj["tabletConnected"]):
                        #Update the tablet information
                        tabletID = int(obj["tabletID"])
                        ip       = headsetIP[0]

                        for tablet in tabletData:
                            if tablet.tabletID == tabletID:
                                tablet.headsetIP = ip
                                break

                #Track the trial ID and annotation's target position
                elif name == "SendNextTrial":
                    currentTrial          = obj["currentTrialID"]
                    currentTargetPosition = np.array(obj["annotationPos"]) 
                    currentTabletID       = obj["currentTabletID"]
                    currentTrialOffset    = offset
                
                #When we the study is running (i.e., no training)
                if currentTrial != -1:
                    #Track when an annotation started
                    if name == "StartAnnotation":
                        pointingID    = obj["pointingID"]
                        for tablet in tabletData:
                            if tablet.headsetIP == headsetIP[0] and tablet.tabletID == currentTabletID:
                                tablet.initAnnotation(1, currentTrial, pointingID, currentTargetPosition, currentTrialOffset, offset)
                                break

                    #Anchor the annotation
                    elif name == "AnchorAnnotation":
                            for tablet in tabletData:
                                if tablet.headsetIP == headsetIP[0] and tablet.tabletID == currentTabletID:
                                    anchorPos = np.array(obj["localPos"])
                                    tablet.commitAnnotation(anchorPos, offset)
                                    break


        except json.JSONDecodeError as jsonErr:
            print("Could not parse the json file. Error : {0}".format(jsonErr))
except IOError as err:
    print("Could not open the file {0}. Error : {1}".format(sys.argv[1], err))
    sys.exit(-1)

print(tabletData)
