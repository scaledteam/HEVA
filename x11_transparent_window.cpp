//
// Created by scaled
//

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include <GL/gl.h>
#include <GL/glx.h>
#include <GL/glxext.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xrender.h>
#include <X11/Xutil.h>

#include "heva.h"

// inih
#ifdef ALL_IN_ONE
#include "./inih-r53/cpp/INIReader.h"
#else
#include "INIReader.h"
#endif

#ifdef X11_TRANSPARENT_WINDOW
Heva* myApp;

#define USE_CHOOSE_FBCONFIG

//#define USE_GLX_CREATE_WINDOW

static void fatalError(const char *why)
{
	fprintf(stderr, "%s", why);
	exit(0x666);
}

static int Xscreen;
static Atom del_atom;
static Colormap cmap;
static Display *Xdisplay;
static XVisualInfo *visual;
static XRenderPictFormat *pict_format;
static GLXFBConfig *fbconfigs, fbconfig;
static int numfbconfigs;
static GLXContext render_context;
static Window Xroot, window_handle;
static GLXWindow glX_window_handle;
static int width, height;

static int VisData[] = {
GLX_RENDER_TYPE, GLX_RGBA_BIT,
GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
GLX_DOUBLEBUFFER, True,
GLX_RED_SIZE, 8,
GLX_GREEN_SIZE, 8,
GLX_BLUE_SIZE, 8,
GLX_ALPHA_SIZE, 8,
GLX_DEPTH_SIZE, 16,
None
};

static int isExtensionSupported(const char *extList, const char *extension)
{
	const char *start;
	const char *where, *terminator;

	/* Extension names should not have spaces. */
	where = strchr(extension, ' ');
	if ( where || *extension == '\0' )
	return 0;

	/* It takes a bit of care to be fool-proof about parsing the
	OpenGL extensions string. Don't be fooled by sub-strings,
	etc. */
	for ( start = extList; ; ) {
	where = strstr( start, extension );

	if ( !where )
	break;

	terminator = where + strlen( extension );

	if ( where == start || *(where - 1) == ' ' )
	if ( *terminator == ' ' || *terminator == '\0' )
	return 1;

	start = terminator;
	}
	return 0;
}

static Bool WaitForMapNotify(Display *d, XEvent *e, char *arg)
{
	return d && e && arg && (e->type == MapNotify) && (e->xmap.window == *(Window*)arg);
}

static void describe_fbconfig(GLXFBConfig fbconfig)
	{
	int doublebuffer;
	int red_bits, green_bits, blue_bits, alpha_bits, depth_bits;

	glXGetFBConfigAttrib(Xdisplay, fbconfig, GLX_DOUBLEBUFFER, &doublebuffer);
	glXGetFBConfigAttrib(Xdisplay, fbconfig, GLX_RED_SIZE, &red_bits);
	glXGetFBConfigAttrib(Xdisplay, fbconfig, GLX_GREEN_SIZE, &green_bits);
	glXGetFBConfigAttrib(Xdisplay, fbconfig, GLX_BLUE_SIZE, &blue_bits);
	glXGetFBConfigAttrib(Xdisplay, fbconfig, GLX_ALPHA_SIZE, &alpha_bits);
	glXGetFBConfigAttrib(Xdisplay, fbconfig, GLX_DEPTH_SIZE, &depth_bits);

	fprintf(stderr, "FBConfig selected:\n"
	"Doublebuffer: %s\n"
	"Red Bits: %d, Green Bits: %d, Blue Bits: %d, Alpha Bits: %d, Depth Bits: %d\n",
	doublebuffer == True ? "Yes" : "No",
	red_bits, green_bits, blue_bits, alpha_bits, depth_bits);
}

static void createTheWindow()
{
	XEvent event;
	int x,y, attr_mask;
	XSizeHints hints;
	XWMHints *startup_state;
	XTextProperty textprop;
	XSetWindowAttributes attr = {0,};
	static char title[] = "HEVA - High Efficiency VTuber Avatar";

	Xdisplay = XOpenDisplay(NULL);
	if (!Xdisplay) {
		fatalError("Couldn't connect to X server\n");
	}
	Xscreen = DefaultScreen(Xdisplay);
	Xroot = RootWindow(Xdisplay, Xscreen);

	fbconfigs = glXChooseFBConfig(Xdisplay, Xscreen, VisData, &numfbconfigs);
	fbconfig = 0;
	for(int i = 0; i<numfbconfigs; i++) {
		visual = (XVisualInfo*) glXGetVisualFromFBConfig(Xdisplay, fbconfigs[i]);
		if(!visual)
			continue;

		pict_format = XRenderFindVisualFormat(Xdisplay, visual->visual);
		if(!pict_format)
			continue;

		fbconfig = fbconfigs[i];
		if(pict_format->direct.alphaMask > 0) {
			break;
		}
	}

	if(!fbconfig) {
		fatalError("No matching FB config found");
	}

	describe_fbconfig(fbconfig);

	/* Create a colormap - only needed on some X clients, eg. IRIX */
	cmap = XCreateColormap(Xdisplay, Xroot, visual->visual, AllocNone);

	attr.colormap = cmap;
	attr.background_pixmap = None;
	attr.border_pixmap = None;
	attr.border_pixel = 0;
	attr.event_mask =
		StructureNotifyMask |
		EnterWindowMask |
		LeaveWindowMask |
		ExposureMask |
		ButtonPressMask |
		ButtonReleaseMask |
		OwnerGrabButtonMask |
		KeyPressMask |
		KeyReleaseMask;
	
	//attr.event_mask |= ResizeRedirectMask;
	//attr.event_mask &= !ResizeRequest;

	attr_mask =
		CWBackPixmap|
		CWColormap|
		CWBorderPixel|
		CWEventMask;
		
	//attr.override_redirect = 1;
	//attr_mask |= CWOverrideRedirect;

	/*width = DisplayWidth(Xdisplay, DefaultScreen(Xdisplay))/2;
	height = DisplayHeight(Xdisplay, DefaultScreen(Xdisplay))/2;
	x=width/2, y=height/2;*/

	//width = 550;
	//height = 800;
	x=0; y=0;

	window_handle = XCreateWindow(
		Xdisplay,
		Xroot,
		x, y, width, height,
		0,
		visual->depth,
		InputOutput,
		visual->visual,
		attr_mask, &attr
	);

	if( !window_handle ) {
		fatalError("Couldn't create the window\n");
	}

	#if USE_GLX_CREATE_WINDOW
	int glXattr[] = { None };
	glX_window_handle = glXCreateWindow(Xdisplay, fbconfig, window_handle, glXattr);
	if( !glX_window_handle ) {
		fatalError("Couldn't create the GLX window\n");
	}
	#else
	glX_window_handle = window_handle;
	#endif

	textprop.value = (unsigned char*)title;
	textprop.encoding = XA_STRING;
	textprop.format = 8;
	textprop.nitems = strlen(title);

	hints.x = x;
	hints.y = y;
	hints.width = width;
	hints.height = height;
	hints.flags = USPosition|USSize;

	startup_state = XAllocWMHints();
	startup_state->initial_state = NormalState;
	startup_state->flags = StateHint;

	XSetWMProperties(Xdisplay, window_handle,&textprop, &textprop,
	NULL, 0,
	&hints,
	startup_state,
	NULL);


	XFree(startup_state);

	XMapWindow(Xdisplay, window_handle);
	XIfEvent(Xdisplay, &event, WaitForMapNotify, (char*)&window_handle);

	if ((del_atom = XInternAtom(Xdisplay, "WM_DELETE_WINDOW", 0)) != None) {
		XSetWMProtocols(Xdisplay, window_handle, &del_atom, 1);
	}
}

static int ctxErrorHandler( Display *dpy, XErrorEvent *ev )
{
	fputs("Error at context creation", stderr);
	return 0;
}

static void createTheRenderContext()
{
	int dummy;
	if (!glXQueryExtension(Xdisplay, &dummy, &dummy)) {
	fatalError("OpenGL not supported by X server\n");
}

#if USE_GLX_CREATE_CONTEXT_ATTRIB
#define GLX_CONTEXT_MAJOR_VERSION_ARB       0x2091
#define GLX_CONTEXT_MINOR_VERSION_ARB       0x2092
render_context = NULL;
if( isExtensionSupported( glXQueryExtensionsString(Xdisplay, DefaultScreen(Xdisplay)), "GLX_ARB_create_context" ) ) {
	typedef GLXContext (*glXCreateContextAttribsARBProc)(Display*, GLXFBConfig, GLXContext, Bool, const int*);
	glXCreateContextAttribsARBProc glXCreateContextAttribsARB = (glXCreateContextAttribsARBProc)glXGetProcAddressARB( (const GLubyte *) "glXCreateContextAttribsARB" );
	if( glXCreateContextAttribsARB ) {
		int context_attribs[] =
		{
		GLX_CONTEXT_MAJOR_VERSION_ARB, 3,
		GLX_CONTEXT_MINOR_VERSION_ARB, 0,
		//GLX_CONTEXT_FLAGS_ARB        , GLX_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB,
		None
		};

		int (*oldHandler)(Display*, XErrorEvent*) = XSetErrorHandler(&ctxErrorHandler);

		render_context = glXCreateContextAttribsARB( Xdisplay, fbconfig, 0, True, context_attribs );

		XSync( Xdisplay, False );
		XSetErrorHandler( oldHandler );

		fputs("glXCreateContextAttribsARB failed", stderr);
	} else {
		fputs("glXCreateContextAttribsARB could not be retrieved", stderr);
	}
} else {
	fputs("glXCreateContextAttribsARB not supported", stderr);
}

if(!render_context)
{
#else
	{
#endif
		render_context = glXCreateNewContext(Xdisplay, fbconfig, GLX_RGBA_TYPE, 0, True);
		if (!render_context) {
			fatalError("Failed to create a GL context\n");
		}
	}

	if (!glXMakeContextCurrent(Xdisplay, glX_window_handle, glX_window_handle, render_context)) {
		fatalError("glXMakeCurrent failed for window\n");
	}
}

static int updateTheMessageQueue()
{
	XEvent event;
	XConfigureEvent *xc;

	while (XPending(Xdisplay))
	{
		XNextEvent(Xdisplay, &event);
		switch (event.type)
		{
			case ClientMessage:
				if (event.xclient.data.l[0] == del_atom)
				{
					return 0;
				}
				break;

			case ConfigureNotify:
				xc = &(event.xconfigure);
				width = xc->width;
				height = xc->height;
				//printf("%d, %d\n", width, height);
				break;
			
			case KeyPress:
				myApp->HandleKeyUp(event.xkey.keycode);
				//printf("%d\n", event.xkey.keycode);
				break;
		}
	}
	return 1;
}

int main(int argc, char *argv[])
{
	// No comments
	width = 550;
	height = 800;
	bool graphicsVsync = true;
	
	/*INIReader reader(HEVA_CONFIG_PATH);
	if (reader.ParseError() < 0) {
		printf("Can't load 'test.ini'\n");
	}
	else {
		printf("Ini loaded.\n");
		
		width = reader.GetInteger("graphics", "width", 550);
		height = reader.GetInteger("graphics", "height", 800);
		graphicsVsync = reader.GetBoolean("graphics", "vsync", true);
	}*/
	
	for (int i=0; i<(sizeof(VisData)/sizeof(int)); i+=2) {
		if (VisData[i] == GLX_DOUBLEBUFFER) {
			VisData[i+1] = graphicsVsync;
		}
	}
	
	createTheWindow();
	createTheRenderContext();
	
	XClassHint* class_hints = XAllocClassHint();
	class_hints->res_name = (char*)malloc(sizeof(char)*5);
	class_hints->res_class = (char*)malloc(sizeof(char)*5);
	strcpy(class_hints->res_name, "heva");
	strcpy(class_hints->res_class, "heva");
	XSetClassHint(Xdisplay, window_handle, class_hints);
	

	Urho3D::Context* context = (new Urho3D::Context());
	myApp = new Heva(context, (void*)window_handle);
	myApp->Setup();
	myApp->Start();

	while (updateTheMessageQueue()) {
		myApp->HandleUpdate();
	}
	myApp->Stop();

	return 0;
}
#endif
