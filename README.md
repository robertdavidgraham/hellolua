Hello, Lua!
===========

This project demonstrates integration of Lua into a C program.
Each file represents a single concept in a small, minimalistic
fashion, without a lot of unnecessary cruft that's not helpful
in demonstrating that one concept.

I created these as I was learning to integrate Lua into my own
project. My project wants millions of coroutines/user-threads processing
TCP connections, like the  *OpenResty* project (used in *nginx* and *CloudFlare*) 
rather than something like *World of Warcraft*.

If like me, you are learning how to integrate Lua into C, then the first thing I
recommend is buying the book *Programming in Lua (Fourth Edition)*, the
book written by the author of Lua. It explains important concepts well that
I was struggling to learn by reading the online reference manual or googling
on StackExchange.

The other source of information is to simply read Lua source code itself.
Often times the code is far easier to understand than the description of 
what the function is supposed to do. In particular, read the libraries. These
already do what you are trying to do it, integrate external code with Lua.
For example, the *io* library is a wrapper around the *FILE* I/O that you
already understand, so watching how they integrate it is extremely
educational.

hello01 - Getting a script to run
--------
This project is just about learning how to run a Lua script in the first place, which is extremely
easy. It's just

    L = luaL_newstate();
    luaL_openlibs(L);
    luaL_dofile(L, filename);
    lua_close(L);
    
However, I do add the complexity of spliting up the *dofile()* function into separate *loadfile()*
and *pcall()* steps. It's important to understand compile-time vs. run-time errors.
I include two scripts *hello01-syntaxerr.lua* and *hello01-runerr.lua* to highlight how these
errors are handled.

The thing to learn from this lesson is the variable *L* which represents the Lau virtual
machine (VM). Every function we call will have *L* as the first parameter. (There's nothing
special about the variable name, but *L* seems to be what most example source
code uses. Variable names should be shorter the more often they are used. The only
better name for this variable would be *$*, as it is in JQuery.)


hello02 - Data between Lua and C
--------
In this project, I show some first steps at geting data into and out of scripts. Baby steps.

The thing to learn from this lesson is the *stack*. We don't access Lua data directly. Instead,
we move it onto the stack, then grab it out of the stack. Or, we move it onto the stack, and
then move it into Lua. Stack stack stack stack stack stack stack.


hello03 - Lua calling C functions
--------
This project shows how to call a C function. The C function Lua calls take just the 
VM variable *L*, wich contains the stack that we'll push/pop from.

Stack indexes are positive, from the bottom of the stack upwards, for function parameters.
Important to note this, because outside of functions, stack indexes are usually negative,
from the top down, to reference things we've recently pushed onto the stack.

The thing to learn here is that we can't simply grab parameters but also much error
check them. Part of understanding this is that the function may return within the middle:
if an error is found, a "longjmp()" will be executed that unrolls the C stack. This is a
little known feature of C you should read up on. Among other things, if you've
malloc()ed memory before the error occurs, but before you've stashed it somewhere,
errors will cause memory leaks.


hello04 - userdata objects
--------
This project is probably skipping several simpler steps. The thing is that the simpler steps
aren't used alone. You can't simply allocate *userdata* and not keep track of type
information and have a finalizer. And while you are at it, you might as well wrap it in
a full *Object*, so we might as well learn about the object-oriented system alongside it.

Like in JavaScript, there is no inherent object-oriented system. Instead, you mess around
with things like *metatables* to create the effect. It actually works well, but you have to
follow the pattern to make it work.

One of the key things to learn here is *type safety*. This is both an important concern for
stability as well as for cybersecurity. What that means here is that we give the *metatables*
a unique name, and check it to verify the type of a parameter. Search for *MY_FILE_CLASS*
in the file -- this is the name we use identify the type of the object.


hello05 - coroutine threads
--------
For large scale network applications, the power of Lua is to create a coroutine per TCP
connection, as in OpenResty.

This example is doing file reads like the last example, but instead of within a C method
by exiting the script (temporarilty), doing the read, then resuming the script. This is, of course,
a stupid way to do file reads. Instead, it's how I plan to do networking, with a million concurrent
TCP connections, where all the threads are paused waiting for data to arrive.


hello06 - memory usage
--------
This example measures how much memory is being used by things. It's a little off because
sometimes memory isn't freed, because of garbage collection. On my macOS laptop,
I get the following:

    newstate  =   4593 bytes,   54 allocs,    0 frees
    openlibs  =  16644 bytes,  240 allocs,   23 frees
    newthread =    848 bytes,    2 allocs,    0 frees
    loadstring=    520 bytes,   20 allocs,    8 frees
    pcall     =    187 bytes,    4 allocs,    0 frees
    gc        = 4294966556 bytes,    1 allocs,   16 frees

A `newstat` VM is 4k. Registering the libraries is the most expensive park, at 16k.

Each new thread can be under 1k in size, which is attractive for a server trying to support
a massive number (millions) of concurrent threads. The amount of memory needed to handle
those connections is greater than the minimal overheaded needed for Lua. On the other hand,
the amount of memory a Lua thread needs will grow as it defines variables and such. Also,
the dynamic memory required will cause a zillion small allocations all over the place, which
will stress the garbage collection.









