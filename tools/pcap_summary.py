#!/usr/bin/env python3
import struct
import sys


def mac(data):
    return ":".join(f"{byte:02x}" for byte in data)


def ipv4(data):
    return ".".join(str(byte) for byte in data)


def main():
    if len(sys.argv) != 2:
        print("usage: pcap_summary.py <file>", file=sys.stderr)
        return 2

    with open(sys.argv[1], "rb") as file:
        data = file.read()

    offset = 24
    index = 0
    while offset + 16 <= len(data):
        _ts_sec, _ts_usec, included, _original = struct.unpack_from("<IIII", data, offset)
        offset += 16
        packet = data[offset:offset + included]
        offset += included
        index += 1

        if len(packet) < 14:
            print(f"{index}: short len={len(packet)}")
            continue

        ethertype = struct.unpack("!H", packet[12:14])[0]
        line = f"{index}: {mac(packet[6:12])}->{mac(packet[:6])} eth=0x{ethertype:04x} len={len(packet)}"

        if ethertype == 0x0806 and len(packet) >= 42:
            op = struct.unpack("!H", packet[20:22])[0]
            line += f" arp op={op} {ipv4(packet[28:32])}->{ipv4(packet[38:42])}"
        elif ethertype == 0x0800 and len(packet) >= 34:
            ip = packet[14:]
            protocol = ip[9]
            line += f" ip proto={protocol} {ipv4(ip[12:16])}->{ipv4(ip[16:20])}"
            header_length = (ip[0] & 0x0f) * 4
            if protocol == 6 and len(ip) >= header_length + 20:
                tcp = ip[header_length:]
                src_port, dst_port = struct.unpack("!HH", tcp[:4])
                seq, ack = struct.unpack("!II", tcp[4:12])
                flags = tcp[13]
                data_offset = (tcp[12] >> 4) * 4
                payload = len(ip) - header_length - data_offset
                line += f" tcp {src_port}->{dst_port} flags=0x{flags:02x} seq={seq} ack={ack} payload={payload}"
                if payload:
                    sample = tcp[data_offset:data_offset + min(payload, 32)]
                    line += f" data={sample!r}"

        print(line)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
