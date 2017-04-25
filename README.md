# Chat
Console chat for windows. Clients connect to Server and communicate.<br>
Client can send broadcast message to all other connected clients and private messages. Plus client can request a list of connected users and to change nickname.<br>
Internaly based on tcp sockets. On server side each client runs in separate thread. Client runs in two threads - one for user input and one for receiving data from server.
