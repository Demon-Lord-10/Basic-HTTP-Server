# Socket


Simple Multithreaded HTTP Server in C
=====================================

A basic HTTP/1.1 server written in C that supports multiple clients using POSIX threads (pthread).
The server can respond to different routes, echo text, serve files, and display the client’s User-Agent.

Features
--------
- Handles multiple clients concurrently (multithreaded with pthreads)
- Serves static files from the server directory
- Simple routing:
    /           — Home page
    /hello      — Hello page
    /echo/{text} — Echoes {text} as plain text
    /file/{filename} — Serves the specified file if it exists
    /user-agent — Displays the client's User-Agent string
- Returns appropriate HTTP status codes (200, 400, 404, 500)
- Well-commented and modular code

How to Build
------------
gcc -o http_server http_server.c -lpthread

How to Run
----------
./http_server

The server listens by default on http://localhost:4221

Example Usage
-------------
Home:
    curl http://localhost:4221/
Hello:
    curl http://localhost:4221/hello
Echo:
    curl http://localhost:4221/echo/yourtext
File:
    Place a file (e.g., test.txt) in the server directory, then:
    curl http://localhost:4221/file/test.txt
User-Agent:
    curl http://localhost:4221/user-agent

Project Structure
-----------------
- http_server.c — Main server source code
- Files to be served should be in the same directory as the server


Learning Highlights
-------------------
- Sockets and network programming in C
- Multithreading with pthreads
- HTTP protocol basics
- File I/O and error handling
- Writing readable, well-documented code

License
-------
MIT License

Author: [Demon-Lord-10]
