var top_ws;
var blobby;
var top_data;

var count = 0;
var id_base = Math.floor( Math.random() * 0xFFFFFFFF );

var send_request = function( ws, obj )
{
  var message = JSON.stringify( obj );
  var num_bytes = message.length + 4 + 1;
  var byte_array = new Uint8Array( num_bytes );

  // Set up sequence number
  var id = id_base; 
  id += count;
  count += 1;
  id = ((id & 0xFFFFFFFF) | 0x80000000 );
  console.log(id);
  for( var i=3; i >= 0; i-- )
  {
    var next_val = (id >> 8) & 0xFF;
    byte_array[i] = next_val;
    id = id >> 8;
    console.log( next_val );
  }

  // Copy message text
  for( var i=0; i < message.length; i++ )
  {
    byte_array[i+4] = message.charCodeAt(i);
  }

  // C-string
  byte_array[ num_bytes-1 ] = 0; 

  var blob = new Blob([byte_array], {type: "application/octet-stream"});

  ws.send(blob);
  console.log("sent message");
}

$(document).ready( function() {

  var button = $("#input_button");
  var result = $("#result");

  var ws = new WebSocket('ws://127.0.0.1:8081', 
                         ['rep.sp.nanomsg.org']
  );

  top_ws = ws;

  ws.onmessage = function( e ) {
    blobby = e.data;
    var reader = new FileReader();
    reader.addEventListener('loadend', function() {
      var text = reader.result;
      var data = JSON.parse( text );
      top_data = data;
      if( data.response == "all_users" )
      {
        $('#users_table').find("tr:gt(0)").remove();
        $(function() {
          $.each( data.users, function( i, user ) {
            $('<tr>').append(
                $('<td>').text( user.name ),
                $('<td>').text( user.weight ),
                $('<td>').text( user.age ),
                $('<td>').text( user.id )
            ).appendTo('#users_table');
          });
        });
      }
      result.text( text );
    });
    var text = e.data.slice( 4, e.data.size-1 );
    reader.readAsText( text );
    console.log("got message");
  }

  ws.onclose = function( e ) { result.text( "WS closed" ); }
  ws.onerror = function( e ) { result.text( "WS error" ); }
  ws.onopen = function( e ) { result.text( "WS opened" ); }
  
  button.click( function() {
    var data = { 
      'request' : 'get_users'
    };
    send_request( ws, data );
    result.text("Sent message");
  });

  $('#result').css({
                   color: 'red',
                   border: '1px solid blue'
  });
});

