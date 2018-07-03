-- a test file ----------------

-- This version of the script removes the close() method, so the
-- object is cleaned up during garbage collection instead of immediately

x,y,z = FILE.open("hello04.lua", "r");
if x == nil then
   print(y);
else
   local xy = x:read(20);
   print(xy)
   -- x:close();
end


