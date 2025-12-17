import socket
import struct
import time
import threading

HOST = "127.0.0.1"
PORT = 12345

# Message Types (Must match C++ enum)
MSG_ORDER_NEW = 1
MSG_ORDER_CANCEL = 2
MSG_LOGIN_RESPONSE = 5


def run_client(name):
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.connect((HOST, PORT))

        # 1. Handshake: Receive Client ID (5 bytes)
        # Type (1 byte) + ID (4 bytes)
        header = s.recv(5)
        if len(header) < 5:
            print(f"[{name}] Failed to receive handshake")
            return

        msg_type, client_id = struct.unpack("!BI", header)
        if msg_type != MSG_LOGIN_RESPONSE:
            print(f"[{name}] Invalid handshake type: {msg_type}")
            return

        print(f"[{name}] Logged in as Client ID: {client_id}")

        # 2. Send Limit Order
        # Structure: Type(1) + OrderID(8) + Price(8) + Qty(4) + Side(1) + Type(1) + TIF(1)
        # Size: 1 + 8 + 8 + 4 + 1 + 1 + 1 = 24 bytes
        # Format: !BQ Q I B B B

        # Construct Order 1001, Price 10050, Qty 10, Buy, Limit, GTC
        order_payload = struct.pack(
            "!BQQIBBB",
            MSG_ORDER_NEW,
            1001,
            100050,
            10,
            0,  # BID
            0,  # LIMIT
            0,  # GTC
        )
        s.sendall(order_payload)
        print(f"[{name}] Sent New Order 1001")

        time.sleep(1)

        # 3. Send Cancel Order
        # Structure: Type(1) + OrderID(8)
        # Size: 1 + 8 = 9 bytes
        cancel_payload = struct.pack("!BQ", MSG_ORDER_CANCEL, 1001)
        s.sendall(cancel_payload)
        print(f"[{name}] Sent Cancel Order 1001")

        time.sleep(1)
        s.close()
        print(f"[{name}] Disconnected")

    except Exception as e:
        print(f"[{name}] Error: {e}")


if __name__ == "__main__":
    run_client("Trader-A")
