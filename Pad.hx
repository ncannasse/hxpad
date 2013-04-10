enum PadState {
	Waiting;
	Ready;
	Error( msg : String );
	NotSupported;
	NoPadFound;
	UnknownError( code : Int );
}

class Pad {

	var sock : flash.net.Socket;
	
	public var xAxis = 0.;
	public var yAxis = 0.;
	public var buttons : Array<Bool>;
	var frame : Int;
	var buttonsFrame : Array<Int>;
	
	public var active(default, set) : Bool;
	public var state : PadState;
	
	public function new() {
		state = Waiting;
		active = true;
		buttons = [];
		buttonsFrame = [];
	}
	
	public function update() {
		for( i in 0...buttonsFrame.length )
			if( buttonsFrame[i] == -1 ) {
				buttonsFrame[i] = 0;
				buttons[i] = false;
			}
	}
	
	public function isToggled(butId) {
		return buttons[butId] && buttonsFrame[butId] == frame;
	}
	
	
	public function init() {
		flash.Lib.current.stage.addEventListener(flash.events.Event.ENTER_FRAME, function(_) frame++);
		// only supported on Windows
		if( flash.system.Capabilities.os.toLowerCase().indexOf("windows") == -1 ) {
			state = NotSupported;
			return;
		}
		if( flash.system.Capabilities.playerType == "Desktop" )
			startProcess();
		else
			connect();
	}
	
	// AIR
	function startProcess() {
		var p = new flash.desktop.NativeProcess();
		var i = new flash.desktop.NativeProcessStartupInfo();
		i.arguments = flash.Vector.ofArray(["-noconsole"]);
		i.executable = flash.filesystem.File.applicationDirectory.resolvePath("pad.exe");
		i.workingDirectory = flash.filesystem.File.applicationDirectory;
		p.addEventListener("exit", function(e:Dynamic) {
			var code : Int = Reflect.field(e, "exitCode");
			switch( code ) {
			case 3:
				state = NoPadFound;
			case 0xC0000005:
				state = Error("Access Violation");
			default:
				state = UnknownError(code);
			}
		});
		p.addEventListener(flash.events.IOErrorEvent.IO_ERROR, function(e) {
			trace(e);
		});
		p.start(i);
		haxe.Timer.delay(connect, 1000);
	}
	
	function connect() {
		sock = new flash.net.Socket();
		sock.endian = flash.utils.Endian.LITTLE_ENDIAN;
		sock.addEventListener(flash.events.Event.CONNECT, onConnect);
		sock.addEventListener(flash.events.ProgressEvent.SOCKET_DATA, onData);
		sock.addEventListener(flash.events.IOErrorEvent.IO_ERROR, onError);
		sock.addEventListener(flash.events.SecurityErrorEvent.SECURITY_ERROR, onError);
		sock.connect("127.0.0.1", 8034);
	}
	
	function set_active(b) {
		if( !b ) {
			xAxis = yAxis = 0;
			buttons = [];
		}
		return active = b;
	}
	
	function onError(_) {
		switch( state ) {
		case Ready:
			state = Error("Disconnected");
		case Waiting:
			state = Error("Failed to connect");
		default:
			// keep previous error
		}
		buttons = [];
		xAxis = yAxis = 0;
	}
	
	function onConnect(_) {
		state = Ready;
	}
	
	function onData(e:flash.events.ProgressEvent) {
		while( true ) {
			if( sock.bytesAvailable < 3 )
				break;
			var code = sock.readByte();
			var data = sock.readShort();
			if( !active )
				continue;
			switch( code ) {
			case -1:
				xAxis = data / (1 << 15);
			case -2:
				yAxis = data / (1 << 15);
			default:
				if( data == 1 ) {
					buttons[code] = true;
					buttonsFrame[code] = frame;
				} else if( buttonsFrame[code] == frame ) {
					// delay release until we had a chance to actually catch the event
					buttonsFrame[code] = -1;
				} else {
					buttons[code] = false;
				}
			}
		}
	}
		
}