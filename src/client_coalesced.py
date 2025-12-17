import socket
import struct
import time

HOST = "127.0.0.1"
PORT = 12345


def run_coalesced():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect((HOST, PORT))
    s.recv(5)

    # Prepare 3 orders back-to-back
    orders = b""
    for i in range(3):
        orders += struct.pack("!BQQIBBB", 1, 5000 + i, 100000, 10, 0, 0, 0)

    print(f"[Batch] Sending {len(orders)} bytes (3 orders) at once...")
    s.sendall(orders)

    time.sleep(1)
    s.close()


if __name__ == "__main__":
    run_coalesced()
