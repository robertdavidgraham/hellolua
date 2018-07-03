-- a test file ----------------

-- if you replace 'FILE' with 'io', you'll get the standard library version

x,y,z = FILE.open("hello04.lua", "r");
if x == nil then
   print(y);
else
   local xy = x:read(20);
   print(xy)
   x:close();
end


