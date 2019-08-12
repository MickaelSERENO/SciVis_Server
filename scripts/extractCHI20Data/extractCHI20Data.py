#!/usr/bin/python3
#-*-coding:utf-8-*-

import json
import sys
import functools
import math
import numpy as np
import matplotlib.pyplot as plt
import csv
import os

from tablet import *
from bootstrap import *
from plot import *


#Parse all the files
pairData  = {}
files     = []
outputDir = os.getcwd()

argv = sys.argv[1:]
i    = 0

while i < len(argv):
    #Read file names
    arg = argv[i]
    if not arg.startswith("--"):
        files.append(arg)
        i+=1
    else:
        while i < len(argv):
            arg = argv[i]
            if arg == "--output":
                if i < len(argv)-1:
                    outputDir = argv[i+1]
                    i+=1
                else:
                    print("Missing directory path value to '--output' parameter")
                    sys.exit(-1)
            else:
                print("Unknown parameter {}".format(arg))
                sys.exit(-1)
            i+=1

#Check length of the command line arguments
if len(files) == 0:
    print("Run ./extractCHI20Data.py <jsonPath1> [jsonPathN...] [--output dirOutput] [--show]")
    sys.exit(-1)

try:
    for fileName in files:
        print("Opening {}".format(fileName))
        with open(fileName, "r") as f:
            try:
                tabletData = [TabletData(0, None), TabletData(1, None)]
                jsonData = json.load(f) #The JSON data

                currentStudyID        = -1
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

                    if name == "SetPairID":
                        pairID = obj["pairID"]
                        for t in tabletData:
                            t.pairID = pairID

                    #Track the trial ID and annotation's target position
                    elif name == "SendNextTrial" and sender == "Server":
                        currentTrial          = obj["currentTrialID"]
                        currentTargetPosition = np.array(obj["annotationPos"]) 
                        currentTabletID       = obj["currentTabletID"]
                        currentTrialOffset    = offset
                        currentStudyID        = obj["currentStudyID"]

                    elif name == "ScaleDataset": #Take account of the scaling
                        if obj["datasetID"] == 0 and obj["subDatasetID"] == 0 and sender == "Server" and obj["inPublic"] == 1: #Correct dataset, correct sender, in public space
                            currentDatasetScaling = np.array(obj["scale"])
                    
                    #When the study is running (i.e., no training)
                    if currentTrial != -1 and currentStudyID != -1:
                        #Track when an annotation started
                        #This will initialize a new annotation. We track the server sending message.
                        if name == "StartAnnotation" and sender == "Server":
                            if obj["datasetID"] == 0 and obj["subDatasetID"] == 0 and obj["inPublic"] == 1: #Only the main dataset counts 
                                pointingID = obj["pointingID"]
                                for tablet in tabletData:
                                    if tablet.headsetIP == headsetIP[0] and tablet.tabletID == currentTabletID:
                                        tablet.initAnnotation(currentStudyID, currentTrial, pointingID, currentTargetPosition, currentTrialOffset, offset)
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
                sys.exit(-1)
except IOError as err:
    print("Could not open the file {0}. Error : {1}".format(sys.argv[1], err))
    sys.exit(-1)

def getAnnotationsStudy2(pair):
    """Get the list of the annotations for a pair of participants in study 2
    @param pair the pair of participant"""

    annotations =  functools.reduce(lambda x, y : x + y, [y.annotations + z.annotations for y, z in zip(pair[0].study2Data, pair[1].study2Data)], [])
    return annotations

def computeStudy2Data(pairData, pointingID):
    """Compute the data for a giving pointing technique.
    @param pairData the data of pairs (dict)
    @param pointingID the pointingID to look at
    @return a tuple (pIDs, accs, annotTCTs, trialTCTs)"""

    #Annotations
    annots = functools.reduce(lambda x, y: x+y, [getAnnotationsStudy2(pair) for pair in pairData.values()], [])

    #Participant IDs
    pIDs = [ann.pID for ann in annots if ann.pointingID==pointingID]

    #Trial IDs
    trialIDs = [ann.trialID for ann in annots if ann.pointingID==pointingID]

    #Accuracy in World Space
    accs = [ann.worldAccuracy for ann in annots if ann.pointingID==pointingID]

    #TCT from the start of the annotation
    annotTCTs = [ann.annotTCT for ann in annots if ann.pointingID==pointingID]

    #TCT from the start of the trial
    trialTCTs = [ann.trialTCT for ann in annots if ann.pointingID==pointingID]

    return (np.array(pIDs), np.array(trialIDs), np.array(accs), np.array(annotTCTs), np.array(trialTCTs))

def computeBootstrap(data, pointingID):
    """Compute the 95% Bootstrap CI from a list of annotations
    @param data tuple of np.array containing (accs, annotTCTs, trialTCTs)
    @param pointingID the pointingID to look at
    @return a tuple (acc_avg, acc_std, tct_trial_avg, tct_trial_std, tct_annot_avg, tct_annot_std). 
    Each value of the tuple contains a list containing two values. The first one containing the annotation TCT and the second one containing the trial TCT"""

    accs, annotTCTs, trialTCTs = data
    
    #Accuracy
    accBootstrap = bootstrap(accs, 5000)
    accAvg, accStd = getMeanAndStd(accBootstrap(0.95))

    #TCT
    annotTCTBootstrap = bootstrap(annotTCTs, 5000)
    trialTCTBootstrap = bootstrap(trialTCTs, 5000)
    tctBootstrap = [annotTCTBootstrap(0.95), trialTCTBootstrap(0.95)]
    tctAvgs, tctStds = getMeansAndStds(tctBootstrap)

    return (accAvg, accStd,        #Avg
           tctAvgs[1], tctStds[1], #Trial TCT
           tctAvgs[0], tctStds[0]) #Annot TCT

def study2Pipeline(pairData):
    """Compute the data for the second part of the study
    This will generate a list of PDFs permitting to visualize the dataset
    @param pairData the data of pairs of participants"""

    #Get the pointingData and the maximum axis for the TCT values
    pointingData = {}
    maxAxisTCT = 0
    maxAxisAcc = 0
    pointingITs = [POINTINGID_GOGO, POINTINGID_WIM, POINTINGID_MANUAL]
    for p in pointingITs:
        data = computeStudy2Data(pairData, p)
        csvFilePath = "{}/pointing_{}.csv".format(outputDir, p)
        print("Saving {}".format(csvFilePath))
        with open(csvFilePath, "w") as csvFile:
            writer = csv.writer(csvFile, delimiter=',')
            writer.writerow(["pID", "trialID", "acc", "annotTCT", "trialTCT"])
            for r in zip(*data):
                writer.writerow(r)

        cis  = computeBootstrap(data[-3:], p)

        pointingData[p] = cis
        maxAxisTCT = max(maxAxisTCT, cis[2]+cis[3]) #Compute the maximum TCT axis length based on the trial parameter (which is bigger than the annotation parameter)
        maxAxisAcc = max(maxAxisAcc, cis[0]+cis[1]) #Compute the maximum Acc axis length

    #Print the PDFs
    for p in pointingITs:
        #Print the time completion task graphs
        tctAvgs = [pointingData[p][4], pointingData[p][2]]
        tctStds = [pointingData[p][5], pointingData[p][3]]

        filePathTCT = "{}/tct_{}.pdf".format(outputDir, p)
        filePathAcc = "{}/acc_{}.pdf".format(outputDir, p)

        print("Saving {}".format(filePathTCT))
        drawCIs(filePathTCT, tctAvgs, tctStds, ["Annotation TCT", "Trial TCT"], "#CCCCCC", maxAxis=maxAxisTCT)
        print("Saving {}".format(filePathAcc))
        drawCIs(filePathAcc, [pointingData[p][0]], [pointingData[p][1]], ["Accuracy"], "#CCCCCC", maxAxis=maxAxisAcc)

study2Pipeline(pairData)
