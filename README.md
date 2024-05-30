## Chat server

### Use
Compile the C source file:
```
gcc server.c
```

Run the server:
```
./a.out
```

In another terminal session, connect to the server:
```
nc localhost 9034
```

Write data to the connection by typing, and press enter to send.  Any other terminal sessions connected to the server will recieve the messages.
