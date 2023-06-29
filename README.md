Project based on https://github.com/socketio/socket.io-client-cpp/tree/3.1.0

based on v3.1.0

## What is this?

The general socketio implementation is written for unix socket implementations. While lwip is based on this I wanted to get a component for esp up and running

## What this does not do

You can choose to either use polling or websocket as transport from start to finish. upgrading from polling to websocket is WIP if i get around to it.

There will definitely not be any kind of connection reuse or anything similar.

I am not using the usual subscription system of socketio. If you want to get events use esp_event internal manager with the events defined by me. 

## Usage notice:
Any function taking an client_id locks the struct internally. This includes the packet creation functions as they internally need the namespace in order to create proper payloads.

When manually using them do not forget unlocking them.

## First connect

The first connection message is generated and negotiated automatically. From this this library gets the session ID and various timeout values amongst other things. 

After this though the first message that needs to be sent depends on your use-case. In general it goes like this (random token omitted):
Client -> Server: GET from endpoint (EIO version, transport, etc. in the query)
Client <- Server: response '0{"sid":XXX,"upgrades":XXX, ...}' 0. The first number indicated EIO type a second one would indicate SIO type. 0 = Connect

Client -> Server: POST with same info as first GET (just with another token, and the sid added gotten from the server before). Data = '40{/namespace,}{auth-body}'. (the '/' after '40' is only needed if you are using a ns, as is the ',' after the ns). (the body for the auth request is gotten from the clients getter function (since these can be timestamped))
Client <- Server: '44XXXX' this is different for everyone so you have to handle yourself after this point


After this all communication is typically in '44' messages. Only exception are heartbeat messages which the library handles for you.


## http client usage:

### polling
For the initial handshake a http client is allocated, its used to connect to the endpoint.
For polling the handshake client is reused.

For posting a new client is created when doing it for the first time at which point it is also reused.