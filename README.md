# P2P-File-Sharing-System
A P2P File Sharing System 

## Environment: Unix/Linux Programming Language C++

## System Description
The system will include multiple clients (also named peers) and one central server. A peer can
join the P2P file sharing system by connecting to the server and providing a list of files it wants to
share. The server shall keep a list of all the files shared on the network. The file being distributed is
divided into chunks (e.g. a 10 MB file may be transmitted as ten 1 MB chunks). For each file, the
server shall be responsible for keeping track of the list of chunks each peer has. As a peer receives
a new chunk of the file it becomes a source (of that chunk) for other peers. When a peer intends
to download a file, it will initiate a direct connection to the relevant peers to download the file. A
peer should be able to download different chunks of the file simultaneously from many peers.

### protocol Design
Register Request/Reply 
File List Request/Reply
File Location Request/Reply
Chunk Register Request/Reply
Leave Request/Reply

### Basic Design
Multiple Connections
Parallel Downloading
Chunk Management

### How to Build
```
cd p2p
cd server
make
cd ..
cd client
make
```

### Server Side
```
cd server
./server
```

### Client Side [IP port] is optional, default port is 27890.
```
cd client
./client [IP port]
```
