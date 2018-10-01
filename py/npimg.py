#!/usr/bin/env python

import sys
import time
import numpy
import scipy
from scipy import stats
from scipy import ndimage
from PIL import Image

import os
import fnmatch

def saveFrame(name,ina):
    img= Image.fromarray(ina)
    img.save(name+".png")

def num(s):
    try:
        return int(s)
    except ValueError:
        return float(s)

def scanFileName (name):
    lN=[]
    l1S= name.split('(')
    if len(l1S) > 1:
        l2S= l1S[1].split(')')
        lNS= l2S[0].split(',')
        for s in lNS:
            lN.append(num(s))
        #print "lN=",lN
    return l1S[0], lN

def convert (inpath,name,outpath):
    prefix, lN= scanFileName(name)
    if len(lN) < 2 or len(lN) > 4:
        print "WARNING: bad def", lN
    if len(lN) >= 2:
        defPlane= (lN[0], lN[1])
        if len(lN) >= 3:
            defN= (lN[0], lN[1], lN[2])
            if len(lN) >= 4:
                defN= (lN[0], lN[1], lN[2], lN[3])
    defRGB=(defN[0], defN[1], 3)
    img= numpy.zeros(defRGB, dtype=numpy.uint8, order='C')
    vt= numpy.fromfile(inpath+'/'+name, numpy.float64, -1, '')
    #print vt.shape
    if len(vt) == numpy.prod(defN):
        _nP= numpy.prod(defPlane)
        _xa= vt[0 : _nP]
        _xb= vt[_nP : _nP + _nP]
        sa= scipy.stats.describe(_xa)
        sb= scipy.stats.describe(_xb)
        print "stats: a=", sa
        print "stats: b=", sb
        a= _xa.reshape(defPlane)
        b= _xb.reshape(defPlane)
        print "reshaped: ", a.shape, b.shape
        # NB - flatten/ravel not compatible with scipy.stats.describe()
        # print "stats: af=", scipy.stats.describe(a.reshape(-1))
        # print "stats: bf=", scipy.stats.describe(b.reshape(-1))
        # Hack fixes for Centos7 decrepit Python farce
        mma= sa[1] # .minmax
        mmb= sb[1] # .minmax
        print "mmab:", mma[0], mma[1], mmb[0], mmb[1]
        rna= rnb= 255.0
        if mma[1] > 0:
            rna= 255.0 / mma[1]
        if mmb[1] > 0:
            rnb= 255.0 / mmb[1]
        img[... ,0]= rna * a
        img[... ,2]= rnb * b
        saveFrame(outpath+'/'+prefix,img)

### ###
outpath='.'
if len(sys.argv) > 1:
    name= sys.argv[1]
    if len(sys.argv) > 2:
        outpath= sys.argv[2]
else:
    name= "raw/gsrd*F64.raw"
    outpath= 'png'

try:
    i= name.find('*')
    if i > -1:
        j= name.rfind('/')
        if j > -1 and j < i:
            #print name, j, i
            inpath=name[0:j]
            pattern=name[j+1:]
            #print inpath, pattern
        else:
            inpath='.'
            pattern= name
        flist= fnmatch.filter(os.listdir(inpath), pattern)
        for fname in flist:
            convert(inpath,fname,outpath)
    else:
        convert(name)

except KeyboardInterrupt:
    print("\nbye...\n")
    pass


