var net = require('net');
var dgram = require('dgram');

var rxReportPort = 51111;
var rxReceivePort = 51112;
var txCommandPort = 52222;

var rxBufferReadIndex = 0;
var rxBufferWriteIndex = 0;
var rxBufferCount = 0;
var rxBufferMax = 10000;
var rxBufferOverflow = false;
var rxBufferOverflowGuard = 10;
var rxBuffer = new Array(rxBufferMax);

var reportTimer;
var reportStream;
var reportActive = false;
var reportConnected = false;

var txAddress;
var txPort;
var txCount;
var txBytes;
var txDelay;
var txTtl;

var txStarted;
var txTimer;
var txFrame;
var txMessage;

function rxReport() {
	if (--rxBufferCount == 0) {
		reportActive = false;
  		clearInterval(reportTimer);
	}

	var entry = rxBuffer[rxBufferReadIndex];
	
	rxBuffer[rxBufferReadIndex++] = null;
	
	if (rxBufferReadIndex == rxBufferMax) {
		rxBufferReadIndex = 0;
	}

	reportStream.write(entry);
}

var rxServer = net.createServer(function (stream) {
	reportStream = stream;
	
	stream.on('connect', function () {
	    console.log('RX Connected');
    	reportConnected = true;
    	if (rxBufferCount > 0) {
    		reportActive = true;
    		reportTimer = setInterval(rxReport, 1);
    	}
  	});
  
	stream.on('end', function () {
    	console.log('RX Disconnected');
    	reportConnected = false;
    	reportActive = false;
  		clearInterval(reportTimer);
    	stream.end();
  	});
});

var rxReceiver = dgram.createSocket("udp4");

rxReceiver.on("message", function (msg, rinfo) {
  	var timestamp = new Date().getTime() * 1000;
  
  	var entry = new Buffer(16);
  
  	for (var i = 0; i < 12; i++) {
  		entry[i] = msg[i];
  	}
  
  	entry[12] = (timestamp >>> 24) & 0xFF;
  	entry[13] = (timestamp >>> 16) & 0xFF;
  	entry[14] = (timestamp >>> 8) & 0xFF;
  	entry[15] = timestamp & 0xFF;

	if (rxBufferOverflow) {
		if (rxBufferCount < (rxBufferMax - rxBufferOverflowGaurd)) {
			var blown = new Buffer(16);

			blown.Fill(0);

			rxBufferCount++;
	
			rxBuffer[rxBufferWriteIndex++] = blown;
	
			if (rxBufferWriteIndex == rxBufferMax) {
				rxBufferWriteIndex = 0;
			}

			rxBufferCount++;
	
			rxBuffer[rxBufferWriteIndex++] = entry;
	
			if (rxBufferWriteIndex == rxBufferMax) {
				rxBufferWriteIndex = 0;
			}
			
			rxBufferOverflow = false;
			
			if (reportConnected && !reportActive) {
	    		reportActive = true;
	    		reportTimer = setInterval(rxReport, 1);
			}
		}
	}
	else {
		if (rxBufferCount < rxBufferMax) {
			rxBufferCount++;
	
			rxBuffer[rxBufferWriteIndex++] = entry;
	
			if (rxBufferWriteIndex == rxBufferMax) {
				rxBufferWriteIndex = 0;
			}

			if (reportConnected && !reportActive) {
	    		reportActive = true;
	    		reportTimer = setInterval(rxReport, 1);
			}
		}
		else {
			rxBufferOverflow = true;
		}
	}
});

function txError(stream, txMessage) {
	var error = "ERROR " + txMessage;
    console.log("TX " + error);
    stream.write(error);
    stream.write("\n");
}

function txSend() {
	txFrame++;

    txMessage[4] = (txFrame >>> 24) & 0xFF;
    txMessage[5] = (txFrame >>> 16) & 0xFF;
    txMessage[6] = (txFrame >>> 8) & 0xFF;
    txMessage[7] = txFrame & 0xFF;
    
	var timestamp = new Date().getTime() * 1000;
	
    txMessage[8] = (timestamp >>> 24) & 0xFF;
    txMessage[9] = (timestamp >>> 16) & 0xFF;
    txMessage[10] = (timestamp >>> 8) & 0xFF;
    txMessage[11] = timestamp & 0xFF;

	txSender.send(txMessage, 0, txBytes, txPort, txAddress);
	
	if (txCount != 0) {
		if (--txCount == 0) {
			txStarted = false;
    		clearInterval(txTimer);
		    console.log("TX Stopped");
			return;
		}
	}
}

var txSender = dgram.createSocket("udp4");

var txServer = net.createServer(function (stream) {
  stream.setEncoding('utf8');
  
  stream.on('connect', function () {
    console.log('TX Connected');
  });
  
  stream.on('data', function (data) {
    var parts = data.substring(0, data.length - 1).split(' ');

    if (parts[0] == 'start') {
		if (parts.length != 7) {
		    txError(stream, "Invalid number of arguments for start command\n");
		    return;
		}
		
		var aparts = parts[1].split(':');
		
		if (aparts.length != 2) {
		    txError(stream, "Invalid endpoint specified\n");
		    return;
		}
		
		try {
			txPort = parseInt(aparts[1]);
	    }
	    catch(err) {
		    txError(stream, "Invalid txPort specified");
		    return;
	    }
	    
	    txAddress = aparts[0];

		try {
			id = parseInt(parts[2]);
	    }
	    catch(err) {
		    txError(stream, "Invalid id specified");
		    return;
	    }
	    
	    if (id == 0) {
		    txError(stream, "Id must be non-zero");
		    return;
	    }

		try {
			txCount = parseInt(parts[3]);
	    }
	    catch(err) {
		    txError(stream, "Invalid txCount specified");
		    return;
	    }

		try {
			txBytes = parseInt(parts[4]);
	    }
	    catch(err) {
		    txError(stream, "Invalid txBytes specified");
		    return;
	    }
	    
	    if (txBytes < 12) {
		    txError(stream, "Specified txBytes less than minimum of 12");
		    return;
	    }

	    if (txBytes > 65536) {
		    txError(stream, "Specified txBytes greater than maximum of 65536");
		    return;
	    }
	    
		try {
			txDelay = parseInt(parts[5]);
	    }
	    catch(err) {
		    txError(stream, "Invalid txDelay specified");
		    return;
	    }
	    
	    if (txDelay == 0) {
		    txError(stream, "Invalid txDelay of 0 specified");
		    return;
	    }
	    
		try {
			txTtl = parseInt(parts[6]);
	    }
	    catch(err) {
		    txError(stream, "Invalid txTtl specified");
		    return;
	    }
	    
	    stream.write("OK\n");
	    
	    txStarted = true;
	    
	    txFrame = 0;
	
		txMessage = new Buffer(txBytes);
		
		txMessage.fill(0);

	    txMessage[0] = (id >>> 24) & 0xFF;
	    txMessage[1] = (id >>> 16) & 0xFF;
	    txMessage[2] = (id >>> 8) & 0xFF;
	    txMessage[3] = id & 0xFF;

		txTimer = setInterval(txSend, txDelay);
		
		var num = txCount;
		
		if (num == 0) {
		  num = "infinite";
		}
		
	    console.log("TX sending " + num + " messages with an id of " + id + " and a length of " + txBytes + " bytes to " + txAddress + ":" + txPort + " with a delay of " + txDelay + "mS and ttl of " + txTtl);
    }
    else if (parts[0] == 'stop') {
    	if (txStarted == true) {
    		txStarted = false;
    		clearInterval(txTimer);
		    console.log("TX Stopped");
    	}
	    stream.write("OK\n");
    }
    else {
	    txError(stream, "Urecognised command");
    }
  });
  
  stream.on('end', function () {
    console.log('TX Disconnected');
    stream.end();
  });
  
});

txServer.maxConnections = 1;
rxServer.maxConnections = 1;
txServer.listen(txCommandPort, "0.0.0.0");
rxServer.listen(rxReportPort, "0.0.0.0");
rxReceiver.bind(rxReceivePort, "0.0.0.0");

console.log("TX commands on port : " + txCommandPort);
console.log("RX receiver on port : " + rxReceivePort);
console.log("RX reporter on port : " + rxReportPort);

