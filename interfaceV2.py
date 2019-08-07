from PyQt5.uic import loadUiType

import sys
from PyQt5.QtWidgets import QDialog, QApplication, QPushButton, QMessageBox, QFileDialog, QErrorMessage

from matplotlib.backends.backend_qt5agg import FigureCanvasQTAgg as FigureCanvas
from matplotlib.backends.backend_qt5agg import NavigationToolbar2QT as NavigationToolbar
from mpl_toolkits.axes_grid1 import make_axes_locatable

import numpy as np
import matplotlib.pyplot as plt
import serial as Serial
import scipy.interpolate
from PIL import Image
import time


Ui_MainWindow, QMainWindow = loadUiType('interface.ui')



class Main(QMainWindow, Ui_MainWindow):
    def __init__(self, ): 
        super(Main, self).__init__()
        self.setupUi(self)

        self.fig = plt.Figure()
        self.ax = self.fig.add_subplot(111)
        self.canvas = FigureCanvas(self.fig)
        self.openTextInput()
        self.inputData = np.zeros([512,512])
        self.resizedArr = np.zeros([256,256])
        self.hlinePosition = 0

        #take care of a colorbar
        #self.ax.set_aspect('equal')
        divider = make_axes_locatable(self.ax)
        self.cax = divider.new_horizontal(size="5%", pad=0.15, pack_start=False)
        self.colorbar = self.fig.add_axes(self.cax)
        try:
            self.serial = Serial.Serial(port='/dev/tty.usbmodem1414201', baudrate=57600)
        except Serial.serialutil.SerialException:
            print('Serial port not available.')
        #

        
        #Definitions of callbacks to functions, at events.

        
        ## Input definition controls
        self.pushUpdatePlot.clicked.connect(self.updatePlot)
        #self.textNotes
        #self.spinTimeMultiplier

        self.pushFile.clicked.connect(self.generateFile)
        self.toolButton.clicked.connect(self.openFileNameDialog)

        self.pushDisc.clicked.connect(self.generateDisc)
        self.pushFZP.clicked.connect(self.generateFZP)

        self.pushSinusoid.clicked.connect(self.generateSinus)
        #self.spinBoxNx
        #self.spinBoxNy


        ## Execution control
        self.pushCenter.clicked.connect(self.defineCenter)
        self.pushStart.clicked.connect(self.startScan)
        self.pushPause.clicked.connect(self.pauseScan)
        self.pushStop.clicked.connect(self.stopScan)

        #self.joystickManual
        #self.joystickLockout
        self.shutterClosed.toggled.connect(lambda: self.shutterToggle(self.shutterClosed))
        self.shutterOpen.toggled.connect(lambda: self.shutterToggle(self.shutterOpen))

        self.joystickManual.toggled.connect(lambda: self.joystickToggle(self.joystickManual))
        self.joystickLockout.toggled.connect(lambda: self.joystickToggle(self.joystickLockout))

        
        ## Set relevant states
        self.stateScanning = False
        self.statePaused = False
        self.stateWasPaused = False
        self.stateRemoteJoystick = False
        self.stateShutterOpen = False
        self.stateCenterDefined = False
        self.stateHasData = False
        self.stateHasPlotted = False

        # Establish Serial comms
        #connectSerialPort()
        


    def closeEvent(self, event):

        quit_msg = "Are you sure you want to exit the program?"
        reply = QMessageBox.question(self, 'Message', 
                        quit_msg, QMessageBox.Yes, QMessageBox.No)

        if reply == QMessageBox.Yes:
            #save the contents of the text box.
            self.saveTextInput()
            #disconnectSerialPort()
            time.sleep(0.1)
            event.accept()
        else:
            event.ignore()


    def addmpl(self, fig):
        self.canvas = FigureCanvas(fig)
        self.mplLayout.addWidget(self.canvas)
        self.canvas.draw()

        self.toolbar = NavigationToolbar(self.canvas, 
                self.plotContainer, coordinates=True)
        self.mplLayout.addWidget(self.toolbar)
        

    def connectSerialPort(self):
        if not self.serial.is_open:
            try:
                self.serial.open()
                self.readAllSerial()
            except SerialException:
                print("Serial port not available")
        else:
            print("Serial port already open.")

    def disconnectSerialPort(self):
        if self.serial.is_open:
            self.serial.close()

    def readAllSerial(self, returnOutput = False, printOutput = True):
        if self.serial.is_open:
            returnString = str(self.serial.read(self.serial.inWaiting())).strip("'").strip("b'")

            if printOutput:
                print(returnString)

            # In case of an error from the arduino:
            if returnString[:5] == "ERROR:"[:5]:
                errorMessage = QErrorMessage()
                errorMessage.showMessage(returnString)
            
            if returnOutput:
                return returnString

    def saveTextInput(self):
        textFile = './RunningNotes.html'
        textFileHandle = open(textFile, 'w')

        print(self.textNotes.toHtml())
        textFileHandle.write(self.textNotes.toHtml())
        #print("Wrote {} characters to {}".format(
                                                #len(self.textNotes.toHtml()),
                                                #textFile))
        textFileHandle.close()
    
    def openTextInput(self):
        textFile = './RunningNotes.html'
        textFileHandle = open(textFile, 'r')
        inputText = textFileHandle.read()
        self.textNotes.setText(str(inputText))
        textFileHandle.close()


    def openFileNameDialog(self):
        options = QFileDialog.Options()
        options |= QFileDialog.DontUseNativeDialog
        self.fileName, _ = QFileDialog.getOpenFileName(self,"Select input image", "","Sane image files (*.png, *.jpg, *.jpeg);;All Files (*)", options=options)
        if self.fileName:
            print("Opening file: "+self.fileName)
            self.lineFileName.setText(self.fileName)

    def updatePlot(self):
        if self.stateHasData:
            #self.fig.clear()

            # Scale image
            n = 256
            image = Image.fromarray(self.inputData)
            resized = image.resize((n,n), resample=Image.BILINEAR).getdata()
            imgData = np.array(resized).reshape(n,n)*self.spinTimeMultiplier.value()
            self.resizedArr = imgData.astype(int)

            #plot the data
            im=self.ax.imshow(imgData, origin='lower')

            #take care of the colorbar
            self.fig.colorbar(im, cax=self.cax, orientation='vertical',)

            #plot a line that indicates the scan line
            self.ax.axhline(self.hlinePosition, c='r', lw=2)

            plt.tight_layout()
            self.canvas.draw()
            self.stateHasPlotted = True
    
    def generateFile(self):
        if self.fileName:
            image = Image.open(self.fileName)
            imgNx, imgNy = image.size
            # Three things happen here:
            ## I convert to grayscale, then extract the data, and finally reshape to correct dimensions. This means the imageArray is ready to pass on.
            imageArray = np.array(image.convert('L').getdata(), dtype=float).reshape([imgNy, imgNx])

            self.inputData = imageArray
            self.stateHasData = True
            
        
        else:
            print("No file specified for generateFile")
    
    def generateDisc(self):
        extend = self.spinExtend.value()
        nn = np.linspace(-extend, extend, 256)
        XX, YY = np.meshgrid(nn, nn)

        zsingle = np.floor(np.sqrt(XX**2 + YY**2))
        zarray = np.concatenate((zsingle, zsingle, zsingle, zsingle, zsingle))
        self.inputData = np.concatenate((zarray, zarray, zarray, zarray, zarray), axis=1)*100
        self.stateHasData = True

    def generateFZP(self):
        print("This feature is not yet implemented.")

    def generateSinus(self):
        sz = 512
        large = np.zeros([sz,sz])
        ns = np.linspace(0, np.pi*3, large.shape[0])
        XX,YY = np.meshgrid(ns, ns)

        ZZ = np.sin(XX*self.spinBoxNx.value()/np.pi) * np.sin(YY/np.pi*self.spinBoxNy.value()) * 40 + 40

        self.inputData = ZZ
        self.stateHasData = True


    def defineCenter(self):
        if self.serial.is_open:
            self.serial.write(b'CENTER\n')
            self.stateCenterDefined = True
            time.sleep(0.2)
            self.readAllSerial()

    def startScan(self):
        # only allowed when it has been centered
        if (self.stateCenterDefined and self.serial.is_open and self.stateHasPlotted) :
            self.stateScanning = True

            #Handle the four segments of figures that are sent:
            listOfStringsA = []
            listOfStringsB = []
            listOfStringsC = []
            listOfStringsD = []
            for i in range(self.resizedArr.shape[1]):
                tempString = ""
                for j in range(self.resizedArr.shape[0]//4):
                    tempString += str(self.resizedArr[i,j]) + ' '
                listOfStringsA.append(tempString)
                    
                tempString = ""
                for j in range(self.resizedArr.shape[0]//4):
                    tempString += str(self.resizedArr[i,j+64]) + ' '
                listOfStringsB.append(tempString)
                    
                tempString = ""
                for j in range(self.resizedArr.shape[0]//4):
                    tempString += str(self.resizedArr[i,j+128]) + ' '
                listOfStringsC.append(tempString)
                    
                tempString = ""
                for j in range(self.resizedArr.shape[0]//4):
                    tempString += str(self.resizedArr[i,j+192]) + ' '
                listOfStringsD.append(tempString)


            while self.hlinePosition < 256: 
            #transmit the segments:

                # First thing; check if we have not pressed the stop button.
                if not self.stateScanning:
                    break
                byteStringA = b'SCAN A ' + str.encode(listOfStringsA[self.hlinePosition]) + b' \n'
                print(byteStringA)
                self.serial.write(byteStringA)
                time.sleep(0.2)

                byteStringB = b'SCAN B ' + str.encode(listOfStringsB[self.hlinePosition]) + b' \n'
                print(byteStringB)
                self.serial.write(byteStringB)
                time.sleep(0.2)

                byteStringC = b'SCAN C ' + str.encode(listOfStringsC[self.hlinePosition]) + b' \n'
                print(byteStringC)
                self.serial.write(byteStringC)
                time.sleep(0.2)

                byteStringD = b'SCAN D ' + str.encode(listOfStringsD[self.hlinePosition]) + b' \n'
                print(byteStringD)
                self.serial.write(byteStringD)
                time.sleep(0.2)

                #Call the ucontroller to execute the scan line.
                self.serial.write(b'EXEC\n')
                time.sleep(0.5)
                self.readAllSerial()

                self.hlinePosition += 1
                self.updatePlot()

                stateWaitingForLine = True
                while stateWaitingForLine and self.stateScanning:
                    output = str(self.readAllSerial(returnOutput=True, printOutput=False) )
                    if len(output)>10:
                        print(output)
                    if "succesfully" in output:
                        stateWaitingForLine = False
                        print("Reached End of line.")
                        
                    time.sleep(0.05)
                time.sleep(2.5)

                #Crude implementation of the pause. As it is now, it's fairly useless.
                if self.statePaused:
                    self.closeShutter()
                    while statePaused:
                        pass
                    self.openShutter()
                    time.sleep(0.1)


    def pauseScan(self):
        # To enable/disable the pause, I need to use the pause button to pause and unpause. To help people, I make it change the text when pressed.
        if self.stateScanning:
            if not self.statePaused:
                self.statePaused = True
                self.pushPause.setText("Restart")
            elif self.statePaused:
                self.statePaused = False
                self.pushPause.setText('Pause')

    def stopScan(self):
        if self.stateScanning:
            self.stateScanning = False
            self.stateWasPaused = False
            # only when you press the stop button, the counter starts at zero.
            self.hlinePosition = 0
        else:
            pass

    def closeShutter(self):
        if self.serial.is_open:
            self.serial.write(b'CLOSE\n')
            self.readAllSerial()
    
    def openShutter(self):
        if self.serial.is_open:
            self.serial.write(b'OPEN\n')
            self.readAllSerial()

    def shutterToggle(self, button):
        if button.text() == 'Open':
            if button.isChecked():
                self.openShutter()
            else: 
                self.closeShutter()
        elif button.text() == 'Closed':
            if button.isChecked():
                self.closeShutter()
            else:
                self.openShutter()

        else:
            print('Odd things abound.')
            pass #We should hopefully never get here.


    def controlRemote(self):
        if self.serial.is_open:
            self.stateRemoteJoystick = True
            self.serial.write(b'REM\n')
            time.sleep(0.2)
            self.readAllSerial()
    
    def controlManual(self):
        if self.serial.is_open:
            self.stateRemoteJoystick = False
            self.serial.write(b'MAN\n')
            time.sleep(0.2)
            self.readAllSerial()

    def joystickToggle(self, button):
        print(button.text())
        if button.text() == 'Manual':
            if button.isChecked():
                self.controlManual()
            else:
                self.controlRemote()
        elif button.text() == "Remote lock-out":
            if button.isChecked():
                self.controlRemote()
            else:
                self.controlManual()
        else:
            print("Something not cool has gone down.")


    

'''
########################################################################################
########################################################################################
########################################################################################
########################################################################################
'''
 
if __name__ == '__main__':

    
 
    app = QApplication(sys.argv)
    main = Main()
    main.addmpl(main.fig)
    main.updatePlot()
    main.show()
    sys.exit(app.exec_())