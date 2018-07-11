
port = 7000;
print("Script loaded...");

function onConnect(socket)
	local x;

	local peer = '[' .. socket:peername() .. ']:' .. socket:peerport() .. ':script: ';

	print(peer .. "coroutine started");

	x = socket:receive();
	print(peer .. "received: " .. x);
	socket:send(x);
	print(peer .. "sent: " .. x);

	x = socket:receive();
	print(peer .. "received: " .. x);
	print("Script: received: ".. x);
	socket:send(x);
	print(peer .. "sent: " .. x);

	x = socket:receive();
	print(peer .. "received: " .. x);
	print("Script: received: ".. x);
	socket:send(x);
	print(peer .. "sent: " .. x);

	x = socket:receive();
	print(peer .. "received: " .. x);
	print("Script: received: ".. x);
	socket:send(x);
	print(peer .. "sent: " .. x);

	print(peer .. "closing");
end
