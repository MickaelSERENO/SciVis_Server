import numpy as np

class Annotation:
    """Represent an annotation"""
    def __init__(self, trialID, pointingID, targetPos, anchorPos, tct, trialTCT):
        """Constructor.
        @param trialID the id of the trial
        @param targetPos the true position of the annotation (what should have been the best result?). Numpy array
        @param anchorPos the position the user put the annotation. Numpy array
        @param tct time completion time from start anchoring to end anchoring
        @param trialTCT the time completion time from the start of the trial (SendNextTrial)"""

        self.trialID    = trialID
        self.targetPos  = targetPos
        self.anchorPos  = anchorPos
        self.annotTCT   = tct
        self.trialTCT   = trialTCT
        self.pointingID = pointingID;

    def __repr__(self):
        return "ID: {}, Accuracy: {}, annotTCT: {}, trialTCT: {}\n".format(self.trialID, np.linalg.norm(self.anchorPos-self.targetPos), self.annotTCT*1e-6, self.trialTCT*1e-6)

    precision = property(lambda self: np.linalg.norm(self._targetPos - self._anchorPos))

class PointingTrial:
    """Represent a set of trial for a given pointingID"""

    def __init__(self, pointingID):
        """Constructor.
        @param pointingID the pointingID of this interaction technique"""
        self.annotations = list()
        self.pointingID  = pointingID

    def __repr__(self):
        return "PointingID: {}\nAnnotations:\n{}\n".format(self.pointingID, self.annotations)


class TabletData:
    """ Represent the data own by a Tablet"""
    def __init__(self, tabletID, headsetIP):
        self._study1Annotations = [PointingTrial(0), PointingTrial(1), PointingTrial(3)]
        self._study2Annotations = [PointingTrial(0), PointingTrial(1), PointingTrial(3)]
        self._currentAnnotation = None
        self._currentStudyID    = -1
        self.tabletID           = tabletID
        self.headsetIP          = headsetIP

    def __repr__(self):
        return "tabletID: {}\nstudy 1: {}\n Study2: {}\n".format(self.tabletID, self._study1Annotations, self._study2Annotations)

    def initAnnotation(self, studyID, trialID, pointingID, targetPos, trialStartTime, annotStartTime):
        """Initialize a new Annotation. This annotation will not yet be pushed into the tablet data (waiting the commitAnnotation call)
        @param studyID the ID of the study. Either 1, 2 or 3
        @param trialID the ID of the trial
        @param targetPos the position of the target
        @param startTime the starting time of the annotation"""

        self._currentAnnotation = Annotation(trialID, pointingID, targetPos, targetPos, annotStartTime, trialStartTime)
        self._currentStudyID    = studyID

    def commitAnnotation(self, anchorPos, endTime):
        """Commit a started annotation. Call initAnnotation first
        @param anchorPos the anchoring position done by the user
        @param endTime the when the annotation has been anchored"""

        if self._currentAnnotation is not None:
            self._currentAnnotation.annotTCT = endTime - self._currentAnnotation.annotTCT
            self._currentAnnotation.trialTCT = endTime - self._currentAnnotation.trialTCT
            self._currentAnnotation.anchorPos = anchorPos
            self._pushAnnotation(self._currentStudyID, self._currentAnnotation)
            self._currentAnnotation = None


    def _pushAnnotation(self, studyID, annot):
        """Push a new annotation into the tablet data
        @param studyID the ID of the study. Either 1, 2 or 3
        @param annot the Annotation object to add"""

        studyList       = None

        if studyID == 1:
            studyList = self._study1Annotations
        elif studyID == 2:
            studyList = self._study2Annotations

        if studyList != None:
            for pointingTrial in studyList:
                if pointingTrial.pointingID == annot.pointingID:
                    pointingTrial.annotations.append(annot)
                    break
