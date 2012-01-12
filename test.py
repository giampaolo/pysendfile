import SocketServer, os, socket, sendfile
datafile = open('sendfilemodule.c', 'rb')
datafileblocksize = os.fstat(datafile.fileno()).st_blksize
print datafileblocksize
class handler(SocketServer.BaseRequestHandler):
	def handle (self):
		if sendfile.has_sf_hdtr:
			#print sendfile.sendfile(self.request.fileno(), datafile.fileno(), 0, 0, (["HTTP/1.1 200 OK\r\n", "Content-Type: text/html\r\n", "Connection: close\r\n\r\n"], 'test'))
			print sendfile.sendfile(self.request.fileno(), datafile.fileno(), 0, 0, ["HTTP/1.1 200 OK\r\n", "Content-Type: text/html\r\n", "Connection: close\r\n\r\n"])
			#print sendfile.sendfile(self.request.fileno(), datafile.fileno(), 0, 0, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n", 'test')
		else:
			print sendfile.sendfile(self.request.fileno(), datafile.fileno(), 0, 0)
		self.request.close()
SocketServer.ThreadingTCPServer.request_queue_size = socket.SOMAXCONN
SocketServer.ThreadingTCPServer.allow_reuse_address = True
server = SocketServer.ThreadingTCPServer (('', 8079), handler)
server.serve_forever()
