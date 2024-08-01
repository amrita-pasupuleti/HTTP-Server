# Assignment 4 directory

This directory contains source code and other files for Assignment 4.
This is a multi-threaded http server that supports get and put requests, and implements reader and writer locks in C.

# Files

Makefile <br />
README.md <br />
httpserver.c <br />
hashtable.h <br />
hashtable.c <br />
debug.h <br />
protocol.h <br />
queue.h <br />
rwlock.h <br />
asgn_2_helper_funcs.h <br />
asgn_4_helper_funcs.a <br />

## Given Header Files and Helper Functions

debug.h, protocol.h, queue.h, rwlock.h, asgn_2_helper_funcs.h, asgn_4_helper_funcs.a

## Design of hashtable.c and hashtable.h

I implemented a simple hashtable to store reader and writer locks for each file being opened in the http server.

## Design of httpserver.c

I used one mutex for adding to the hashtable of locks. I inititalized a number of threads and a worker function, in which the regex is passed as an argument. Because I could not pass multiple arguments, I created a struct for both of my regex matches. The functionality of each thread is almost the same as assingment 2, where I use a client struct to store the parsed data from the input. I then passed the client struct as an argument to the get or set functions. The input checking is less extensive here compared to assignment 2.
