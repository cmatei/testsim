#!/usr/bin/env python3
"""
Simple WinKeyer3 TCP test client
Tests basic connection and protocol handshake
"""
import socket
import sys
import time

def test_winkeyer_tcp(host='localhost', port=7890):
    """Test WinKeyer3 TCP connection"""
    print(f"Connecting to {host}:{port}...")

    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(5)
        sock.connect((host, port))
        print("✓ Connected!")

        # Send Admin Open command (0x00 0x02)
        print("Sending Admin Open command (0x00 0x02)...")
        sock.send(bytes([0x00, 0x02]))

        # Read version byte
        version = sock.recv(1)
        if len(version) > 0:
            print(f"✓ Received version: {version[0]}")

        # Read status byte
        status = sock.recv(1)
        if len(status) > 0:
            print(f"✓ Received status: 0x{status[0]:02x}")

        # Send WPM speed command (0x02 0x25 = 37 WPM)
        print("Setting speed to 37 WPM (0x02 0x25)...")
        sock.send(bytes([0x02, 0x25]))
        time.sleep(0.1)

        # Send text message "TEST"
        print("Sending text: 'TEST'...")
        sock.send(b'TEST\x00')
        time.sleep(0.5)

        # Send status request (0x11)
        print("Requesting status (0x11)...")
        sock.send(bytes([0x11]))
        status = sock.recv(1)
        if len(status) > 0:
            print(f"✓ Received status: 0x{status[0]:02x}")
            if status[0] & 0x02:
                print("  - Busy flag is SET")
            else:
                print("  - Busy flag is CLEAR")

        # Send Admin Close command (0x00 0x03)
        print("Sending Admin Close command (0x00 0x03)...")
        sock.send(bytes([0x00, 0x03]))
        time.sleep(0.1)

        sock.close()
        print("\n✓ Test completed successfully!")
        return True

    except socket.timeout:
        print(f"✗ Connection timeout")
        return False
    except ConnectionRefusedError:
        print(f"✗ Connection refused - is testsim running?")
        return False
    except Exception as e:
        print(f"✗ Error: {e}")
        return False

if __name__ == '__main__':
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 7890
    success = test_winkeyer_tcp('localhost', port)
    sys.exit(0 if success else 1)
