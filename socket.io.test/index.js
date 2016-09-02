var app = require('express')();
var http = require('http').Server(app);
var io = require('socket.io')(http);

app.get('/', function(req, res){
  res.sendFile(__dirname + '/index.html');
});

io.on('connection', function(socket){
  console.log('a user connected');

  socket.on('disconnect', function(){
    console.log('user disconnected');
  });

  socket.on('hello', function(msg){
    io.emit('hello', msg);
  });

  socket.on('message', function(msg){
    console.log(msg);
    io.emit('message', msg);
  });
});

http.listen(3030, function(){
  console.log('listening on *:3030');
});
