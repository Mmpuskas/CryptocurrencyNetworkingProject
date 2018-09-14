# Simplified bitcoin project

The focus of this project was on the networking aspects of the client/server and peer-to-peer relationships of a cryptocurrency. The crypto aspects of cryptocurrencies were left out to maintain the focus.

# How to use
To compile:
  gcc -o miner miner.c
  gcc -o server server.c

  NOTE: When testing the program on a Windows machine using the built-in Ubuntu virtual machine, the -std=c99 flag had to be used because of compiler differences. Additionally, miner.c had to be given additional includes (“sys/select.h”, “strings.h”) and the top of the file had to have “#define _GNU_SOURCE”. Server.c had to be given “sys/select.h” as an include as well. This was not required on my Linux machine running the gcc included with Ubuntu 16.04, so code changes were not made permanent.

To run:
  First ./server to start the server.
  Then start getting the miners online: ./miner <username (20 char max)><identifier (0-9)> <IP> <port> <server IP> <server port>
  Miner must first be told to "register", then "query".
  Once miner is running, you can "transfer <identifier (0-9)> <amount (non-negative)>" to broadcast a transaction.
  You can "save <filename (20 char max)>" to tell the server to save.
  You can "clock" to print the current vector clock state.
  You can "chain" to print the current blockchain state.
  You can "deregister" to send the server a deregister command and broadcast a deregister to all peers.

# Extra notes
Protocol design decisions:
  The client/server communication happens through two structs.
    ● Struct toServerMessage is used for sending query/register/deregister/save commands to the server. The type field will be set to 0 for queries, 1 for register, 2 for deregister, and 3 for save.
      On register, the clientInfo field will be full.
      On deregister, the clientInfo field will only contain the username.
      On save, the fileName field will contain the name of the file.
    ● Struct fromServerMessage is used for returning data to the client.
      The type field will be 0 when the peers field is full, 1 when the returnCode and identifier fields are full, and 2 when only the returnCode field is full.
  The peer to peer communication happens through the blockMessage struct.
    ● The type field will be set to 0 for broadcasted transactions, 1 for broadcasted proof of works, and 2 for broadcasted deregisters.
    ● On 0 or 1, the messageBlock field will contain the relevant block. On 2, no data will be passed. (Only using message as a signal.)

Pseudocode:
P2P Client-side (Server communication, sending peer requests)
  Initialize miner (set clock to 0, initialize chain)
  Process data from CLI (set self address, name, coins, server address)
  Connect to the server
  Register with server and save ID
  Query server and save peer data
  Establish connections to peers
  Repeatedly check for user input
  On transfer command, build a new block containing the transaction, broadcast the block to all peers, then begin processing the block. When processing is done or valid proof of work is received, add the block to the chain.
  On clock command, print the current state of the vector clock
  On chain command, print the entirety of the blockchain and timestamps
  On save command, ask the server to save with the given filename
  On deregister command, ask the server to deregister and broadcast a deregister message to all peers.Check if a block is currently being processed. If so, do the next stop of processing then loop to checking user input.

P2P Server-side (Responding to peer requests) starting from client-side initialized state
  Set up socket for listening
  Check listener for new connections.
  Add to set if found
  Check sockets for messages
  If new transaction, add it to be processed by the client side
  If proof of work, check it for validity. If valid, add to blockchain and tell client-side to stop processing that block.
  If deregister, mark the peer as gone.

Server
  Set up socket for listening
  Initialize client and FD arrays.
  Loop through set of file descriptors
  Check listener for new connections
  Add to set if found
  Check sockets for messages
  If query, build a response containing the active miner set and send.
  If register, assign an ID to the client and return a message containing the ID and “SUCCESS”
  If deregister, return a message containing “SUCCESS” and remove the client from the active miner set.
  If save, use the file name to open a file, save the number of miners and minerInfo to the file, then close the file and return a message containing “SUCCESS”.
