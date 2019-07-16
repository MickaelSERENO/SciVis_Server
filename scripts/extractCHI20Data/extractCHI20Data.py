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

                print(tabletData)
                pairData[fileName] = tabletData
            except json.JSONDecodeError as jsonErr:
                print("Could not parse the json file. Error : {0}".format(jsonErr))
except IOError as err:
    print("Could not open the file {0}. Error : {1}".format(sys.argv[1], err))
    sys.exit(-1)


#Compute our data

def getAnnotationsStudy2(pair):
    """Get the list of the annotations for a pair of participants in study 2
    @param pair the pair of participant"""

    annotations =  functools.reduce(lambda x, y : x + y, [y.annotations + z.annotations for y, z in zip(pair[0].study1Data, pair[1].study1Data)], [])
    return annotations

############## Accuracy ################
def computeAccuracySum(annots):
    """Get sum of the accuracy for a pair of participant
    @param annots list of annotations"""
    return functools.reduce(lambda x, y: x+y.accuracy, annots, 0)

def computeAccuracyStdSum(annots, avg):
    """Get sum of the standard deviation of the accuracy for a pair of participant
    @param annots list of annotations
    @param avg the average value computed"""
    return (avg-functools.reduce(lambda x, y: x+y.accuracy, annots, 0))**2

############# Annotation TCT ################
def computeAnnotTCTSum(annots):
    """Get sum of the time completion task starting from the 'StartAnnotation' message for a pair of participant
    @param annots list of annotations"""
    return functools.reduce(lambda x, y: x+y.annotTCT, annots, 0)

def computeAnnotTCTStdSum(annots, avg):
    """Get sum of the standard deviation of the time completion task starting from the 'StartAnnotation' message for a pair of participant
    @param annots list of annotations
    @param avg the average value computed"""
    return (avg-functools.reduce(lambda x, y: x+y.annotTCT, annots, 0))**2

############# Trial TCT ################
def computeTrialTCTSum(annots):
    """Get sum of the time completion task starting from the beginning of the trial for a pair of participant
    @param annots list of annotations"""
    return functools.reduce(lambda x, y: x+y.trialTCT, annots, 0)

def computeTrialTCTStdSum(annots, avg):
    """Get sum of the standard deviation of the time completion task starting from the beginning of the trial for a pair of participant
    @param annots list of annotations
    @param avg the average value computed"""
    return (avg - functools.reduce(lambda x, y: x+y.trialTCT, annots, 0))**2

def drawCIs(path, avgs, cis, xLabels, barColor, nbData, distance=0.05, width=0.2):
    """Draw the confidence intervale of a data both on screen and in a file
    @param path the path to save the file
    @param avgs array of average. Each value is a different bar chart
    @param cis  array of confidence interface. Can be shape(N,) (CIs symmetrical) or shape(N,2) (assymetrical CIs)
    @param xLabels the labels to use per bar
    @param distance the distance separating the bars
    @param width the bars width"""
    positions = [x*(width+distance) for x in range(len(avgs))]

    ax = plt.subplots()[1]
    ax.barh(positions, avgs, height=width, color=barColor)
    ax.errorbar(avgs, positions, xerr=cis, color='#000000', ls = 'none', lw=2, capthick=2, capsize=5, marker='o')

    plt.yticks(positions, xLabels)
    plt.savefig(path, format='pdf', bbox_inches='tight')
    plt.show()

def computeStudy2Data(pairData):
    """Compute the data for the second part of the study
    @param pairData the data of pairs of participants"""
    annotationsStudy2 = functools.reduce(lambda x, y: x+y, [getAnnotationsStudy2(pair) for pair in pairData.values()], [])
    nbData            = len(annotationsStudy2)

    #Accuracy
    accs = [ann.accuracy for ann in annotationsStudy2]
    accBootstrap = bootstrap(accs, 5000)

    #TCT from the start of the annotation
    annotTCTs = [ann.annotTCT for ann in annotationsStudy2]
    annotTCTBootstrap = bootstrap(annotTCTs, 5000)

    #TCT from the start of the trial
    trialTCTs = [ann.trialTCT for ann in annotationsStudy2]
    trialTCTBootstrap = bootstrap(trialTCTs, 5000)

    #Avg and standard deviation arrays
    tctBootstrap = [annotTCTBootstrap(0.95), trialTCTBootstrap(0.95)]
    tctAvgs = [(x+y)/2 * 1e-3 for x, y in tctBootstrap] #Convert from us to ms
    tctStds = [(y-x)/2 * 1e-3 for x, y in tctBootstrap] #Convert from us to ms

    print(tctAvgs)

    drawCIs("tct.pdf", tctAvgs, tctStds, ["Annotation TCT", "Trial TCT"], "#CCCCCC", nbData)

computeStudy2Data(pairData)
