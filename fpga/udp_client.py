import socket
import numpy as np

bytesToSend = np.ones((8192), dtype=np.float32)
serverAddressPort = ("127.0.0.1", 20001)

# Create a UDP socket at client side
UDPClientSocket = socket.socket(family=socket.AF_INET, type=socket.SOCK_DGRAM)

# Send to server using created UDP socket
UDPClientSocket.sendto(bytesToSend, serverAddressPort)
