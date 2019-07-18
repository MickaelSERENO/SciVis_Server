#!/usr/bin/python3
#-*-coding:utf-8-*-

import json
import sys
import functools
import math
import numpy as np
import matplotlib.pyplot as plt

from tablet import *
from bootstrap import *
from plot import *

#Check length of the command line arguments
if len(sys.argv) < 2:
    print("Run ./extractCHI20Data.py <jsonPath1> [jsonPathN...]")
    sys.exit(0)

#Parse all the files
pairData = {}
try:
    for fileName in sys.argv[1:]:
        print("Opening {}".format(fileName))
        with open(fileName, "r") as f:
            try:
                tabletData = [TabletData(0, None), TabletData(1, None)]
                jsonData = json.load(f) #The JSON data

                currentTrial          = -1   #current trial ID
                currentTargetPosition = None #current target position for this trial
                currentTabletID       = -1   #current tablet ID for this trial
                currentTrialOffset    = 0    #time offset for when this trial started
                currentDatasetScaling = np.array([1.0, 1.0, 1.0]) #The current dataset scaling

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
                    elif name == "SendNextTrial" and sender == "Server":
                        currentTrial          = obj["currentTrialID"]
                        currentTargetPosition = np.array(obj["annotationPos"]) 
                        currentTabletID       = obj["currentTabletID"]
                        currentTrialOffset    = offset

                    elif name == "ScaleDataset": #Take account of the scaling
                        if obj["datasetID"] == 0 and obj["subDatasetID"] == 0 and sender == "Server" and obj["inPublic"] == 1: #Correct dataset, correct sender, in public space
                            currentDatasetScaling = np.array(obj["scale"])
                    
                    #When the study is running (i.e., no training)
                    if currentTrial != -1:
                        #Track when an annotation started
                        #This will initialize a new annotation. We track the server sending message.
                        if name == "StartAnnotation" and sender == "Server":
                            if obj["datasetID"] == 0 and obj["subDatasetID"] == 0 and obj["inPublic"] == 1: #Only the main dataset counts 
                                pointingID    = obj["pointingID"]
                                for tablet in tabletData:
                                    if tablet.headsetIP == headsetIP[0] and tablet.tabletID == currentTabletID:
                                        tablet.initAnnotation(1, currentTrial, pointingID, currentTargetPosition, currentTrialOffset, offset)
                                        break

                        #Anchor the annotation
                        #This will finalize the initialized annotation. We track the server sending message.
                        elif name == "AnchorAnnotation" and sender == "Server":
                            if obj["datasetID"] == 0 and obj["subDatasetID"] == 0 and obj["inPublic"] == 1: #Only the main dataset counts
                                for tablet in tabletData:
                                    if tablet.headsetIP == headsetIP[0] and tablet.tabletID == currentTabletID:
                                        anchorPos = np.array(obj["localPos"])
                                        tablet.commitAnnotation(anchorPos, offset, currentDatasetScaling)
                                        break

                pairData[fileName] = tabletData
            except json.JSONDecodeError as jsonErr:
                print("Could not parse the json file. Error : {0}".format(jsonErr))
except IOError as err:
    print("Could not open the file {0}. Error : {1}".format(sys.argv[1], err))
    sys.exit(-1)

def getAnnotationsStudy2(pair):
    """Get the list of the annotations for a pair of participants in study 2
    @param pair the pair of participant"""

    annotations =  functools.reduce(lambda x, y : x + y, [y.annotations + z.annotations for y, z in zip(pair[0].study1Data, pair[1].study1Data)], [])
    return annotations

def computeStudy2PointingTechniqueData(annots, pointingID):
    """Compute the data for a giving pointing technique.
    @param annots the list of annotations
    @param pointingID the pointingID to look at
    @return a tuple (acc_avg, acc_std, tct_trial_avg, tct_trial_std, tct_annot_avg, tct_annot_std). 
    Each value of the tuple contains a list containing two values. The first one containing the annotation TCT and the second one containing the trial TCT"""
    #Accuracy in World Space
    accs = [ann.worldAccuracy for ann in annots if ann.pointingID==pointingID]
    accBootstrap = bootstrap(accs, 5000)
    accAvg, accStd = getMeanAndStd(accBootstrap(0.95))

    #TCT from the start of the annotation
    annotTCTs = [ann.annotTCT for ann in annots if ann.pointingID==pointingID]
    annotTCTBootstrap = bootstrap(annotTCTs, 5000)

    #TCT from the start of the trial
    trialTCTs = [ann.trialTCT for ann in annots if ann.pointingID==pointingID]
    trialTCTBootstrap = bootstrap(trialTCTs, 5000)

    #Avg and standard deviation arrays
    tctBootstrap = [annotTCTBootstrap(0.95), trialTCTBootstrap(0.95)]
    tctAvgs, tctStds = getMeansAndStds(tctBootstrap)

    return (accAvg, accStd,        #Avg
           tctAvgs[1], tctStds[1], #Trial TCT
           tctAvgs[0], tctStds[0]) #Annot TCT

def computeStudy2Data(pairData):
    """Compute the data for the second part of the study
    This will generate a list of PDFs permitting to visualize the dataset
    @param pairData the data of pairs of participants"""

    #Get the list of annotations
    annots = functools.reduce(lambda x, y: x+y, [getAnnotationsStudy2(pair) for pair in pairData.values()], [])
    nbData = len(annots)

    #Get the pointingData and the maximum axis for the TCT values
    pointingData = {}
    maxAxisTCT = 0
    maxAxisAcc = 0
    pointingITs = [POINTINGID_GOGO, POINTINGID_WIM, POINTINGID_MANUAL]
    for p in pointingITs:
        data = computeStudy2PointingTechniqueData(annots, p)
        pointingData[p] = data
        maxAxisTCT = max(maxAxisTCT, data[2]+data[3]) #Compute the maximum TCT axis length based on the trial parameter (which is bigger than the annotation parameter)
        maxAxisAcc = max(maxAxisAcc, data[0]+data[1]) #Compute the maximum Acc axis length

    #Print the PDFs
    for p in pointingITs:
        #Print the time completion task graphs
        tctAvgs = [pointingData[p][4], pointingData[p][2]]
        tctStds = [pointingData[p][5], pointingData[p][3]]
        drawCIs("tct_{}.pdf".format(p), tctAvgs, tctStds, ["Annotation TCT", "Trial TCT"], "#CCCCCC", maxAxis=maxAxisTCT)
        drawCIs("acc_{}.pdf".format(p), [pointingData[p][0]], [pointingData[p][1]], ["Accuracy"], "#CCCCCC", maxAxis=maxAxisAcc)

computeStudy2Data(pairData)
