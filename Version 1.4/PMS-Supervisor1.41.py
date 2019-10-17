import matplotlib
matplotlib.use("TkAgg")
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg, NavigationToolbar2Tk
from matplotlib.figure import Figure
import matplotlib.animation as animation
import serial.tools.list_ports
try:
	from Tkinter import *
except:
	from tkinter import *
import serial
import datetime
import csv
#Test Line
# 28/06/19 Extended from v1.1
# About box with version number and credits
# Channel description in row 0 of left frame
# Revised order for top level menu items
# Message box added to bottom frame
# Serial port name defined by a string at top of code to make change easier
# Board data displayed in message box
#
# 29/06/19 v1.3
# renamed as PMS-Supervisor v 1.3
# added channel specific Y axis labels and used A-level label notation on x and y axis labels
# dialogs for samples and intervals prepopulated with values from microcontroler
# Tools/Connect command implemented
# 01/07/19
# Fixed line terminator problem in writing CSV file

# next steps:
# 1. Unit element in yAxisLabel to be obtained from microcontroller
# 2. display data to no more than 3sf (esp for axis labelling)
# 3. move related code to separate modules and import using the "from import" command
#    possible modules: graph plotting, GUI definition, set and get routines
# 4. Auto save tick box on Config menu to save data to CSV
# 5. exit option on file menu
#      https://stackoverflow.com/questions/110923/how-do-i-close-a-tkinter-window
# 6. scrollable data display in right frame
#      https://stackoverflow.com/questions/3085696/adding-a-scrollbar-to-a-group-of-widgets-in-tkinter

# set this to the name of the serial port for the board you have connected


# 03/10/2019 v 1.41
# Changed toolbar options from cascades to commands

#Hopefully the last version
#Things to implement in vetsion 2
# - Build a graph on Canvas by hand(to solve the size of file issue as matplotlib libraries are too big)
# - Implement the whole project in Classes

def getPort(): #Function to auto get port
	ports = list(serial.tools.list_ports.comports())
	comPort = str(ports[-1])
	indexCount = 0
	for x in comPort:
		if x != " ":
			indexCount += 1
		else:
			return comPort[0:indexCount]
			
serialPort = ""
try:
	serialPort = getPort()
except:
	print("Port not found")
verStr = "1.4"

creditsStr = "Physics Measurement System v" + verStr + "\nJune 2019\n\nCOMPSCI GANG\nGUI:Adi Bozzhanov\nArduino: Laveen Chandnani\nSensors: Martin Lee & Tanthun Assawapitiyaporn\n"

NORM_FONT= ("Verdana", 10)
# channel specific text
chanStr = ["Voltage","Ultrasonic", "IR sensor"] # sensor type
yAxisLabel = ["Voltage /V","distance /mm","time period /ms"] # 

# these variables are global at the module level
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
        # change the line terminator to \n instead of the default \r\n
        # see here for details:  https://docs.python.org/3/library/csv.html#csv-fmt-params
        writer = csv.writer(writeFile,dialect='excel',lineterminator='\n')
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

# return the samples setting from the currently selected channel
def getSamples():
    if connected:
        writeSer("GET SAMPLES")
        response=readSer()     
        response = response.split(" ")
        #print("value list is ",response)
        return(int(response[3]))
    
def setInterval():
    writeSer("SET INTERVAL "+ str(inter))
    readSer()

# return the interval setting from the currently selected channel
def getInterval():
    if connected:
        writeSer("GET INTERVAL")
        response=readSer()     
        response = response.split(" ")
        #print("value list is ",response)
        return(int(response[3]))

def refresh(i):
    global count, refreshing, unit, notPressed, csvArray,chanNum,yAxisLabel
    
    
    if (refreshing == True)and(count != 0):
        xar=[]
        yar=[]
        for eachLine in dataArray:
            if len(eachLine)>1:
                x,y = eachLine.split(',')
                xar.append(float(x))
                yar.append(float(y))
        a.clear()
        a.set_xlabel("time /s")
        a.set_ylabel(yAxisLabel[chanNum])
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
    global a, f, unit,chanNum,yAxisLabel
    
    unit = ""
    
    f = Figure(figsize=(5,5), dpi=100)
    a = f.add_subplot(111)
    a.set_xlabel("time /s")
    a.set_ylabel(yAxisLabel[chanNum])
    canvas = FigureCanvasTkAgg(f, topFrame)
    canvas.draw()
    canvas.get_tk_widget().pack(side=BOTTOM, fill=BOTH, expand=True)
    canvas._tkcanvas.pack(side=TOP, fill=BOTH, expand=True)


def samplePop():
    global popup,e1,t1,samples
    if connected:
        samples=getSamples() # read the value from the Arduino
    popup = Tk()
    t1 = Label(popup, text = "Enter Number of samples:")
    t1.grid(row = 0, column = 0,sticky = W)
    # see here for Entry widget: https://www.tutorialspoint.com/python/tk_entry.htm
    e1 = Entry(popup) # single line text entry
    e1.insert(1,str(samples)) # inserting the value at position 1
    e1.grid(row = 1, column = 0,sticky = W)
    b1 = Button(popup, text = "OK", command = sampleButton)
    b1.grid(row = 1, column = 1, sticky = W)
    popup.mainloop()

def intervalPop():
    global popup,e1,t1,inter
    if connected:
        inter=getInterval() # read the value from the Arduino
    popup = Tk()
    t1 = Label(popup, text = "Enter Interval (ms):")
    t1.grid(row = 0, column = 0,sticky = W)
    e1 = Entry(popup)
    e1.insert(1,str(inter))
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
    b1 = Button(popup, text="Close", command = popup.destroy)
    b1.pack()
    popup.mainloop()
    
       
def getBoard():
    if connected:
        writeSer("GET BOARD")
        board=readSer()     
        board = board.split(" ")
        msg = "Version: " +board[3] + "  Number of Channels: " + board[4]
        print(msg)
        outMessage("Version: " +board[3] + "  Number of Channels: " + board[4])



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
    configMenu.add_command(label = "Voltage", command = setVoltage)
    configMenu.add_command(label = "Ultra Sound Meter", underline = 0,command = setUltraSound)
    configMenu.add_command(label = "IR sensor", underline = 0,command = irSensor)
    configMenu.add_separator()
    configMenu.add_command(label = "Samples", command = samplePop)
    configMenu.add_command(label = "Interval", command = intervalPop)
    #Tools
    toolMenu = Menu(toolbar)
    toolbar.add_cascade(label = "Tools", menu = toolMenu)
    toolMenu.add_command(label = "Connect",command = setConnection)
    toolMenu.add_command(label = "Board", command = getBoard)
    #Help
    helpMenu = Menu(toolbar)
    toolbar.add_cascade(label = "Help", menu = helpMenu)
    helpMenu.add_command(label = "About", command = lambda:popupmsg("About OIC PMS",creditsStr))

    master.config(menu = toolbar)

def initDataDisplay(master):
    global sampleText,intervalText,chanText,chanNum,chanDesc

    # chanDesc is used to update the channel description string in the left frame
        # see here: https://stackoverflow.com/questions/1918005/making-python-tkinter-label-widget-update
    chanDesc = StringVar()
    chanDesc.set("Chan: " + chanStr[chanNum])
    
    dataFrame = Frame(master)
    dataFrame.grid(row = 1, column = 0, sticky = W)
    chanText = Label(dataFrame, textvariable = chanDesc)
    chanText.grid(row = 0, column = 0, sticky = W)
    sampleText = Label(dataFrame, text = "Number of samples: " + str(samples))
    sampleText.grid(row = 1, column = 0, sticky = W)
    intervalText = Label(dataFrame, text = "Interval: " + str(inter))
    intervalText.grid(row = 2, column = 0, sticky = W)
    buttonFrame = Frame(master)
    buttonFrame.grid(row = 0, column = 0, sticky = W)
    startButton = Button(buttonFrame, text = "START", command = start)
    startButton.grid(row = 0, column = 0)
    stopButton = Button(buttonFrame, text = "STOP", command = stop)
    stopButton.grid(row = 0, column = 1)

def outMessage(i):
    global textHolder
    textHolder.configure(text = i)
    
def printS(i):
    print(i[0:len(i)-2].decode("utf-8"))

    
def writeSer(i):
    if connected == True:
        ser.write(bytes(i+"\n","utf-8"))
        print(i)

def readSer(): # *** what does this return if not connected?
    if connected == True:
        temp = ser.readline()
        printS(temp)
        return temp[0:len(temp)-2].decode("utf-8")
    

def setVoltage():
    global a,unit, chanNum,chanDesc
    writeSer("SET CHAN 0")
    readSer()
    unit = "Voltage (V)"
    chanNum = 0
    #print("chan num ",chanNum)
    chanDesc.set("Chan: " + chanStr[chanNum])
    a.set_ylabel(yAxisLabel[chanNum])

def setUltraSound():
    global a,unit, chanNum,chanDesc
    writeSer("SET CHAN 1")
    readSer()
    unit = "distance (mm)"    
    chanNum = 1
    #print("chan num ",chanNum)
    chanDesc.set("Chan: " + chanStr[chanNum])
    a.set_ylabel(yAxisLabel[chanNum])

def irSensor():
    global a,unit, chanNum,chanDesc
    writeSer("SET CHAN 2")
    readSer()
    unit = "time period (ms)"
    chanNum = 2
    print("chan num ",chanNum)
    chanDesc.set("Chan: " + chanStr[chanNum])
    a.set_ylabel(yAxisLabel[chanNum])

# manual connection initiated from the Tools menu
def setConnection():
    if connected == False:
        connect(serialPort)

def initialise(master):
    global leftFrame, rightFrame, bottomFrame, topFrame, inter, samples, refreshing, textHolder,chanNum
    
    refreshing = False
    # initial settings which are overwritten when a connection is available
    inter = 200 # ms
    samples = 10
    chanNum = 0
    
    initToolbar(master)
    leftFrame = Frame(master, width = 50)
    leftFrame.grid(row = 0, column = 0, sticky = N)
    topFrame = Frame(master)
    topFrame.grid(row = 0, column = 1)
    bottomFrame = Frame(master)
    bottomFrame.grid(row = 1, column = 1, sticky = W)
    rightFrame = Frame(master)
    rightFrame.grid(row = 0, column = 2, sticky = N)
    
    #MessageBox
    textHolder = Label(bottomFrame, text = "")
    textHolder.pack()

    connect(serialPort)
    
    initGraph(topFrame)
    initTable(rightFrame)
    initDataDisplay(leftFrame)

def initTable(master):
    global table
    t1 = Label(master, text = "Data Values:")
    t1.grid(row = 0, column = 0, sticky = W)
    table = Listbox(master, selectmode = EXTENDED, height = 25)
    table.grid(row = 1, column = 0, sticky = W)

def connect(port):
    global ser, connected
    try:
        ser = serial.Serial(getPort())
        ser.flushInput()
        print("Connected")
        outMessage("Connected")
        connected = True
        #getSamples() # number of samples for current channel        
    except:
        connected = False
        print("Not connected")
        outMessage("Not connected")

def main():
    root = Tk()
    root.title("OIC Physics Measurement System 2019")
    initialise(root)
    
    ani = animation.FuncAnimation(f,refresh, interval=inter/2)
    root.mainloop()
main()
