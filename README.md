# Chat

Simple chat written in C.

## Usage

To start server type: 

  $ make
  $ ./server [port]
  
Port parameter is optional. If not set, it will
be listening on `20160`.

To start client type:

  $ make
  $ ./client [host] [port]
  
To connect client to server we need to provide host
(if testing on one machine just type `localhost`).
Port parameter is optional. If not set, it will try
to connect to port `20160`.
