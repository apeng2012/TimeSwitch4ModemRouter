import sys
import time
import serial
try:
    import xml.etree.cElementTree as ET
except ImportError:
    import xml.etree.ElementTree as ET

saveFileName = "SwitchTime.xml"

def list2xml(stList):
    
    if len(stList) != 7:
        print "DayOfWeek num error!"
        return
    weekname = "sunday", "monday", "tuesday", "wednesday", "thursday", "friday", "saturday"
    root = ET.Element("switchTime")
    i = 0
    for itList in stList:
        dayofweek = ET.SubElement(root, weekname[i])
        print weekname[i]
        i = i+1
        for it in itList:
            item = ET.SubElement(dayofweek, "item")
            item.text = it
            print "\t"+it

    # wrap it in an ElementTree instance, and save as XML
    tree = ET.ElementTree(root)
    tree.write(saveFileName)


def xml2list(stList):
    tree = ET.ElementTree(file=saveFileName)
    root = tree.getroot();
    i=0
    for day in root:
        print day.tag
        for elem in day:
            print elem.text
            stList[i].append(elem.text)
        i = i+1



#!/usr/bin/env python  
#encoding: utf-8
import ctypes

STD_INPUT_HANDLE = -10
STD_OUTPUT_HANDLE= -11
STD_ERROR_HANDLE = -12

FOREGROUND_BLACK = 0x0
FOREGROUND_BLUE = 0x01 # text color contains blue.
FOREGROUND_GREEN= 0x02 # text color contains green.
FOREGROUND_RED = 0x04 # text color contains red.
FOREGROUND_INTENSITY = 0x08 # text color is intensified.

BACKGROUND_BLUE = 0x10 # background color contains blue.
BACKGROUND_GREEN= 0x20 # background color contains green.
BACKGROUND_RED = 0x40 # background color contains red.
BACKGROUND_INTENSITY = 0x80 # background color is intensified.

class Color:
    ''' See http://msdn.microsoft.com/library/default.asp?url=/library/en-us/winprog/winprog/windows_api_reference.asp
    for information on Windows APIs.'''
    std_out_handle = ctypes.windll.kernel32.GetStdHandle(STD_OUTPUT_HANDLE)
    
    def set_cmd_color(self, color, handle=std_out_handle):
        """(color) -> bit
        Example: set_cmd_color(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY)
        """
        bool = ctypes.windll.kernel32.SetConsoleTextAttribute(handle, color)
        return bool
    
    def reset_color(self):
        self.set_cmd_color(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE)
    
    def print_red_text(self, print_text):
        self.set_cmd_color(FOREGROUND_RED | FOREGROUND_INTENSITY)
        print print_text
        self.reset_color()
        
    def print_green_text(self, print_text):
        self.set_cmd_color(FOREGROUND_GREEN | FOREGROUND_INTENSITY)
        print print_text
        self.reset_color()
    
    def print_blue_text(self, print_text): 
        self.set_cmd_color(FOREGROUND_BLUE | FOREGROUND_INTENSITY)
        print print_text
        self.reset_color()
          
    def print_red_text_with_blue_bg(self, print_text):
        self.set_cmd_color(FOREGROUND_RED | FOREGROUND_INTENSITY| BACKGROUND_BLUE | BACKGROUND_INTENSITY)
        print print_text
        self.reset_color()    




def main():
    ser = serial.Serial(sys.argv[1], 9600, timeout=1)
    #ser = serial.Serial('COM10', 9600, timeout=1)
    if ser.isOpen() != True:
        print "open serial port error!"
        return

    clr = Color()

    #Hi
    cmd = "Hi"
    ser.write(cmd); print cmd
    res = ser.readline(); res.strip(); clr.print_green_text(res)
    if res.find("Hello") == -1:
        print "please reset board or check serial port."
        return

    #SetDateTime 2014-01-27 13:21:25 1
    dt = time.localtime()
    cmd = time.strftime("SetDateTime %Y-%m-%d %H:%M:%S %w\r\n", dt)
    ser.write(cmd); print cmd
    res = ser.readline(); res.strip(); clr.print_green_text(res)

    #GetDateTime
    cmd = "GetDateTime"
    ser.write(cmd); print cmd
    res = ser.readline(); res.strip(); clr.print_green_text(res)

    reList = [[] for i in range(7)]

    #ReadAlarm
    cmd = "ReadAlarm"
    ser.write(cmd); print cmd
    res = ser.readline(); res.strip(); clr.print_green_text(res) # "ReadAlarm x
    for i in range(7):
        while True:
            res = ser.readline(); res.strip(); clr.print_green_text(res)
            if res.find("no alarm") != -1:
                continue
            if res.find("weekday") != -1:
                break
            reList[i].append(res[0:12])

    list2xml(reList)
    print "Please set switch time in " + saveFileName
    raw_input("Press Enter to continue...")
    
    reList = [[] for i in range(7)]
    xml2list(reList)
    
    # WriteAlarmX 1>hh:mm-hh:mm 2>...
    for i in range(7):
        cmd = "WriteAlarm" + str(i) + " "
        j = 1
        for t in reList[i]:
            t.strip()
            cmd = cmd + str(j) + ">" + t + " "
            j = j + 1
        ser.write(cmd); print cmd
        res = ser.readline(); res.strip(); clr.print_green_text(res)

    # ProgramAlarm
    cmd = "ProgramAlarm"
    ser.write(cmd); print cmd
    res = ser.readline(); res.strip(); clr.print_green_text(res)

    print "Config Over. reset board to start"

    ser.close()

if __name__=='__main__':
    main()