# MY_smtp-Simplified-Email-Service
A simplified email transfer protocol (My_SMTP) implemented using POSIX TCP sockets in C Includes a server and client to send, receive, list, and retrieve emails over a LAN.

Features
Server: Listens on port 2525, handles multiple clients, stores emails in mailbox/<recipient>.txt, supports My_SMTP commands (HELO, MAIL FROM, RCPT TO, DATA, LIST, GET_MAIL, QUIT).
Client: Connects to the server, sends emails, lists/retrieves emails, displays server responses.
Protocol: Custom My_SMTP with defined commands and response codes (200 OK, 400 ERR etc)
