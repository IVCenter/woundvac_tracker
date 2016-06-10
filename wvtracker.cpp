/*
 *  modified version of simpleTest.c from the ARToolkit examples
 *
 *  Performs AR based tracking for the woundvac project
 *
 *  Press '?' while running for help on available key commands.
 *
 *  Disclaimer: IMPORTANT:  This Daqri software is supplied to you by Daqri
 *  LLC ("Daqri") in consideration of your agreement to the following
 *  terms, and your use, installation, modification or redistribution of
 *  this Daqri software constitutes acceptance of these terms.  If you do
 *  not agree with these terms, please do not use, install, modify or
 *  redistribute this Daqri software.
 *
 *  In consideration of your agreement to abide by the following terms, and
 *  subject to these terms, Daqri grants you a personal, non-exclusive
 *  license, under Daqri's copyrights in this original Daqri software (the
 *  "Daqri Software"), to use, reproduce, modify and redistribute the Daqri
 *  Software, with or without modifications, in source and/or binary forms;
 *  provided that if you redistribute the Daqri Software in its entirety and
 *  without modifications, you must retain this notice and the following
 *  text and disclaimers in all such redistributions of the Daqri Software.
 *  Neither the name, trademarks, service marks or logos of Daqri LLC may
 *  be used to endorse or promote products derived from the Daqri Software
 *  without specific prior written permission from Daqri.  Except as
 *  expressly stated in this notice, no other rights or licenses, express or
 *  implied, are granted by Daqri herein, including but not limited to any
 *  patent rights that may be infringed by your derivative works or by other
 *  works in which the Daqri Software may be incorporated.
 *
 *  The Daqri Software is provided by Daqri on an "AS IS" basis.  DAQRI
 *  MAKES NO WARRANTIES, EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION
 *  THE IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE, REGARDING THE DAQRI SOFTWARE OR ITS USE AND
 *  OPERATION ALONE OR IN COMBINATION WITH YOUR PRODUCTS.
 *
 *  IN NO EVENT SHALL DAQRI BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL
 *  OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) ARISING IN ANY WAY OUT OF THE USE, REPRODUCTION,
 *  MODIFICATION AND/OR DISTRIBUTION OF THE DAQRI SOFTWARE, HOWEVER CAUSED
 *  AND WHETHER UNDER THEORY OF CONTRACT, TORT (INCLUDING NEGLIGENCE),
 *  STRICT LIABILITY OR OTHERWISE, EVEN IF DAQRI HAS BEEN ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *
 *  Copyright 2015 Daqri LLC. All Rights Reserved.
 *  Copyright 2002-2015 ARToolworks, Inc. All Rights Reserved.
 *
 *  simpletest.c Author(s): Hirokazu Kato, Philip Lamb.
 *
 */

#ifdef _WIN32
#  include <windows.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef __APPLE__
#  include <GL/gl.h>
#  include <GL/glut.h>
#else
#  include <OpenGL/gl.h>
#  include <GLUT/glut.h>
#endif
#include <AR/ar.h>
#include <AR/gsub.h>
#include <AR/video.h>

#include <map>
#include <vector>
#include <string>
#include <iostream>
#include <sstream>

#include <pthread.h>

#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#include <jsoncpp/json/value.h>
#include <jsoncpp/json/writer.h>

#define             PATT_NAME        "Data/hiro.patt"

#define NET_PORT 8877

char * paramFile = NULL;
char * settingsFile = NULL;

ARHandle           *arHandle;
ARPattHandle       *arPattHandle;
AR3DHandle         *ar3DHandle;
ARGViewportHandle  *vp;
int                 xsize, ysize;
int                 flipMode = 0;
int                 patt_id;
// change this if the marker size is changed
double              patt_width = 0.1016; // meters (4 inch)
int                 count = 0;
char                fps[256];
char                errValue[256];
int                 distF = 0;
int                 contF = 0;
ARParamLT          *gCparamLT = NULL;

int originId = 0;
int totalMarkers = 4;
struct MarkerState
{
    bool valid;
    int id;
    double q[4];
    double pos[3];
};
std::map<int,ARdouble*> g_poseMap;
std::vector<MarkerState*> g_stateList;

pthread_mutex_t stateLock = PTHREAD_MUTEX_INITIALIZER;
pthread_t thread;

static void   init(int argc, char *argv[]);
static void   keyFunc( unsigned char key, int x, int y );
static void   cleanup(void);
static void   mainLoop(void);
static void   draw( ARdouble trans[3][4] );


void * netThread(void * param)
{
    std::cerr << "Starting net thread" << std::endl;

    int listensd, clientsd = -1;
    struct sockaddr_in serv_addr, cli_addr;
    socklen_t clilen;

    listensd = socket(AF_INET, SOCK_STREAM, 0);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(NET_PORT);

    int enable = 1;
    setsockopt(listensd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
    if(bind(listensd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
    {
	std::cerr << "Error binding socket" << std::endl;
	return NULL;
    }

    listen(listensd,5);
    
    clilen = sizeof(cli_addr);

    std::cerr << "Waiting for client" << std::endl;
    while((clientsd = accept(listensd, 
                 (struct sockaddr *) &cli_addr, 
                 &clilen)) >= 0)
    {
	std::cerr << "Got client connection" << std::endl;

	int flag = 1;
        int result = setsockopt(clientsd,IPPROTO_TCP,TCP_NODELAY,(char *) &flag,sizeof(int));

	char c;
	while(recv(clientsd,&c,sizeof(char),0) > 0)
	{
	    pthread_mutex_lock(&stateLock);
	    // create json message
	    Json::Value root;
	    
	    for(int i = 0; i < totalMarkers; ++i)
	    {
		std::stringstream ss;
		ss << "m" << i;
		std::string base = ss.str();

		std::string key = base + "v";

		if(i == originId || !g_stateList[i]->valid)
		{
		    root[key.c_str()] = false;
		    continue;
		}

		root[key.c_str()] = true;

		// swap y and z to get to unity coord system
		key = base + "px";
		root[key.c_str()] = g_stateList[i]->pos[0];
		key = base + "py";
		root[key.c_str()] = g_stateList[i]->pos[2];
		key = base + "pz";
		root[key.c_str()] = g_stateList[i]->pos[1];

		// swap y and z for same reason
		key = base + "qx";
		root[key.c_str()] = g_stateList[i]->q[0];
		key = base + "qy";
		root[key.c_str()] = g_stateList[i]->q[2];
		key = base + "qz";
		root[key.c_str()] = g_stateList[i]->q[1];
		key = base + "qw";
		root[key.c_str()] = g_stateList[i]->q[3];
	    }

	    pthread_mutex_unlock(&stateLock);

	    Json::FastWriter fastWriter;
	    std::string stateStr = fastWriter.write(root);

	    //std::cerr << "State Json: " << stateStr << std::endl;

	    int size = stateStr.length();
	    
	    send(clientsd,&size,sizeof(int),0);
	    send(clientsd,stateStr.c_str(),size,0);
	}
	close(clientsd);

	std::cerr << "Waiting for next client" << std::endl;
    }

    return NULL;
}


int main(int argc, char *argv[])
{
    if(argc < 2)
    {
	std::cerr << "Usage: " << argv[0] << " camParamFile [camSettingsFile]" << std::endl;
	return 1;
    }

	glutInit(&argc, argv);
    init(argc, argv);

    argSetDispFunc( mainLoop, 1 );
	argSetKeyFunc( keyFunc );
	count = 0;
    fps[0] = '\0';
	arUtilTimerReset();
    argMainLoop();
	return (0);
}
static void keyFunc( unsigned char key, int x, int y )
{
    int   value;

    switch (key) {
		case 0x1b:
			cleanup();
			exit(0);
			break;
		case '1':
		case '-':
        	arGetLabelingThresh( arHandle, &value );
        	value -= 5;
        	if( value < 0 ) value = 0;
        	arSetLabelingThresh( arHandle, value );
        	ARLOG("thresh = %d\n", value);
        	break;
		case '2':
		case '+':
        	arGetLabelingThresh( arHandle, &value );
       		value += 5;
        	if( value > 255 ) value = 255;
        	arSetLabelingThresh( arHandle, value );
        	ARLOG("thresh = %d\n", value);
        	break;
		case 'd':
		case 'D':
        	arGetDebugMode( arHandle, &value );
       		value = 1 - value;
        	arSetDebugMode( arHandle, value );
            break;
        case 'h':
        case 'H':
            if( flipMode & AR_GL_FLIP_H ) flipMode = flipMode & AR_GL_FLIP_V;
            else                         flipMode = flipMode | AR_GL_FLIP_H;
            argViewportSetFlipMode( vp, flipMode );
            break;
        case 'v':
        case 'V':
            if( flipMode & AR_GL_FLIP_V ) flipMode = flipMode & AR_GL_FLIP_H;
            else                         flipMode = flipMode | AR_GL_FLIP_V;
            argViewportSetFlipMode( vp, flipMode );
        	break;
        case ' ':
            distF = 1 - distF;
            if( distF ) {
                argViewportSetDistortionMode( vp, AR_GL_DISTORTION_COMPENSATE_ENABLE );
            } else {
                argViewportSetDistortionMode( vp, AR_GL_DISTORTION_COMPENSATE_DISABLE );
            }
            break;
        case 'c':
            contF = 1 - contF;
            break;
		case '?':
		case '/':
			ARLOG("Keys:\n");
			ARLOG(" [esc]         Quit demo.\n");
			ARLOG(" - and +       Adjust threshhold.\n");
			ARLOG(" d             Activate / deactivate debug mode.\n");
			ARLOG(" h and v       Toggle horizontal / vertical flip mode.\n");
            ARLOG(" [space]       Toggle distortion compensation.\n");
			ARLOG(" ? or /        Show this help.\n");
			ARLOG("\nAdditionally, the ARVideo library supplied the following help text:\n");
			arVideoDispOption();
			break;
		default:
			break;
	}
}



static void mainLoop(void)
{
    static ARUint8 *dataPtr = NULL;
    ARMarkerInfo   *markerInfo;
    int             markerNum;
    ARdouble        err;
    int             imageProcMode;
    int             debugMode;

    /* grab a video frame */
    if( (dataPtr = (ARUint8 *)arVideoGetImage()) == NULL ) {
        arUtilSleep(2);
        return;
    }

    argDrawMode2D(vp);
    arGetDebugMode( arHandle, &debugMode );
    if( debugMode == 0 ) {
        argDrawImage( dataPtr );
    }
    else {
        arGetImageProcMode(arHandle, &imageProcMode);
        if( imageProcMode == AR_IMAGE_PROC_FRAME_IMAGE ) {
            argDrawImage( arHandle->labelInfo.bwImage );
        }
        else {
            argDrawImageHalf( arHandle->labelInfo.bwImage );
        }
    }

    /* detect the markers in the video frame */
    if( arDetectMarker(arHandle, dataPtr) < 0 ) {
        cleanup();
        exit(0);
    }

    if( count % 60 == 0 ) {
        sprintf(fps, "%f[fps]", 60.0/arUtilTimer());
        arUtilTimerReset();
    }
    count++;
    glColor3f(0.0f, 1.0f, 0.0f);
    argDrawStringsByIdealPos(fps, 10, ysize-30);

    markerNum = arGetMarkerNum( arHandle );
    if( markerNum == 0 ) {
        argSwapBuffers();
        return;
    }

    /* check for object visibility */
    markerInfo =  arGetMarker( arHandle ); 
    for( int j = 0; j < markerNum; j++ ) {
	if(markerInfo[j].id < 0)
	{
	    continue;
	}
        //ARLOG("ID=%d, CF = %f\n", markerInfo[j].id, markerInfo[j].cf);
	if(g_poseMap.find(markerInfo[j].id) == g_poseMap.end())
	{
	    g_poseMap[markerInfo[j].id] = new ARdouble[3*4];
	    err = arGetTransMatSquare(ar3DHandle, &(markerInfo[j]), patt_width, (ARdouble (*)[4])g_poseMap[markerInfo[j].id]);
        }
	else
	{
	    // using the previous pose was causing some markers to lock to others, may be useful to look at later
	    //err = arGetTransMatSquareCont(ar3DHandle, &(markerInfo[j]), (ARdouble (*)[4])g_poseMap[markerInfo[j].id], patt_width, (ARdouble (*)[4])g_poseMap[markerInfo[j].id]);
	    err = arGetTransMatSquare(ar3DHandle, &(markerInfo[j]), patt_width, (ARdouble (*)[4])g_poseMap[markerInfo[j].id]);
	}
    }

    if(g_poseMap.find(originId) != g_poseMap.end())
    {
	pthread_mutex_lock(&stateLock);
	double oinv[3][4];
	arUtilMatInv((ARdouble (*)[4])g_poseMap[originId], oinv);
	
	for(int i = 0; i < totalMarkers; ++i)
	{
	    if(i == originId)
	    {
	    	continue;
	    }

	    double rel[3][4];

	    if(g_poseMap.find(i) != g_poseMap.end())
	    {
		arUtilMatMul(oinv, (ARdouble (*)[4])g_poseMap[i], rel);

		arUtilMat2QuatPos(rel,g_stateList[i]->q,g_stateList[i]->pos);
		g_stateList[i]->valid = true;
	    }

	    //std::cerr << "state: " << i << std::endl;
	    //std::cerr << " pos: " << g_stateList[i]->pos[0] << " " << g_stateList[i]->pos[1] << " " << g_stateList[i]->pos[2] << std::endl;
	}
	pthread_mutex_unlock(&stateLock);
    }

    sprintf(errValue, "err = %f", err);
    glColor3f(0.0f, 1.0f, 0.0f);
    argDrawStringsByIdealPos(fps, 10, ysize-30);
    argDrawStringsByIdealPos(errValue, 10, ysize-60);
    //ARLOG("err = %f\n", err);

    //draw(patt_trans);

    argSwapBuffers();
}

static void   init(int argc, char *argv[])
{
    ARParam         cparam;
    ARGViewport     viewport;
    //char            vconf[512];
    AR_PIXEL_FORMAT pixFormat;
    ARUint32        id0, id1;
    int             i;

    paramFile = argv[1];

    if(argc > 2)
    {
	settingsFile = argv[2];
    }

    //if( argc == 1 ) vconf[0] = '\0';
    //else {
    //    strcpy( vconf, argv[1] );
    //    for( i = 2; i < argc; i++ ) {strcat(vconf, " "); strcat(vconf,argv[i]);}
    //}

    // hard coded video camera config
    char * vconf = (char*) "-width=1280 -height=720";

    /* open the video path */
	ARLOGi("Using video configuration '%s'.\n", vconf);
    if( arVideoOpen( vconf ) < 0 ) exit(0);
    if( arVideoGetSize(&xsize, &ysize) < 0 ) exit(0);
    ARLOGi("Image size (x,y) = (%d,%d)\n", xsize, ysize);
    if( (pixFormat=arVideoGetPixelFormat()) < 0 ) exit(0);
    if( arVideoGetId( &id0, &id1 ) == 0 && settingsFile != NULL) {
        ARLOGi("Camera ID = (%08x, %08x)\n", id1, id0);
        sprintf(vconf, settingsFile, id1, id0);
        if( arVideoLoadParam(vconf) < 0 ) {
            ARLOGe("No camera setting data!!\n");
        }
    }

    /* set the initial camera parameters */
    if( arParamLoad(paramFile, 1, &cparam) < 0 ) {
        ARLOGe("Camera parameter load error !!\n");
        exit(0);
    }
    arParamChangeSize( &cparam, xsize, ysize, &cparam );
    ARLOG("*** Camera Parameter ***\n");
    arParamDisp( &cparam );
    if ((gCparamLT = arParamLTCreate(&cparam, AR_PARAM_LT_DEFAULT_OFFSET)) == NULL) {
        ARLOGe("Error: arParamLTCreate.\n");
        exit(-1);
    }
    
    if( (arHandle=arCreateHandle(gCparamLT)) == NULL ) {
        ARLOGe("Error: arCreateHandle.\n");
        exit(0);
    }
    if( arSetPixelFormat(arHandle, pixFormat) < 0 ) {
        ARLOGe("Error: arSetPixelFormat.\n");
        exit(0);
    }

    if( (ar3DHandle=ar3DCreateHandle(&cparam)) == NULL ) {
        ARLOGe("Error: ar3DCreateHandle.\n");
        exit(0);
    }

    /*if( (arPattHandle=arPattCreateHandle()) == NULL ) {
        ARLOGe("Error: arPattCreateHandle.\n");
        exit(0);
    }
    if( (patt_id=arPattLoad(arPattHandle, PATT_NAME)) < 0 ) {
        ARLOGe("pattern load error !!\n");
        exit(0);
    }
    arPattAttach( arHandle, arPattHandle );*/

    arSetPatternDetectionMode(arHandle, AR_MATRIX_CODE_DETECTION);
    arSetMatrixCodeType(arHandle, AR_MATRIX_CODE_3x3_HAMMING63);

    /* open the graphics window */
/*
    int winSizeX, winSizeY;
    argCreateFullWindow();
    argGetScreenSize( &winSizeX, &winSizeY );
    viewport.sx = 0;
    viewport.sy = 0;
    viewport.xsize = winSizeX;
    viewport.ysize = winSizeY;
*/
    viewport.sx = 0;
    viewport.sy = 0;
    viewport.xsize = xsize;
    viewport.ysize = ysize;
    if( (vp=argCreateViewport(&viewport)) == NULL ) exit(0);
    argViewportSetCparam( vp, &cparam );
    argViewportSetPixFormat( vp, pixFormat );
    //argViewportSetDispMethod( vp, AR_GL_DISP_METHOD_GL_DRAW_PIXELS );
    argViewportSetDistortionMode( vp, AR_GL_DISTORTION_COMPENSATE_DISABLE );

	if (arVideoCapStart() != 0) {
        ARLOGe("video capture start error !!\n");
        exit(0);
	}

    for(int i = 0; i < totalMarkers; ++i)
    {
	MarkerState * ms = new MarkerState();
	ms->valid = false;
	ms->id = i;
	g_stateList.push_back(ms);
    }

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    pthread_create(&thread, &attr, netThread, (void *)NULL);
    pthread_attr_destroy(&attr);

}

/* cleanup function called when program exits */
static void cleanup(void)
{
    arVideoCapStop();
    argCleanup();
	arPattDetach(arHandle);
	arPattDeleteHandle(arPattHandle);
	ar3DDeleteHandle(&ar3DHandle);
	arDeleteHandle(arHandle);
    arParamLTFree(&gCparamLT);
    arVideoClose();
    
}

static void draw( ARdouble trans[3][4] )
{
    ARdouble  gl_para[16];
    GLfloat   mat_diffuse[]     = {0.0f, 0.0f, 1.0f, 0.0f};
    GLfloat   mat_flash[]       = {1.0f, 1.0f, 1.0f, 0.0f};
    GLfloat   mat_flash_shiny[] = {50.0f};
    GLfloat   light_position[]  = {100.0f,-200.0f,200.0f,0.0f};
    GLfloat   light_ambi[]      = {0.1f, 0.1f, 0.1f, 0.0f};
    GLfloat   light_color[]     = {1.0f, 1.0f, 1.0f, 0.0f};
    
    argDrawMode3D(vp);
    glClearDepth( 1.0 );
    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    
    /* load the camera transformation matrix */
    argConvGlpara(trans, gl_para);
    glMatrixMode(GL_MODELVIEW);
#ifdef ARDOUBLE_IS_FLOAT
    glLoadMatrixf( gl_para );
#else
    glLoadMatrixd( gl_para );
#endif

    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER, 1);
    glLightfv(GL_LIGHT0, GL_POSITION, light_position);
    glLightfv(GL_LIGHT0, GL_AMBIENT,  light_ambi);
    glLightfv(GL_LIGHT0, GL_DIFFUSE,  light_color);
    glLightfv(GL_LIGHT0, GL_SPECULAR, light_color);
    glMaterialfv(GL_FRONT, GL_SPECULAR,  mat_flash);
    glMaterialfv(GL_FRONT, GL_SHININESS, mat_flash_shiny);	
    glMaterialfv(GL_FRONT, GL_DIFFUSE,   mat_diffuse);
    glMaterialfv(GL_FRONT, GL_AMBIENT,   mat_diffuse);

#if 1
    glTranslatef( 0.0f, 0.0f, 40.0f );
    glutSolidCube(80.0);
#else
    glTranslatef( 0.0f, 0.0f, 20.0f );
    glRotatef(90.0f, 1.0f, 0.0f, 0.0f);
    glutSolidTeapot(40.0);
#endif
    glDisable(GL_LIGHT0);
    glDisable( GL_LIGHTING );

    glDisable( GL_DEPTH_TEST );
}
