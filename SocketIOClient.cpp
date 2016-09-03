/*
socket.io-particle-client: a Socket.IO client for the Particle Photon

Based on the Kevin Rohling WebSocketClient & Bill Roy Socket.io Lbrary
Copyright 2015 Florent Vidal
Supports Socket.io v1.x
Permission is hereby granted, free of charge, to any person
obtaining a copy of this software and associated documentation
files (the "Software"), to deal in the Software without
restriction, including without limitation the rights to use,
copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following
conditions:
The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.
*/

/*
* Modified by RoboJay
* and by your grandpa
*/

#include <SocketIOClient.h>

SocketIOClient::SocketIOClient() {
	for (int i = 0; i < MAX_ON_HANDLERS; i++) {
		onId[i] = "";
	}
}

bool SocketIOClient::connect(String thehostname, int theport) {
	thehostname.toCharArray(hostname, MAX_HOSTNAME_LEN);
	port = theport;

	if (!tcp.connect(hostname, theport)){ return false; }

	sendHandshake(hostname);
	return readHandshake();
}

bool SocketIOClient::connectHTTP(String thehostname, int theport) {
	thehostname.toCharArray(hostname, MAX_HOSTNAME_LEN);
	port =  theport;
	if (!tcp.connect(hostname, theport)){ return false; }
	return true;
}

bool SocketIOClient::reconnect(String thehostname, int theport) {
	thehostname.toCharArray(hostname, MAX_HOSTNAME_LEN);
	port = theport;

	if (!tcp.connect(hostname, theport)){ return false; }

	sendHandshake(hostname);
	return readHandshake();
}

bool SocketIOClient::connected() { return tcp.connected();}

void SocketIOClient::disconnect() { tcp.stop(); }

void SocketIOClient::eventHandler(int index) {
	String id = "";
	String data = "";
	String rcvdmsg = "";
	int sizemsg = databuffer[index + 1]; // 0-125 byte, index ok Fix provide by Galilei11. Thanks
	if (databuffer[index + 1]>125) {
		sizemsg = databuffer[index + 2];   // 126-255 byte
		index += 1;                        // index correction to start
	}
	#ifdef IODEBUG
	Serial.print("[eventHandler] Message size = ");	Serial.println(sizemsg);
	#endif
	for (int i = index + 2; i < index + sizemsg + 2; i++) {
		rcvdmsg += (char)databuffer[i];
	}
	#ifdef IODEBUG
	Serial.print("[eventHandler] Received message = ");Serial.println(rcvdmsg);
	#endif
	switch (rcvdmsg.charAt(0)) {
		case '2':
		#ifdef IODEBUG
		Serial.println("[eventHandler] Ping received - Sending Pong");
		#endif
		heartbeat(1);
		break;
		case '3':
		#ifdef IODEBUG
		Serial.println("[eventHandler] Pong received - All good");
		#endif
		break;
		case '4':
		switch (rcvdmsg.charAt(1)) {
			case '0':
			#ifdef IODEBUG
			Serial.println("[eventHandler] Upgrade to WebSocket confirmed");
			#endif
			break;
			case '2':
			id = rcvdmsg.substring(4, rcvdmsg.indexOf("\","));
			#ifdef IODEBUG
			Serial.println("[eventHandler] id = " + id);
			#endif
			data = rcvdmsg.substring(rcvdmsg.indexOf("\",") + 3, rcvdmsg.length () - 2);
			#ifdef IODEBUG
			Serial.println("[eventHandler] data = " + data);
			#endif
			for (uint8_t i = 0; i < onIndex; i++) {
				if (id == onId[i]) {
					#ifdef IODEBUG
					Serial.println("[eventHandler] Found handler = " + String(i));
					#endif
					(*onFunction[i])(data);
				}
			}
			break;
		}
	}
}

bool SocketIOClient::monitor() {
	int index = -1;
	int index2 = -1;
	String tmp = "";
	*databuffer = 0;
	static unsigned long pingTimer = 0;

	if (!tcp.connected()) {
		#ifdef IODEBUG
		Serial.println("[monitor] Client not connected.");
		#endif
		if (connect(hostname, port)) {
			#ifdef IODEBUG
			Serial.println("[monitor] Connected.");
			#endif
			return true;
		} else {
			#ifdef IODEBUG
			Serial.println("[monitor] Can't connect. Aborting.");
			#endif
		}
	}

	// the PING_INTERVAL from the negotiation phase should be used
	// this is a temporary hack
	if (tcp.connected() && millis() >= pingTimer) {
		heartbeat(0);
		pingTimer = millis() + PING_INTERVAL;
	}

	if (!tcp.available()){ return false;}
	while (tcp.available()) { // read availible tcp
		readLine();
		tmp = databuffer;
		dataptr = databuffer;
		index = tmp.indexOf((char)129);	//129 DEC = 0x81 HEX = sent for proper communication
		index2 = tmp.indexOf((char)129, index + 1);
		if (index != -1)  { eventHandler(index); }
		if (index2 != -1) { eventHandler(index2);}
	}
	return false;
}

void SocketIOClient::sendHandshake(char hostname[]) {
	tcp.println(F("GET /socket.io/1/?transport=polling&b64=true HTTP/1.1"));
	tcp.print(F("Host: "));
	tcp.println(hostname);
	tcp.println(F("Origin: Arduino\r\n"));
}

bool SocketIOClient::waitForInput(void) {
	unsigned long now = millis();
	while (!tcp.available() && ((millis() - now) < 30000UL)) { ; }
	return tcp.available();
}

void SocketIOClient::eatHeader(void) {
	while (tcp.available()) {	// consume the header
		readLine();
		if (strlen(databuffer) == 0) { break; }
	}
}

bool SocketIOClient::readHandshake() {
	if (!waitForInput()){ return false; }
	// check for happy "HTTP/1.1 200" response
	readLine();
	if (atoi(&databuffer[9]) != 200) {
		while (tcp.available()) readLine();
		tcp.stop();
		return false;
	}
	eatHeader();
	readLine();
	String tmp = databuffer;

	int sidindex = tmp.indexOf("sid");
	int sidendindex = tmp.indexOf("\"", sidindex + 6);
	int count = sidendindex - sidindex - 6;

	for (int i = 0; i < count; i++) {
		sid[i] = databuffer[i + sidindex + 6];
	}
	#ifdef IODEBUG
	Serial.print(F("[readHandshake] Connected. SID="));
	Serial.println(sid);	// sid:transport:timeout
	#endif

	while (tcp.available()) readLine();
	tcp.stop();
	delay(1000);

	// reconnect on websocket connection
	if (!tcp.connect(hostname, port)) {
		#ifdef IODEBUG
		Serial.print(F("[readHandshake] Websocket failed."));
		#endif
		return false;
	}
	#ifdef IODEBUG
	Serial.println(F("[readHandshake] Connecting via Websocket"));
	#endif

	tcp.print(F("GET /socket.io/1/websocket/?transport=websocket&b64=true&sid="));
	tcp.print(sid);
	tcp.print(F(" HTTP/1.1\r\n"));
	tcp.print(F("Host: "));
	tcp.print(hostname);
	tcp.print("\r\n");
	tcp.print(F("Origin: ArduinoSocketIOClient\r\n"));
	tcp.print(F("Sec-WebSocket-Key: "));
	tcp.print(sid);
	tcp.print("\r\n");
	tcp.print(F("Sec-WebSocket-Version: 13\r\n"));
	tcp.print(F("Upgrade: websocket\r\n"));	// must be camelcase ?!
	tcp.println(F("Connection: Upgrade\r\n"));

	if (!waitForInput()) { return false; }
	readLine();
	if (atoi(&databuffer[9]) != 101) {	// check for "HTTP/1.1 101 response, means Updrage to Websocket OK
	while (tcp.available()) { readLine(); }
	tcp.stop();
	return false;
}
readLine();
readLine();
readLine();
for (int i = 0; i < 28; i++) {
	key[i] = databuffer[i + 22];	//key contains the Sec-WebSocket-Accept, could be used for verification
}
eatHeader();

/*
Generating a 32 bits mask requiered for newer version
Client has to send "52" for the upgrade to websocket
*/
randomSeed(analogRead(0));
String mask = "";
String masked = "52";
String message = "52";
for (int i = 0; i < 4; i++) {	//generate a random mask, 4 bytes, ASCII 0 to 9
	char a = random(48, 57);
	mask += a;
}
for (unsigned int i = 0; i < message.length(); i++){
	//apply the "mask" to the message ("52")
	masked.setCharAt(i, message.charAt(i) ^ mask.charAt(i % 4));
}

tcp.print((char)0x81);	//has to be sent for proper communication
tcp.print((char)130);	//size of the message (2) + 128 because message has to be masked
tcp.print(mask);
tcp.print(masked);

monitor();		// treat the response as input
return true;
}

void SocketIOClient::getREST(String path) {
	String message = "GET /" + path + "/ HTTP/1.1";
	tcp.println(message);
	tcp.print(F("Host: ")); tcp.println(hostname);
	tcp.println(F("Origin: Arduino"));
	tcp.println(F("Connection: close\r\n"));
}

void SocketIOClient::postREST(String path, String type, String data) {
	String message = "POST /" + path + "/ HTTP/1.1";
	tcp.println(message);
	tcp.print(F("Host: "));           tcp.println(hostname);
	tcp.println(F("Origin: Arduino"));
	tcp.println(F("Connection: close\r\n"));
	tcp.print(F("Content-Length: ")); tcp.println(data.length());
	tcp.print(F("Content-Type: "));   tcp.println(type);
	tcp.println("\r\n");
	tcp.println(data);

}

void SocketIOClient::putREST(String path, String type, String data) {
	String message = "PUT /" + path + "/ HTTP/1.1";
	tcp.println(message);
	tcp.print(F("Host: "));           tcp.println(hostname);
	tcp.println(F("Origin: Arduino"));
	tcp.println(F("Connection: close\r\n"));
	tcp.print(F("Content-Length: ")); tcp.println(data.length());
	tcp.print(F("Content-Type: "));   tcp.println(type);
	tcp.println("\r\n");
	tcp.println(data);
}

void SocketIOClient::deleteREST(String path) {
	String message = "DELETE /" + path + "/ HTTP/1.1";
	tcp.println(message);
	tcp.print(F("Host: ")); tcp.println(hostname);
	tcp.println(F("Origin: Arduino"));
	tcp.println(F("Connection: close\r\n"));
}

void SocketIOClient::readLine() {
	for (int i = 0; i < DATA_BUFFER_LEN; i++) { databuffer[i] = ' '; }
	dataptr = databuffer;
	while (tcp.available() && (dataptr < &databuffer[DATA_BUFFER_LEN - 2])){
		char c = tcp.read();
		//TODO: figure out what should be here
		if ((c == 0) || (c == 255) || (c == '\r')) {;}
		else if (c == '\n') break;
		else *dataptr++ = c;
	}
	*dataptr = 0;
}


void SocketIOClient::emit(String id, String data) {
	String message = "42[\"" + id + "\",\"" + data + "\"]";
	unsigned int msglength = message.length();
	#ifdef IODEBUG
	Serial.printf("[emit] " + message + " - message length: %u\n", msglength);
	#endif
	int header[10];
	header[0] = 0x81;
	#ifdef IODEBUG
	#endif
	randomSeed(analogRead(0));
	String mask = "";
	String masked = message;
	for (int i = 0; i < 4; i++) {
		char a = random(48, 57);
		mask += a;
	}
	for (uint64_t i = 0; i < msglength; i++){
		//apply the "mask" to the message ("52")
		masked.setCharAt(i, message.charAt(i) ^ mask.charAt(i % 4));
	}

	tcp.print((char)header[0]);	// has to be sent for proper communication
	// Depending on the size of the message
	if (msglength <= 125) {
		header[1] = msglength + 128;
		tcp.print((char)header[1]);	//size of the message + 128 because message has to be masked
	} else if (msglength >= 126 && msglength <= 65535) {
		header[1] = 126 + 128;
		tcp.print((char)header[1]);
		header[2] = (msglength >> 8) & 255;
		tcp.print((char)header[2]);
		header[3] = (msglength)& 255;
		tcp.print((char)header[3]);
	} else {
		header[1] = 127 + 128;
		tcp.print((char)header[1]);
		header[2] = (msglength >> 56) & 255;
		tcp.print((char)header[2]);
		header[3] = (msglength >> 48) & 255;
		tcp.print((char)header[3]);
		header[4] = (msglength >> 40) & 255;
		tcp.print((char)header[4]);
		header[5] = (msglength >> 32) & 255;
		tcp.print((char)header[5]);
		header[6] = (msglength >> 24) & 255;
		tcp.print((char)header[6]);
		header[7] = (msglength >> 16) & 255;
		tcp.print((char)header[7]);
		header[8] = (msglength >> 8) & 255;
		tcp.print((char)header[8]);
		header[9] = (msglength)& 255;
		tcp.print((char)header[9]);
	}
	tcp.print(mask);
	tcp.print(masked);
}

void SocketIOClient::heartbeat(int select) {
	randomSeed(analogRead(0));
	String mask = "";
	String masked = "";
	String message = "";
	if (select == 0) {
		masked = "2";
		message = "2";
	} else {
		masked = "3";
		message = "3";
	}
	for (int i = 0; i < 4; i++) { //generate a random mask, 4 bytes, ASCII 0 to 9
		char a = random(48, 57);
		mask += a;
	}
	for (unsigned int i = 0; i < message.length(); i++){
		//apply the "mask" to the message ("2" : ping or "3" : pong)
		masked.setCharAt(i, message.charAt(i) ^ mask.charAt(i % 4));
	}
	tcp.print((char)0x81);	//has to be sent for proper communication
	tcp.print((char)129);	//size of the message (1) + 128 because message has to be masked
	tcp.print(mask);
	tcp.print(masked);
}

void SocketIOClient::on(String id, functionPointer function) {
	if (onIndex == MAX_ON_HANDLERS){ return; }
	onFunction[onIndex] = function;
	onId[onIndex] = id;
	onIndex++;
}
