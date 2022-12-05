import socket
import numpy as np

localIP = "127.0.0.1"
localPort = 20001
bufferSize  = 65536

# Create a datagram socket
UDPServerSocket = socket.socket(family=socket.AF_INET, type=socket.SOCK_DGRAM)

# Bind to address and ip
UDPServerSocket.bind((localIP, localPort))
print("UDP server up and listening")

# Listen for incoming datagrams
while(True):
	bytesAddressPair = UDPServerSocket.recvfrom(bufferSize)
	message = bytesAddressPair[0]
	address = bytesAddressPair[1]

	print(f"Client IP Address: {address}")
	clientArray = np.fromstring(message, dtype=np.float32)
	print(f"length: {clientArray.size} content: {clientArray[-16:]}")
