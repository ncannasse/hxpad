#pragma warning(disable:4996)
#undef UNICODE
#include <Windows.h>
#include <stdio.h>
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <math.h>

#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "wsock32.lib")

IDirectInput8 *input;
IDirectInputDevice8 *pad = NULL;
char *padName;

BOOL DeviceEnum( LPCDIDEVICEINSTANCE inf, LPVOID pvRef ) {
	printf("Found %s\n",inf->tszInstanceName);
	if( input->CreateDevice(inf->guidProduct,&pad,NULL) != DI_OK ) {
		printf("CreateDevice Failure\n");
		return DIENUM_CONTINUE;
	}
	padName = strdup(inf->tszInstanceName);
	return DIENUM_STOP;
}

static int Run( HINSTANCE hInstance) {

	if( DirectInput8Create(hInstance,DIRECTINPUT_VERSION,IID_IDirectInput8,(LPVOID*)&input,NULL) != DI_OK ) {
		printf("DI8Create Failure\n");
		return 1;
	}

	if( input->EnumDevices(DI8DEVCLASS_GAMECTRL,(LPDIENUMDEVICESCALLBACKA)DeviceEnum,NULL,DIEDFL_ATTACHEDONLY) != DI_OK ) {
		printf("EnumDevices Failure\n");
		return 2;
	}

	if( pad == NULL ) {
		printf("No pad found\n");
		return 3;
	}

	if( pad->SetCooperativeLevel(NULL,DISCL_BACKGROUND | DISCL_NONEXCLUSIVE) != DI_OK )
		printf("SetCoop Failure\n");

	if( pad->SetDataFormat(&c_dfDIJoystick2) != DI_OK ) {
		printf("SetDataFormat Failure\n");
		return 4;
	}

	DIPROPDWORD ph;
	ZeroMemory(&ph,sizeof(ph));
	ph.diph.dwHeaderSize = sizeof(DIPROPHEADER);
	ph.diph.dwSize = sizeof(ph);
	ph.diph.dwHow = DIPH_DEVICE;
	ph.dwData = 1024;
	if( pad->SetProperty(DIPROP_BUFFERSIZE,(DIPROPHEADER*)&ph) != DI_OK ) {
		printf("SetBufferSize Failure\n");
		return 5;
	}

	if( pad->Acquire() != DI_OK )
		printf("Acquire Failure\n");


	SOCKET sock = socket(AF_INET,SOCK_STREAM,0);
	struct sockaddr_in addr;
	DWORD host = 0x0100007F;
	int port = 8034;
	memset(&addr,0,sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	*(int*)&addr.sin_addr.s_addr = host;
	printf("Listening on %s:%d\n",inet_ntoa(addr.sin_addr),port);
	if( bind(sock,(struct sockaddr*)&addr,sizeof(addr)) == SOCKET_ERROR ) {
		printf("Bind Failure\n");
		return 6;
	}
	listen(sock,10);

	int addrlen = sizeof(addr);
	SOCKET client = accept(sock,(struct sockaddr*)&addr,&addrlen);
	if( client == INVALID_SOCKET ) {
		printf("Accept Failure\n");
		return 7;
	}

#pragma pack(1)
	struct {
		unsigned char code;
		short data;
	} packet;
#pragma pack()
	printf("Packet size = %d\n",sizeof(packet));

	int lostCount = 0, xVal = 0, yVal = 0;
	while( 1 ) {
		HRESULT r = pad->Poll();
		if( r != DI_OK && r != DI_NOEFFECT )
			break;
		int sleepTime = 10;
		DWORD count = 5;
		DIDEVICEOBJECTDATA data[5];
		ZeroMemory(data,sizeof(data));
		r = pad->GetDeviceData(sizeof(DIDEVICEOBJECTDATA),data,&count,0);
		switch( r ) {
		case DI_OK:
			break;
		case DI_BUFFEROVERFLOW:
			sleepTime = 0;
			break;
		case DIERR_NOTACQUIRED:
			if( lostCount++ < 1000 ) {
				pad->Acquire();
				count = 0;
				break;
			}
			// fallthrough
		default:
			printf("Error %.8X\n", r);
			return r;
		}
		DWORD i;
		int xychanged = 0;

		int MED = (1<<15);
		for(i=0;i<count;i++) {
			DWORD ev = data[i].dwOfs;
			switch( ev ) {
			case DIJOFS_X:
				xychanged = 1;
				xVal = data[i].dwData - MED;
				break;
			case DIJOFS_Y:
				xychanged = 1;
				yVal = data[i].dwData - (1<<15);
				break;
			case DIJOFS_POV(0):
				xychanged = 1;
				if( data[i].dwData == 0xFFFFFFFF ) {
					xVal = yVal = 0;
				} else {
					double angle = ((data[i].dwData / 100.0) - 90) * 3.1415926535897932384626433832795 / 180.0;
					xVal = (int)floor(cos(angle) * MED);
					yVal = (int)floor(sin(angle) * MED);
				}
				break;
			default:
				if( ev >= DIJOFS_BUTTON0 && ev <= DIJOFS_BUTTON31 ) {
					printf("[B%d:%d]\n",ev-DIJOFS_BUTTON0,data[i].dwData);
					packet.code = (unsigned char)(ev-DIJOFS_BUTTON0);
					packet.data = (data[i].dwData == 0) ? 0 : 1;
					if( send(client,(char*)&packet,sizeof(packet),0) == SOCKET_ERROR ) {
						printf("Fail to send data\n");
						return -10;
					}
				} else {
					printf("Unknown event %d[%.8X] %d\n",ev,data[i].dwData);
				}
				break;
			}
		}

		if( xychanged ) {
			if( xVal < -MED )
				xVal = -MED;
			else if( xVal >= MED )
				xVal = MED-1;
			if( yVal < -MED )
				yVal = -MED;
			else if( yVal >= MED )
				yVal = MED-1;

			printf("[X:%d,Y:%d]\n",xVal,yVal);
			packet.code = -1;
			packet.data = xVal;
			if( send(client,(char*)&packet,sizeof(packet),0) == SOCKET_ERROR ) {
				printf("Fail to send data\n");
				return -10;
			}
			packet.code = -2;
			packet.data = yVal;
			if( send(client,(char*)&packet,sizeof(packet),0) == SOCKET_ERROR ) {
				printf("Fail to send data\n");
				return -10;
			}
		}

		if( sleepTime > 0 ) {
			fd_set tmp;
			struct timeval time;
			time.tv_sec = 0;
			time.tv_usec = sleepTime * 1000;
			FD_ZERO(&tmp);
			FD_SET(client,&tmp);
			if( select(1,&tmp,NULL,NULL,&time) == SOCKET_ERROR ) {
				printf("Fail to select\n");
				return -11;
			}
			if( FD_ISSET(client,&tmp) ) {
				char code;
				if( recv(client,&code,1,0) != 1 ) {
					printf("Read socket failure\n");
					return -10;
				}
			}
		}
		
	}

	return 0;
}


int CALLBACK WinMain(
  _In_  HINSTANCE hInstance,
  _In_  HINSTANCE hPrevInstance,
  _In_  LPSTR lpCmdLine,
  _In_  int nCmdShow
) {
	WSADATA sdata;
	int hasConsole = 0;
	WSAStartup(MAKEWORD(2,0),&sdata);

	if( strstr(lpCmdLine,"-noconsole") == NULL ) {
		hasConsole = 1;
		AllocConsole();
		freopen( "CONOUT$", "wb", stdout);
	}

	int ret = Run(hInstance);
	if( pad != NULL ) pad->Release();
	if( input != NULL ) input->Release();
	if( ret != -10 && hasConsole )
		Sleep(5000);

	return ret;
}

