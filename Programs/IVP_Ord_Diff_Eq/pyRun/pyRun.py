#compiles fortran code, computes error between real and numerical solutions, and graphs the data
import os
import sys
import numpy as np
import math
import matplotlib.pyplot as plt
#checks to see if already compiled, if not it will make, if already compiled it will make clean and then make
def make():
	doesExecutableExist = os.path.exists('../fortran/main.exe')
	if not doesExecutableExist:
		os.chdir('../fortran')
		makeIt = 'make'
		os.system(makeIt)
	else:
		os.chdir('../fortran')
		makeCleanMake = 'make clean && make'
		os.system(makeCleanMake)

#computes error between real solution and numerical solution
def error(arr):
    eFinal = 0
    eArray = []
    for i in range(len(arr)):
        #Uses provided formula to calculate error
        eArray.append(abs((-math.sqrt(2*np.log(float(arr[i][0])**2 + 1) + 4)) - float(arr[i][1])))
        eFinal += eArray[i]
    eFinal = eFinal/len(arr)
    return eFinal, eArray
#graphs the provided data and saved as file
def graph(numerical,number):
    t = np.asarray([])
    y = np.asarray([])
    t2 = np.asarray([])
    save = number
    actual = np.asarray([])
    #gets and stores t and y values from numerical array
    for i in range(number):
        t = np.append(round(float(numerical[i][0]),2),t)
        y = np.append(round(float(numerical[i][1]),2),y)
    #computes actual values using provided formula
    for i in range(number):
        actual = np.append(actual,(-math.sqrt(2*np.log(float(numerical[i][0])**2 + 1) + 4)))
    eF, eA = error(numerical)
    #plots numerical solutions
    plt.plot(t,y,'r.:')
    number = number - 1
    while number >= 0:
        t2 = np.append(round(float(numerical[number][0]),2),t2)
        number = number - 1
    #plots real solutions
    plt.plot(t2,actual,'b.-')
    #labels
    plt.title(eF)
    plt.xlabel('t axis')
    plt.ylabel('y axis')
    plt.grid()
    #saves graph as a file
    plt.savefig('result_' + str(save) +'.png')

    
if __name__ == '__main__':
    #calls make function
    make()
    #intialize lists
    fread8,fread16,fread32,fread64 = [],[],[],[]
    #read files and store as lists in the format t,y
    #closes files
    f=open( '../fortran/output_08.dat','r')
    for line in f:
        line = line.split()
        fread8.append(line)
    f.close()
    
    f=open( '../fortran/output_16.dat','r')
    for line in f:
        line = line.split()
        fread16.append(line)
    f.close()

    f=open( '../fortran/output_32.dat','r')
    for line in f:
        line = line.split()
        fread32.append(line)
    f.close()

    f=open( '../fortran/output_64.dat','r')
    for line in f:
        line = line.split()
        fread64.append(line)
    f.close()
    #convert lists into numpy arrays
    array8 = np.asarray(fread8)
    array16 = np.asarray(fread16)
    array32= np.asarray(fread32)
    array64= np.asarray(fread64)
    #calls graph function
    graph(array8,8)
    graph(array16,16)
    graph(array32,32)
    graph(array64,64)



