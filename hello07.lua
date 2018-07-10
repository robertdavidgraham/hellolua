port = 7000;
print("Script loaded...");

function onConnect(socket)
	local x;

	print("connection  started...");
	socket.send(socket.receive());
	socket.send(socket.receive());
	socket.send(socket.receive());

end
