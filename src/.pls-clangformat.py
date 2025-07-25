#!/usr/bin/env python
# -*- coding: utf-8 -*-

import re
import os
import os.path
import sys,re  
import codecs
import platform

customIgnoreList = []
realIgnoreList = []

# check if it is in ignore list
def IsInIgnoreList(fullPath):
    for temp in customIgnoreList:
        if (temp in fullPath):
            return True
    
    return False;
    

currentDir, filename = os.path.split(os.path.abspath(__file__))
print('Current Dir:' + currentDir) #never include '\' at the end


# read ignore list, these files will not apply clang-format
print('\n\n================================= read ignore =====================================')
f = codecs.open(".pls-clangIgnoreList.txt", 'r',encoding= u'utf-8', errors='ignore')  
lines = f.readlines()
for line in lines:
    line = line.replace("\r", "") #remove special characters
    line = line.replace("\n", "") #remove special characters
    if platform.system() == 'Darwin': # mac 
        line = line.replace('\\', '/')
    elif platform.system() == 'Windows':
        line = line.replace('/', '\\')

    fullPath = os.path.join(currentDir, line)
    customIgnoreList.append(fullPath)
    print(fullPath)
f.close()


# find all files in current directory, including all child directoies
print('\n\n================================= format files =====================================')
for root, dirs, files in os.walk(currentDir):
    for name in files:
        if (name.endswith(".h") or 
            name.endswith(".hpp") or 
            name.endswith(".hxx") or 
            name.endswith(".c") or 
            name.endswith(".cpp") or 
            name.endswith(".cc") or 
            name.endswith(".cxx") or 
            name.endswith(".m") or 
            name.endswith(".mm") or 
            name.endswith(".metal") or 
            name.endswith(".hlsl") or 
            name.endswith(".hlsli")):
            fullPath = os.path.join(root, name)
            ignoreFile = IsInIgnoreList(fullPath)
            if ignoreFile == True:
                realIgnoreList.append(fullPath)
            else:
                print('Format:' + fullPath)
                os.system("clang-format -i %s -style=File" %(fullPath))
                print('Finish Format:' + fullPath)
                
                
# show the ignore files which never apply clang-format
print('\n\n================================= ignore files =====================================')
for fullPath in realIgnoreList:
    print('Ignore File: ' + fullPath)
