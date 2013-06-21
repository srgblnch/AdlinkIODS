
resDir = 'results'

import sys


if len(sys.argv) < 2:
	print 'You must specify the date as a parameter'
	sys.exit(-1)

from PyQt4 import QtCore, QtGui
from PyQt4.Qwt5 import QwtPlot, QwtPlotCurve
import pickle

data = sys.argv[1]


app = QtGui.QApplication(sys.argv)

p = QwtPlot()


curve = QwtPlotCurve("Segon i tal")


fn = resDir + '/' + data
f = file(fn)
v = pickle.load(f)


y = v
x = range(len(y))

curve.setData(x, y)

curve.attach(p)

p.replot()

p.show()

sys.exit(app.exec_())




