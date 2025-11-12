#!/usr/bin/env python3

import os
import stat
import hmac
import hashlib
import struct
import json
import sys
import glob

hmacKey = None

skipData = False

packageTypes = {
    'SPIKE_1': 1,
    'GAME': 2,
    'SPIKE_2': 3,
    'SPIKE_3': 4
}

cs = 'ascii'

out = None

MAX32=0xFFFFFFFE


allocOnly = []
def alloc(do, *args):
    global allocOnly
    allocOnly += [0]
    do(*args)
    return allocOnly.pop()
    
def getOffset():
    global allocOnly
    if len(allocOnly) > 0:
        return out.tell() + sum(allocOnly)
    return out.tell()

def write(data):
    global allocOnly
    if len(allocOnly) > 0:
        allocOnly[-1] += len(data)
        return len(data)
    out.write(data)
    return len(data)

def write8(v):
    return write(struct.pack('<B', v))
def write16(v):
    return write(struct.pack('<H', v))
def write32(v):
    return write(struct.pack('<L', v))
def write64(v):
    return write(struct.pack('<Q', v))


def writeChunkHeader(magic, offset, needs64 = False):
    write(magic)
    if not needs64 and offset <= MAX32:
        write32(offset)
    else:
        write32(0xFFFFFFFF)
        write64(offset)

def writeFinf(file, strs, sdat, needs64):

    length = file['dataSize']

    if needs64:
        writeChunkHeader(b'FI64', 0x50) #FIXME
        write64(strs)
        write64(length)
        write64(sdat)
        write64(length)
        write16(file['mode'])
        write(b'\x00\x00\x00') # ???
        write(file['hmac'])
        write(file['md5'])
        write(b'\x00\x00\x00') # ???
        write(b'\x00\x00\x00\x00') # ???
    else:
        writeChunkHeader(b'FINF', 0x3C) #FIXME
        write32(strs)
        write32(length)
        write32(sdat)
        write32(length)
        write16(file['mode'])
        write(b'\x00\x00\x00') # ???
        write(file['hmac'])
        write(file['md5'])
        write(b'\x00\x00\x00') # ???
        
def align(v, alignment):
    return (alignment - (v % alignment)) % alignment

def data(path):
    return open(path, 'rb').read()

def writeSidx(type, name, shortname, version, files):
    global allocOnly

    strsAlign = 4

    sdatSize = 0
    for file in files:
        sdatSize += file['dataSize']

    needs64 = (sdatSize > MAX32)

    def doSidx():
        assert(len(name) <= 29)
        write(name + b'\x00' * (29 - len(name)))
        assert(len(shortname) <= 3)
        write(shortname + b'\x00' * (3 - len(shortname)))
        write8(version[0])
        write8(version[1])
        write8(version[2])
        write8(type)
        write(b'\x00\x00\x00\x00')

        write32(len(files))

        

        if needs64:
            write32(0xFFFFFFFF)
            writeChunkHeader(b'SZ64', 8)
            write64(sdatSize)
        else:
            write32(sdatSize)


        def doStrs():
            for file in files:
                write(file['path'].encode(cs) + b'\x00')



            
        x = alloc(doStrs)
        alignment = align(x, strsAlign)
        x += alignment
        writeChunkHeader(b'STRS', x)
        doStrs()
        write(b'\x00' * alignment)
        
        

        strs = 0
        sdat = 0
        for file in files:
            writeFinf(file, strs, sdat, needs64)
            strs += len(file['path'].encode(cs)) + 1
            sdat += file['dataSize']

        writeChunkHeader(b'FEND', 0)


    x = alloc(doSidx)
    writeChunkHeader(b"SIDX", x, needs64)
    # Align 0x30?
    doSidx()




    writeChunkHeader(b'SDAT', sdatSize if needs64 else 0)
    for file in files:
        if len(allocOnly) > 0:
            allocOnly[-1] += file['dataSize']
        else:
            print("Packing '%s'" % file['path'])
            if skipData:
                write(b'\xAA' * file['dataSize'])
            else:
                write(data(file['dataPath']))



def writeSpk0(package, needs64):
    def do():
        writeSidx(package['type'], package['name'].encode(cs), package['shortname'].encode(cs), package['version'], package['files'])
    
    x = alloc(do)
    writeChunkHeader(b"SPK0", x, needs64)
    do()
    

def writeSpks(chunks):
    def do(needs64):
        for chunk in chunks:
            writeSpk0(chunk, needs64)

    spksOffset = out.tell()

    x = alloc(do, False)
    needs64 = (x > MAX32)
    x = alloc(do, needs64)

    writeChunkHeader(b"SPKS", x)
    write32(len(chunks))
    do(needs64)

    if needs64:
        writeChunkHeader(b'SE64', 8),
        write64(spksOffset)
    else:
        writeChunkHeader(b'SEND', 4),
        write32(spksOffset)        


def package(type, name, shortname, version, files):
    return {
        'type': type,
        'name': name,
        'shortname': shortname,
        'version': version,
        'files': files
    }

def file(mode, path, dataPath, dataSize):
    

    md5Sum = hashlib.md5()
    hmacSum = None if hmacKey == None else hmac.new(hmacKey, digestmod=hashlib.sha1)

    # For testing it is often helpful to skip hashing
    if not skipData:
        #FIXME: Handle in chunks
        tmp = data(dataPath)
        assert(len(tmp) == dataSize)
        if hmacSum != None:
            hmacSum.update(tmp)
        md5Sum.update(tmp)

    return {
        'mode': mode,
        'md5': md5Sum.digest(),
        'hmac': b'\xAA' * 20 if hmacSum == None else hmacSum.digest(),
        'path': path,
        'dataPath': dataPath,
        'dataSize': dataSize
    }

def loadFiles(base, pathArray):
    files = []
    for path in pathArray:
        fullPath = base + '/' + path
        st = os.stat(fullPath)
        print("Loading %s" % fullPath)
        files += [file(st.st_mode, path, fullPath, st.st_size)]
    return files

def getPackageFoldername(type, name, version):
    if (type != packageTypes['GAME']) :
        return "%s-%d_%d_%d" % (name, *version)
    else:
        # Older spk versions only used "%s-%d_%02d", but we always use all 3
        return "%s-%d_%02d_%d" % (name, *version)
    return

def craftFromMetadata(basepath, metadata):

    if 'header' in metadata:
        #FIXME: Write in chunks to allow larger files
        header = open(basepath + '/' + metadata['header'], 'rb').read()
        write(header)

    packages = []
    for packageName, packageMetadata in metadata['packages'].items():
        packageFoldername = getPackageFoldername(packageTypes[packageMetadata['type']], packageName, packageMetadata['version'])
        packageShortname = packageMetadata.get('shortname', '')
        packageRootPath = basepath + '/' + packageFoldername + '/'
        packageFiles = [filePath for path in packageMetadata['files'] for filePath in glob.glob(path, root_dir=packageRootPath, recursive=True, include_hidden=True)]
        packages += [package(packageTypes[packageMetadata['type']], packageName, packageShortname, packageMetadata['version'], loadFiles(packageRootPath, packageFiles))]

    
    writeSpks(packages)


            

if __name__ == "__main__":
    inPath = sys.argv[1]
    outPath = sys.argv[2]

    if len(sys.argv) > 3:
        factoryKeyPath = sys.argv[3]
        hmacKey = open(factoryKeyPath, 'rb').read()[0xB0:0xC0]
    else:
        print("Missing factory-key; signatures will be incorrect")

    out = open(outPath, 'wb')
    craftFromMetadata(inPath, json.loads(open(inPath + '/metadata.json', 'rb').read().decode('utf-8')))