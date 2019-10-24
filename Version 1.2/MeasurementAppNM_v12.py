import matplotlib
matplotlib.use("TkAgg")
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg, NavigationToolbar2Tk
from matplotlib.figure import Figure
import matplotlib.animation as animation
from tkinter import *
import serial
import datetime
import csv

NORM_FONT= ("Verdana", 10)

creditsStr= "Physics Measurement System v1.2\nJune 2019\n\nGUI:Adi Bozzhanov\nArduino: Laveen Chandnai\nSensors: Martin Lee & Tanthun Assawapitiyaporn\n"

global dataArray, csvArray
csvArray = []
dataArray = []
notPressed = True

def saveToCsv():
	global dataArray
	now = datetime.datetime.now()
	fileName = now.strftime("%Y%m%d-%H%M" + ".csv")
	print(fileName)
	print(csvArray)
	with open(fileName, 'w') as writeFile:
		writer = csv.writer(writeFile)
		for each in csvArray:
			writer.writerow([each])



def stop():
	global notPressed, refreshing
	writeSer("STOP")
	readSer()
	notPressed = True
	refreshing = False

def start():
	global count, refreshing, dataArray,notPressed
	if notPressed == True:
		notPressed = False
		dataArray = []
		csvArray = []
		table.delete(0,END)
		setSamples()
		setInterval()
		if refreshing == False:
			count = samples
			refreshing = True
			writeSer("START")
			readSer()



def setSamples():
	writeSer("SET SAMPLES "+ str(samples))
	readSer()
	
def setInterval():
	writeSer("SET INTERVAL "+ str(inter))
	readSer()

def refresh(i):
	global count, refreshing, unit, notPressed, csvArray
	
	
	if (refreshing == True)and(count != 0):
		xar=[]
		yar=[]
		for eachLine in dataArray:
			if len(eachLine)>1:
				x,y = eachLine.split(',')
				xar.append(float(x))
				yar.append(float(y))
		a.clear()
		a.set_xlabel("seconds (s)")
		a.set_ylabel(unit)
		a.plot(xar,yar)
		
		j = samples - count
		data = readSer()
		array = data.split(" ")
		ix = int(array[1])
		data = float(array[2])
		dataArray.append(str(ix*inter/1000)+","+str(data))
		csvArray.append(str(data))
		table.insert(j+1,str(j + 1) + ": " + str(data))
		j = j + 1
		count = count - 1
		if count == 0:
			refreshing = False
			notPressed = True
		
		
	

def initGraph(master):
	global a, f, unit
	
	unit = ""
	
	f = Figure(figsize=(5,5), dpi=100)
	a = f.add_subplot(111)
	a.set_xlabel("seconds (s)")
	a.set_ylabel(unit)
	canvas = FigureCanvasTkAgg(f, topFrame)
	canvas.draw()
	canvas.get_tk_widget().pack(side=BOTTOM, fill=BOTH, expand=True)
	canvas._tkcanvas.pack(side=TOP, fill=BOTH, expand=True)


def samplePop():
	global popup,e1,t1
	popup = Tk()
	t1 = Label(popup, text = "Enter Number of samples:")
	t1.grid(row = 0, column = 0,sticky = W)
	e1 = Entry(popup)
	e1.grid(row = 1, column = 0,sticky = W)
	b1 = Button(popup, text = "OK", command = sampleButton)
	b1.grid(row = 1, column = 1, sticky = W)
	popup.mainloop()

def intervalPop():
	global popup,e1,t1
	popup = Tk()
	t1 = Label(popup, text = "Enter Interval:")
	t1.grid(row = 0, column = 0,sticky = W)
	e1 = Entry(popup)
	e1.grid(row = 1, column = 0,sticky = W)
	b1 = Button(popup, text = "OK", command = intervalButton)
	b1.grid(row = 1, column = 1, sticky = W)
	popup.mainloop()


def sampleButton():
	global samples, e1, popup, sampleText
	samples = int(e1.get())
	sampleText.configure(text = "Number of samples: " + str(samples))
	popup.destroy()

def intervalButton():
	global inter, e1, popup, intervalText
	inter = int(e1.get())
	intervalText.configure(text = "Interval: " + str(inter))
	popup.destroy()
	
def popupmsg(title,msg): # display a message in a popup window with a title
    global popup,t1
    popup = Tk()
    popup.wm_title(title)
    t1 = Label(popup, text=msg , font=NORM_FONT, anchor = W) 	
    t1.pack(side="top", fill="x", pady=10)
    b1 = Button(popup, text="Ok", command = popup.destroy)
    b1.pack()
    popup.mainloop()
	
	
# def showAbout:  # attempt to display a message window but does not work
   # global popup,t1
   # popup = Tk()
   # t1 = MessageBox.showinfo("Say Hello", "Hello World")
   # t1.pack()
   # popup.mainloop()
   
# def getBoard():
    # writeSer("GET BOARD")
	# board=readSer() 	
	# board = split(" ")
	# outMessage("Version: " +board[3] + "| Number of Channels: " + board[4])



def initToolbar(master):
	toolbar = Menu(master)
	#File
	fileMenu = Menu(toolbar)
	toolbar.add_cascade(label = "File", menu = fileMenu)
	fileMenu.add_command(label = "Save", command = saveToCsv)	
	#Config
	configMenu = Menu(toolbar)
	mPointMenu = Menu(configMenu)
	toolbar.add_cascade(label = "Config",menu = configMenu)
	configMenu.add_cascade(label = "Voltage", command = setVoltage)
	configMenu.add_cascade(label = "Ultra Sound Meter", underline = 0,command = setUltraSound)
	configMenu.add_cascade(label = "IR sensor", underline = 0,command = irSensor)
	configMenu.add_separator()
	configMenu.add_command(label = "Samples", command = samplePop)
	configMenu.add_command(label = "Interval", command = intervalPop)
	#Tools
	toolMenu = Menu(toolbar)
	toolbar.add_cascade(label = "Tools", menu = toolMenu)
	toolMenu.add_command(label = "Connect")
	#toolMenu.add_command(label = "Board", command = getBoard)
	toolMenu.add_command(label = "Board")
	#Help
	helpMenu = Menu(toolbar)
	toolbar.add_cascade(label = "Help", menu = helpMenu)
	#helpMenu.add_command(label = "About", command = showAbout) # commented out because this did not work
	helpMenu.add_command(label = "About", command = lambda:popupmsg("About",creditsStr))

	master.config(menu = toolbar)

def initDataDisplay(master):
	global sampleText,intervalText
	
	dataFrame = Frame(master)
	dataFrame.grid(row = 1, column = 0, sticky = W)
	sampleText = Label(dataFrame, text = "Number of samples: " + str(samples))
	sampleText.grid(row = 0, column = 0, sticky = W)
	intervalText = Label(dataFrame, text = "Interval: " + str(inter))
	intervalText.grid(row = 1, column = 0, sticky = W)
	buttonFrame = Frame(master)
	buttonFrame.grid(row = 0, column = 0, sticky = W)
	startButton = Button(buttonFrame, text = "START", command = start)
	startButton.grid(row = 0, column = 0)
	stopButton = Button(buttonFrame, text = "STOP", command = stop)
	stopButton.grid(row = 0, column = 1)
	
def printS(i):
	print(i[0:len(i)-2].decode("utf-8"))

def setUltraSound():
	global unit
	writeSer("SET CHAN 1")
	temp = readSer()
	unit = "millimeters(mm)"
	
def writeSer(i):
	if connected == True:
		ser.write(bytes(i+"\n","utf-8"))
		print(i)

def readSer():
	if connected == True:
		temp = ser.readline()
		printS(temp)
		return temp[0:len(temp)-2].decode("utf-8")
	
	
def irSensor():
	writeSer("SET CHAN 2")
	readSer()
	
	
	
def setVoltage():
	global unit
	writeSer("SET CHAN 0")
	temp = readSer()
	
	unit = "Volts(V)"

def initialise(master):
	global leftFrame, rightFrame, bottomFrame, topFrame, inter, samples, refreshing
	
	refreshing = False
	inter = 100
	samples = 10
	
	
	connect("COM4")
	initToolbar(master)
	leftFrame = Frame(master, width = 50)
	leftFrame.grid(row = 0, column = 0, sticky = N)
	topFrame = Frame(master)
	topFrame.grid(row = 0, column = 1)
	bottomFrame = Frame(master)
	bottomFrame.grid(row = 1, column = 1, sticky = W)
	rightFrame = Frame(master)
	rightFrame.grid(row = 0, column = 2, sticky = N)
	
	
	initGraph(topFrame)
	initTable(rightFrame)
	initDataDisplay(leftFrame)

def initTable(master):
	global table
	t1 = Label(master, text = "Data Logger:")
	t1.grid(row = 0, column = 0, sticky = W)
	table = Listbox(master, selectmode = EXTENDED, height = 25)
	table.grid(row = 1, column = 0, sticky = W)

def connect(port):
	global ser, connected
	try:
		ser = serial.Serial(port)
		ser.flushInput()
		print("Connected")
		connected = True
	except:
		connected = False
		print("Not connected")

def main():
	root = Tk()
	root.title("OIC Physics Measurement System 2019")
	initialise(root)
	
	
	
	
	
	
	ani = animation.FuncAnimation(f,refresh, interval=inter/2)
	root.mainloop()
main()
