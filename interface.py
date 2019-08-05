
import sys
from PyQt5.QtWidgets import QDialog, QApplication, QPushButton, QVBoxLayout

from matplotlib.backends.backend_qt5agg import FigureCanvasQTAgg as FigureCanvas
from matplotlib.backends.backend_qt5agg import NavigationToolbar2QT as NavigationToolbar
from mpl_toolkits.axes_grid1 import make_axes_locatable

import numpy as np
import matplotlib.pyplot as plt
import serial as Serial
import scipy.interpolate
from PIL import Image
import time


class Window(QDialog):
    def __init__(self, parent=None):
        super(Window, self).__init__(parent)

        # a figure instance to plot on
        self.figure = plt.figure()

        # this is the Canvas Widget that displays the `figure`
        # it takes the `figure` instance as a parameter to __init__
        self.canvas = FigureCanvas(self.figure)

        # this is the Navigation widget
        # it takes the Canvas widget and a parent
        self.toolbar = NavigationToolbar(self.canvas, self)

        # Just some button connected to `plot` method
        self.plotButton = QPushButton('Plot')
        self.plotButton.clicked.connect(self.plot)

        # A button to generate standard data
        self.sinusButton = QPushButton('load sinusoidal data')
        self.sinusButton.clicked.connect(self.sinusoidal)

        # set the layout
        layout = QVBoxLayout()
        layout.addWidget(self.toolbar)
        layout.addWidget(self.canvas)
        layout.addWidget(self.sinusButton)
        layout.addWidget(self.plotButton)
        self.setLayout(layout)

        # set state variables
        self.hasData = False
        self.presentLineNumber = 0

    def sinusoidal(self):
        
        sz = 2048
        large = np.zeros([sz,sz])

        ns = np.linspace(0, np.pi*3, large.shape[0])
        XX,YY = np.meshgrid(ns, ns)
        
        ZZ = np.sin(XX*6/np.pi) * np.sin(YY/np.pi*1) * 40 + 40

        # rescale to 256x256:
        n = 256
        image = Image.fromarray(ZZ)
        self.data = image.resize((n,n), resample=Image.BILINEAR)
        self.XX, self.YY = np.meshgrid(range(n), range(n))
        self.hasData = True

    def plot(self):
        if self.hasData:
            ''' plot some random stuff '''
            # instead of ax.hold(False)
            self.figure.clear()

            # create an axis
            ax = self.figure.add_subplot(111)

            # discards the old graph
            # ax.hold(False) # deprecated, see above

            # plot data
            im = ax.contourf(self.XX,self.YY, self.data, levels=50)
            ax.axhline(self.presentLineNumber, c='r', lw=2)

            ax.set_aspect('equal')
            divider = make_axes_locatable(ax)
            cax = divider.new_vertical(size="5%", pad=0.05, pack_start=True)
            self.colorbar = self.figure.add_axes(cax)
            self.figure.colorbar(im, cax=cax, orientation='horizontal')


            #plt.subplots_adjust(left=0, bottom=0.05, right=1, top=1, wspace=0, hspace=0)
            plt.tight_layout()
            # refresh canvas
            self.canvas.draw()
        else: pass

if __name__ == '__main__':
    app = QApplication(sys.argv)

    main = Window()
    main.show()

    sys.exit(app.exec_())