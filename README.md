# MessageRelay
A C based messanger which uses the coordinator-participant paradigm to create a group messanger. With this app the coordinator starts up and participants are able to join using a congiuration file and send messages which are seeable up to a certain expiration time by members that join later but were already registered. All messages which the client recieves based on its join times are saved in 1001-message-log.txt.
## Features
- register [portnumber]
- deregister
- disconnect
- reconnect [portnumber]
- msend [message]
## Technologies Used
- Language: Java
- Developed in: Emacs
- Environment: Unix server
- UI: CLI
## Setup
1. Clone this repository
```bash
git clone https://github.com/jameswarren123/MessageRelay.git
```
2. Open in a compatible C environment where multiple connections are possible
3. compile with gcc -Wall coordinator.c -o coordinator -lpthread and gcc -Wall participant.c -o participant -lpthread
4. Use ./coordinator PP3-coordinator-conf.txt and ./client PP3-participant-conf.txt from their directory 1 coordinator and as many participants as wanted
