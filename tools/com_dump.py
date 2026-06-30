#!/usr/bin/env python3
"""Open COM7 (115200 8N1) and dump received data to the console.

Usage:
    python com_dump.py                 # COM7, 115200 8N1, hex+ascii
    python com_dump.py --port COM3     # different port
    python com_dump.py --baud 9600     # different baud
    python com_dump.py --mode text     # raw text passthrough
    python com_dump.py --out log.bin   # also write raw bytes to a file

Requires pyserial:  pip install pyserial
"""

import argparse
import sys
import time

try:
    import serial  # pyserial
except ImportError:
    sys.exit("pyserial is not installed. Run:  pip install pyserial")


def hexdump_line(offset, chunk):
    hex_part = " ".join(f"{b:02X}" for b in chunk)
    hex_part = f"{hex_part:<47}"  # 16 bytes * 3 chars - 1
    ascii_part = "".join(chr(b) if 32 <= b < 127 else "." for b in chunk)
    return f"{offset:08X}  {hex_part}  |{ascii_part}|"


def main():
    ap = argparse.ArgumentParser(description="Dump data received on a serial port.")
    ap.add_argument("--port", default="COM7", help="serial port (default: COM7)")
    ap.add_argument("--baud", type=int, default=115200, help="baud rate (default: 115200)")
    ap.add_argument("--mode", choices=["hex", "text"], default="hex",
                    help="display mode: hex dump or raw text (default: hex)")
    ap.add_argument("--out", default=None, help="optional file to append raw bytes to")
    args = ap.parse_args()

    try:
        ser = serial.Serial(
            port=args.port,
            baudrate=args.baud,
            bytesize=serial.EIGHTBITS,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE,
            timeout=0.1,
        )
    except serial.SerialException as e:
        sys.exit(f"Failed to open {args.port}: {e}")

    print(f"Opened {args.port} @ {args.baud} 8N1 (mode={args.mode}). Ctrl+C to stop.")

    outfile = open(args.out, "ab") if args.out else None
    offset = 0
    pending = bytearray()  # buffer for hex mode line alignment

    try:
        while True:
            data = ser.read(4096)
            if not data:
                continue

            if outfile:
                outfile.write(data)
                outfile.flush()

            if args.mode == "text":
                sys.stdout.write(data.decode("latin-1"))
                sys.stdout.flush()
            else:
                pending.extend(data)
                while len(pending) >= 16:
                    line = bytes(pending[:16])
                    del pending[:16]
                    print(hexdump_line(offset, line))
                    offset += 16
    except KeyboardInterrupt:
        print()
    finally:
        # flush any remaining buffered bytes in hex mode
        if args.mode == "hex" and pending:
            print(hexdump_line(offset, bytes(pending)))
        ser.close()
        if outfile:
            outfile.close()
        print("Port closed.")


if __name__ == "__main__":
    main()
