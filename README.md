Project based on https://github.com/socketio/socket.io-client-cpp/tree/3.1.0

based on v3.1.0

## What is this?

The general socketio implementation is written for unix socket implementations. While lwip is based on this I wanted to get a component for esp up and running

## What this does not do

You can choose to either use polling or websocket as transport from start to finish. upgrading from polling to websocket is WIP if i get around to it.

There will definitely not be any kind of connection reuse or anything similar.

I am not using the usual subscription system of socketio. If you want to get events use esp_event internal manager with the events defined by me. 
