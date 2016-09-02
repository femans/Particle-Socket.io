#include "SocketIOClient.h"
SocketIOClient socket;

char hostname[] = "192.168.2.6";
int port = 3030;

void hello(String data) {
	Serial.println(data);
	socket.emit("hello", "hi");
}

void setup() {
	Serial.begin(115200);
	if (!socket.connect(hostname, port)) {
		Serial.println("unable to connect");
	}
	socket.on("hello", hello);
}

void loop() {
	socket.monitor();
}
