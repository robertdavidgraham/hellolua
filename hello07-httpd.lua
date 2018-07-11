
-- Choose a high port, because there may already be a web server
-- on this machine, and also low-ports (below 1024) may require
-- root access
port = 8080;

onConnect = function(socket)
    local line;

    -- Grab this data for logging
    local peer = '[' .. socket:peername() .. ']:' .. socket:peerport() .. ':script: ';

    -- Grab the first line
    line = socket:receiveline();

    -- Parse it
    local method,url,major,minor = string.match(line, "(%a+)%s+(%g+)%s+HTTP/(%d+).(%d+)");

    -- Continue receiving until we get end of request header
    repeat
        line = socket:receiveline();
        if line then
            print(peer .. "line: " .. line);
        end;
    until line == "";

    -- If the parse fails, return error
    if method == nil or url == nil then
        print(peer .. "method = nil");
        socket:send("HTTP/1.1 400 Bad Request\r\nServer: hellolua07/1.0\r\n\r\n<h1>400 Bad request</h1>\r\n");
        socket:close();
        return;
    end;
    
    -- Only support the GET method
    if method ~= "GET" then
        socket:send("HTTP/1.1 405 Method Not Allowed\r\nServer: hellolua07/1.0\r\n\r\n<h1>405 Method Not Allowed</h1>\r\n");
        socket:close();
        return;
    end;

    -- Send response
    socket:send("HTTP/1.1 200 OK\r\nServer: hellolua07/1.0\r\nContent-Type: text/html\r\n\r\n<h1>Hello!</h1>\r\n");
    
    print(peer .. "closing");
end

print("httpd: script loaded");

