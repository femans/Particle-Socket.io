#include "SocketIOClient.h"
SocketIOClient socket;

char hostname[] = "192.168.2.6";
int port = 3030;

void hello(String data) {
	Serial.println(data);
	socket.emit("hello", "hi there, thank you for sending me " + data);
}

void setup() {
	Serial.begin(115200);
	pinMode(D7, OUTPUT);
	if (!socket.connect(hostname, port)) {

		Serial.println("unable to connect");
	}
	socket.on("hello", hello);
}

const int interval = 2500;
unsigned int last_t = 0;
void loop() {
	unsigned int t = millis();
	if(t > last_t + interval) {
		last_t = t;
		if(! socket.connected() ) {
			digitalWrite(D7, HIGH);
			Serial.println("socket not connected, attempting to connect again");
			socket.connect(hostname, port);
		}
		else {
			digitalWrite(D7, !digitalRead(D7));
			Serial.println("sending message over socket");
			socket.emit("message", "Hi, I am a photon");
		}
	}

	/*socket.monitor();*/
}
