import os
import re
from argparse import ArgumentParser

# Not: On the PICO there apparently is pico_set_lwip_httpd_content
# Overwrite specific settings per filename
# The following can be overwritten
#   content_type
#   content_encoding
#   cache_control
#   headers_included Can be set to True or False. When set to true it's just headers, for example redirects

SPECIALS = {
    "/index.html.br": {
        "cache_control": 300
    },
    "/index.html.gz": {
        "cache_control": 300
    },
    "/hotspot-detect.html": {
        "cache_control": 1
    },
    "/generate_204": {
        "headers_included": True
    },
    "/gen_204": {
        "headers_included": True
    },
    "/ios.html": {
        "cache_control": 0
    },
    "/captive.html": {
        "cache_control": 0
    }
}

# Content-type:
CONTENT_TYPES = {
    "css":  "text/css",
    "html": "text/html",
    "json": "application/json",
    "js":   "application/javascript",
    "txt":  "text/plain",
    "_":    "text/plain"
}

# Cache-Control:
CACHE_CONTROL = {
    "_": 31536000
}

# Content-Encoding:
CONTENT_ENCODINGS = {
    "br": "br",
    "gz": "gzip",
    "_":  "text"
}

def removeEncodingExtension(fileName):
    parts = fileName.split('.')
    if parts[-1] in CONTENT_ENCODINGS:
        return '.'.join(parts[:-1])
    return fileName

def readFile(fileName):
    with open(fileName, 'rb') as f:
        fileData = bytearray(f.read())
    return fileData, len(fileData)  

def getFormattedName(fileName):
    return removeEncodingExtension(fileName).replace('.', '_').replace('-', '_').replace(' ', '_')[1:]

def generateHeader(filePath, fileName, contentLength):
    header = bytearray()
    # Extract the extensions from the fileName
    parts = fileName.split('.')

    # Check if the filename matches any entry in the SPECIALS hashmap
    if len(parts) > 2:  # For files like file.html.gz
        contentType = CONTENT_TYPES.get(parts[-2], CONTENT_TYPES['_'])
    else:  # For files like file.html
        contentType = CONTENT_TYPES.get(parts[-1], CONTENT_TYPES['_'])

    contenEncoding = CONTENT_ENCODINGS.get(parts[-1], CONTENT_ENCODINGS['_'])
    cacheControl = CACHE_CONTROL.get(parts[-1], CACHE_CONTROL['_'])

    # OVerwrite specials
    specialSettings = SPECIALS.get(fileName, {})        
    contentType = specialSettings.get('content_type', contentType)
    contenEncoding = specialSettings.get('content_encoding', contenEncoding)
    cacheControl = specialSettings.get('cache_control', cacheControl)

    print(f"{fileName} {contentType}")
    
    # Create Headers
    header.extend(f"HTTP/1.0 200 OK\r\n".encode('utf-8'))
    header.extend(f"Content-type: {contentType}\r\n".encode('utf-8'))
    header.extend(f"Content-Encoding: {contenEncoding}\r\n".encode('utf-8'))
    header.extend(f"Content-Length: {contentLength}\r\n".encode('utf-8'))
    header.extend(f"Cache-Control: public, max-age={cacheControl}\r\n".encode('utf-8'))
    header.extend(f"Server: GATAS\r\n".encode('utf-8'))
   
    return header


def generateEntry(filePath, fileName):
    file = bytearray()
    fileData, fileLength = readFile(filePath)

    # fileName, null terminated, remove the encodings
    file.extend(removeEncodingExtension(fileName).encode('utf-8'))
    file.append(0)
    
    specialSettings = SPECIALS.get(fileName, {})     

    # A file can also be a header. Do ensure it's not encrypted! (experimental)
    if not specialSettings.get('headers_included', False):
        file.extend(generateHeader(filePath, fileName, fileLength))
        file.extend("\r\n".encode('utf-8'))
    else:
        # Fix any line endings in the complete file
        lines = re.split(r'\r?\n', fileData.decode('utf-8'))
        normalized = [line.strip() + '\r\n' for line in lines]
        fileData = (''.join(normalized)).encode('utf-8')

    file.extend(fileData)
    return file


def writeCArray(output, fileData, fileName):
    formattedName = getFormattedName(fileName)
    output.write(f'const unsigned char __in_flash() data_{formattedName}[] = {{\n\t')

    for i in range(0, len(fileData), 10):
        chunk = fileData[i:i+10]
        line = ', '.join(f"0x{byte:02x}" for byte in chunk)
        output.write(line)
        output.write(",\n\t" if i + 10 < len(fileData) else "\n")

    output.write("};\n\n")

def writeFileStructs(output, directory, files):
    numFiles = len(files)
    lastFileName = "NULL"
    for filePath in files:
        fileName = filePath.replace(directory, "")
        filenameFormatted = getFormattedName(fileName)
        fileNameNoEncoding = removeEncodingExtension(fileName)
        fileNameLength = len(fileNameNoEncoding) + 1
        output.write(f"const struct fsdata_file file_{filenameFormatted}[] = {{{{{lastFileName}, data_{filenameFormatted}, data_{filenameFormatted} + {fileNameLength}, sizeof(data_{filenameFormatted}) - {fileNameLength}, FS_FILE_FLAGS_HEADER_INCLUDED}}}};\n")
        lastFileName = f"file_{filenameFormatted}"

    output.write(f"\n\n")
    output.write(f"#define FS_ROOT {lastFileName}\n")
    output.write(f"#define FS_NUMFILES {numFiles}\n")

def main(directory, fileName):

        # Open the output file for writing
    with open(fileName, 'w') as output:

        # Get all files in the 'fs' directory
        files = []
        for root, _, fileNames in os.walk(directory):
            for fileName in fileNames:
                files.append(os.path.join(root, fileName))
        files.sort()
        
        for filePath in files:
            fileName = filePath.replace(directory, "")
            fileData = generateEntry(filePath, fileName)
            writeCArray(output, fileData, fileName)

        writeFileStructs(output, directory, files)

# get arguments and start processing
parser = ArgumentParser()
parser.add_argument("-d", "--directory", dest="directory",
                    help="Directory of source files", metavar="FILE")
parser.add_argument("-o", "--output",
                    dest="fileName", default=True,
                    help="Output fileName")
args = parser.parse_args()

if __name__ == '__main__':
    main(args.directory, args.fileName)
    print(f"Generated {args.fileName}")
