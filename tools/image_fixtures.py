"""Deterministic PNG fixtures, generated without an imaging dependency."""
import struct
import zlib


def image_pattern(width=1600,height=1000):
    def chunk(kind,data):
        return struct.pack(">I",len(data))+kind+data+struct.pack(">I",zlib.crc32(kind+data))
    rows=[]
    for y in range(height):
        row=bytearray(b"\0")
        for x in range(width):
            if y<height//2: color=(192,208,214) if x<width//2 else (226,220,197)
            else: color=(96,125,110) if x<width//2 else (144,164,133)
            if x%200<3 or y%200<3: color=(250,249,246)
            row.extend(color)
        rows.append(row)
    return b"\x89PNG\r\n\x1a\n"+chunk(b"IHDR",struct.pack(">IIBBBBB",width,height,8,2,0,0,0))+chunk(b"IDAT",zlib.compress(b"".join(rows)))+chunk(b"IEND",b"")
