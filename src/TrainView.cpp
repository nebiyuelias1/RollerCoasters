/************************************************************************
     File:        TrainView.cpp

     Author:     
                  Michael Gleicher, gleicher@cs.wisc.edu

     Modifier
                  Yu-Chi Lai, yu-chi@cs.wisc.edu
     
     Comment:     
						The TrainView is the window that actually shows the 
						train. Its a
						GL display canvas (Fl_Gl_Window).  It is held within 
						a TrainWindow
						that is the outer window with all the widgets. 
						The TrainView needs 
						to be aware of the window - since it might need to 
						check the widgets to see how to draw

	  Note:        we need to have pointers to this, but maybe not know 
						about it (beware circular references)

     Platform:    Visio Studio.Net 2003/2005

*************************************************************************/

#include <iostream>
#include <Fl/fl.h>
#include <glm/glm.hpp>

// we will need OpenGL, and OpenGL needs windows.h
#include <windows.h>
//#include "GL/gl.h"
#include <glad/glad.h>
#include <glm/glm.hpp>
#include "GL/glu.h"

#include "TrainView.H"
#include "TrainWindow.H"
#include "Utilities/3DUtils.H"


#ifdef EXAMPLE_SOLUTION
#	include "TrainExample/TrainExample.H"
#endif

// Constants for spline types
constexpr auto LINEAR_TYPE = 1;
constexpr auto CARDINAL = 2;
constexpr auto B_SPLINE = 3;

// Used to divide control points
constexpr auto DIVIDE_LINE = 1000;

// The pi constant
constexpr auto M_PI = 3.14159265358979323846;

// These are the basis martices
glm::mat4 cardinalBasisMatrix = {
	{ -0.5,  1.5, -1.5,  0.5 },
	{    1, -2.5,    2, -0.5 },
	{ -0.5,    0,  0.5,    0 },
	{    0,    1,    0,    0 },
};

glm::mat4 bSplineBasisMatrix = {
	{ -0.1667,    0.5,   -0.5, 0.1667 },
	{     0.5,     -1,    0.5,      0 },
	{    -0.5,      0,    0.5,      0 },
	{  0.1667, 0.6667, 0.1667,      0 }
};


//************************************************************************
//
// * Constructor to set up the GL window
//========================================================================
TrainView::
TrainView(int x, int y, int w, int h, const char* l) 
	: Fl_Gl_Window(x,y,w,h,l)
//========================================================================
{
	mode( FL_RGB|FL_ALPHA|FL_DOUBLE | FL_STENCIL );

	resetArcball();
}

//************************************************************************
//
// * Reset the camera to look at the world
//========================================================================
void TrainView::
resetArcball()
//========================================================================
{
	// Set up the camera to look at the world
	// these parameters might seem magical, and they kindof are
	// a little trial and error goes a long way
	arcball.setup(this, 40, 250, .2f, .4f, 0);
}

//************************************************************************
//
// * FlTk Event handler for the window
//########################################################################
// TODO: 
//       if you want to make the train respond to other events 
//       (like key presses), you might want to hack this.
//########################################################################
//========================================================================
int TrainView::handle(int event)
{
	// see if the ArcBall will handle the event - if it does, 
	// then we're done
	// note: the arcball only gets the event if we're in world view
	if (tw->worldCam->value())
		if (arcball.handle(event)) 
			return 1;

	// remember what button was used
	static int last_push;

	switch(event) {
		// Mouse button being pushed event
		case FL_PUSH:
			last_push = Fl::event_button();
			// if the left button be pushed is left mouse button
			if (last_push == FL_LEFT_MOUSE  ) {
				doPick();
				damage(1);
				return 1;
			};
			break;

	   // Mouse button release event
		case FL_RELEASE: // button release
			damage(1);
			last_push = 0;
			return 1;

		// Mouse button drag event
		case FL_DRAG:

			// Compute the new control point position
			if ((last_push == FL_LEFT_MOUSE) && (selectedCube >= 0)) {
				ControlPoint* cp = &m_pTrack->points[selectedCube];

				double r1x, r1y, r1z, r2x, r2y, r2z;
				getMouseLine(r1x, r1y, r1z, r2x, r2y, r2z);

				double rx, ry, rz;
				mousePoleGo(r1x, r1y, r1z, r2x, r2y, r2z, 
								static_cast<double>(cp->pos.x), 
								static_cast<double>(cp->pos.y),
								static_cast<double>(cp->pos.z),
								rx, ry, rz,
								(Fl::event_state() & FL_CTRL) != 0);

				cp->pos.x = (float) rx;
				cp->pos.y = (float) ry;
				cp->pos.z = (float) rz;
				damage(1);
			}
			break;

		// in order to get keyboard events, we need to accept focus
		case FL_FOCUS:
			return 1;

		// every time the mouse enters this window, aggressively take focus
		case FL_ENTER:	
			focus(this);
			break;

		case FL_KEYBOARD:
		 		int k = Fl::event_key();
				int ks = Fl::event_state();
				if (k == 'p') {
					// Print out the selected control point information
					if (selectedCube >= 0) 
						printf("Selected(%d) (%g %g %g) (%g %g %g)\n",
								 selectedCube,
								 m_pTrack->points[selectedCube].pos.x,
								 m_pTrack->points[selectedCube].pos.y,
								 m_pTrack->points[selectedCube].pos.z,
								 m_pTrack->points[selectedCube].orient.x,
								 m_pTrack->points[selectedCube].orient.y,
								 m_pTrack->points[selectedCube].orient.z);
					else
						printf("Nothing Selected\n");

					return 1;
				};
				break;
	}

	return Fl_Gl_Window::handle(event);
}

//************************************************************************
//
// * this is the code that actually draws the window
//   it puts a lot of the work into other routines to simplify things
//========================================================================
void TrainView::draw()
{

	//*********************************************************************
	//
	// * Set up basic opengl informaiton
	//
	//**********************************************************************
	//initialized glad
	if (gladLoadGL())
	{
		//initiailize VAO, VBO, Shader...
	}
	else
		throw std::runtime_error("Could not initialize GLAD!");

	// Set up the view port
	glViewport(0,0,w(),h());

	// clear the window, be sure to clear the Z-Buffer too
	glClearColor(0,0,.3f,0);		// background should be blue

	// we need to clear out the stencil buffer since we'll use
	// it for shadows
	glClearStencil(0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	glEnable(GL_DEPTH);

	// Blayne prefers GL_DIFFUSE
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);

	// prepare for projection
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	setProjection();		// put the code to set up matrices here

	//######################################################################
	// TODO: 
	// you might want to set the lighting up differently. if you do, 
	// we need to set up the lights AFTER setting up the projection
	//######################################################################
	// enable the lighting
	glEnable(GL_COLOR_MATERIAL);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);

	// top view only needs one light
	if (tw->topCam->value()) {
		glDisable(GL_LIGHT1);
		glDisable(GL_LIGHT2);
	} else {
		glEnable(GL_LIGHT1);
		glEnable(GL_LIGHT2);
	}

	//*********************************************************************
	//
	// * set the light parameters
	//
	//**********************************************************************
	GLfloat lightPosition1[]	= {0,1,1,0}; // {50, 200.0, 50, 1.0};
	GLfloat lightPosition2[]	= {1, 0, 0, 0};
	GLfloat lightPosition3[]	= {0, -1, 0, 0};
	GLfloat yellowLight[]		= {0.5f, 0.5f, .1f, 1.0};
	GLfloat whiteLight[]			= {1.0f, 1.0f, 1.0f, 1.0};
	GLfloat blueLight[]			= {.1f,.1f,.3f,1.0};
	GLfloat grayLight[]			= {.3f, .3f, .3f, 1.0};

	glLightfv(GL_LIGHT0, GL_POSITION, lightPosition1);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, whiteLight);
	glLightfv(GL_LIGHT0, GL_AMBIENT, grayLight);

	glLightfv(GL_LIGHT1, GL_POSITION, lightPosition2);
	glLightfv(GL_LIGHT1, GL_DIFFUSE, yellowLight);

	glLightfv(GL_LIGHT2, GL_POSITION, lightPosition3);
	glLightfv(GL_LIGHT2, GL_DIFFUSE, blueLight);



	//*********************************************************************
	// now draw the ground plane
	//*********************************************************************
	// set to opengl fixed pipeline(use opengl 1.x draw function)
	glUseProgram(0);

	setupFloor();
	glDisable(GL_LIGHTING);
	drawFloor(200,10);


	//*********************************************************************
	// now draw the object and we need to do it twice
	// once for real, and then once for shadows
	//*********************************************************************
	glEnable(GL_LIGHTING);
	setupObjects();

	drawStuff();

	// this time drawing is for shadows (except for top view)
	if (!tw->topCam->value()) {
		setupShadows();
		drawStuff(true);
		unsetupShadows();
	}
}

//************************************************************************
//
// * This sets up both the Projection and the ModelView matrices
//   HOWEVER: it doesn't clear the projection first (the caller handles
//   that) - its important for picking
//========================================================================
void TrainView::
setProjection()
//========================================================================
{
	// Compute the aspect ratio (we'll need it)
	float aspect = static_cast<float>(w()) / static_cast<float>(h());

	// Check whether we use the world camp
	if (tw->worldCam->value())
		arcball.setProjection(false);
	// Or we use the top cam
	else if (tw->topCam->value()) {
		float wi, he;
		if (aspect >= 1) {
			wi = 110;
			he = wi / aspect;
		} 
		else {
			he = 110;
			wi = he * aspect;
		}

		// Set up the top camera drop mode to be orthogonal and set
		// up proper projection matrix
		glMatrixMode(GL_PROJECTION);
		glOrtho(-wi, wi, -he, he, 200, -200);
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		glRotatef(-90,1,0,0);
	} 
	// Or do the train view or other view here
	//####################################################################
	// TODO: 
	// put code for train view projection here!	
	//####################################################################
	else {
#ifdef EXAMPLE_SOLUTION
		trainCamView(this,aspect);
#endif
	}
}

//************************************************************************
//
// * this draws all of the stuff in the world
//
//	NOTE: if you're drawing shadows, DO NOT set colors (otherwise, you get 
//       colored shadows). this gets called twice per draw 
//       -- once for the objects, once for the shadows
//########################################################################
// TODO: 
// if you have other objects in the world, make sure to draw them
//########################################################################
//========================================================================
void TrainView::drawStuff(bool doingShadows)
{
	// Draw the control points
	// don't draw the control points if you're driving 
	// (otherwise you get sea-sick as you drive through them)
	if (!tw->trainCam->value()) {
		for(size_t i=0; i<m_pTrack->points.size(); ++i) {
			if (!doingShadows) {
				if ( ((int) i) != selectedCube)
					glColor3ub(240, 60, 60);
				else
					glColor3ub(240, 240, 30);
			}
			m_pTrack->points[i].draw();
		}
	}
	// draw the track
	drawTrack(doingShadows);

	// draw the train
	//####################################################################
	// TODO: 
	//	call your own train drawing code
	//####################################################################
#ifdef EXAMPLE_SOLUTION
	// don't draw the train if you're looking out the front window
	if (!tw->trainCam->value())
		drawTrain(this, doingShadows);
#endif
}

// 
//************************************************************************
//
// * this tries to see which control point is under the mouse
//	  (for when the mouse is clicked)
//		it uses OpenGL picking - which is always a trick
//########################################################################
// TODO: 
//		if you want to pick things other than control points, or you
//		changed how control points are drawn, you might need to change this
//########################################################################
//========================================================================
void TrainView::
doPick()
//========================================================================
{
	// since we'll need to do some GL stuff so we make this window as 
	// active window
	make_current();		

	// where is the mouse?
	int mx = Fl::event_x(); 
	int my = Fl::event_y();

	// get the viewport - most reliable way to turn mouse coords into GL coords
	int viewport[4];
	glGetIntegerv(GL_VIEWPORT, viewport);

	// Set up the pick matrix on the stack - remember, FlTk is
	// upside down!
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity ();
	gluPickMatrix((double)mx, (double)(viewport[3]-my), 
						5, 5, viewport);

	// now set up the projection
	setProjection();

	// now draw the objects - but really only see what we hit
	GLuint buf[100];
	glSelectBuffer(100,buf);
	glRenderMode(GL_SELECT);
	glInitNames();
	glPushName(0);

	// draw the cubes, loading the names as we go
	for(size_t i=0; i<m_pTrack->points.size(); ++i) {
		glLoadName((GLuint) (i+1));
		m_pTrack->points[i].draw();
	}

	// go back to drawing mode, and see how picking did
	int hits = glRenderMode(GL_RENDER);
	if (hits) {
		// warning; this just grabs the first object hit - if there
		// are multiple objects, you really want to pick the closest
		// one - see the OpenGL manual 
		// remember: we load names that are one more than the index
		selectedCube = buf[3]-1;
	} else // nothing hit, nothing selected
		selectedCube = -1;

	printf("Selected Cube %d\n",selectedCube);
}

/// <summary>
/// Draw the track for the train
/// </summary>
/// <param name="doingShadows">Indicates whether or not we're using shadows</param>
void TrainView::drawTrack(bool doingShadows)
{
	for (size_t i = 0; i < m_pTrack->points.size(); ++i) {
		// Get the four control points for this segment
		Pnt3f p1 = m_pTrack->points[i].pos;
		Pnt3f p2 = m_pTrack->points[(i + 1) % m_pTrack->points.size()].pos;
		Pnt3f p3 = m_pTrack->points[(i + 2) % m_pTrack->points.size()].pos;
		Pnt3f p4 = m_pTrack->points[(i + 3) % m_pTrack->points.size()].pos;

		// Get the four orientation of the control points for this segment
		Pnt3f o1 = m_pTrack->points[i].orient;
		Pnt3f o2 = m_pTrack->points[(i + 1) % m_pTrack->points.size()].orient;
		Pnt3f o3 = m_pTrack->points[(i + 2) % m_pTrack->points.size()].orient;
		Pnt3f o4 = m_pTrack->points[(i + 3) % m_pTrack->points.size()].orient;

		Pnt3f qt, orient_t;

		// The parameter - could be considered as time
		float t = 0;

		// The increment used to draw the curves
		float increment = 1.0f / DIVIDE_LINE;

		switch (tw->splineBrowser->value())
		{
			case LINEAR_TYPE:
				qt = (1 - t) * p1 + t * p2;
				break;
			case CARDINAL:
				qt = p2;
				break;
			case B_SPLINE:
				qt = p1 * (1.0f / 6.0f) + p2 * (4.0f / 6.0f) + p3 * (1.0f / 6.0f);
				break;
		}

		for (size_t j = 0; j < DIVIDE_LINE; j++)
		{
			// Since qt is going to be recomputed hold the hold value in variable qt0
			Pnt3f qt0 = qt;

			// The parameter vector T
			glm::vec4 T(pow(t, 3), pow(t, 2), t, 1);

			Pnt3f tangent;

			t += increment;

			switch (tw->splineBrowser->value())
			{
			case LINEAR_TYPE:
				qt = (1 - t) * p1 + t * p2;

				orient_t = (1 - t) * o1 + t * o2;

				tangent = Pnt3f(p2.x - p1.x, p2.y - p1.y, p2.z - p1.z);

				break;
			case CARDINAL:
				glm::vec4 C = cardinalBasisMatrix * T;
				qt = p1 * C[0] + p2 * C[1] + p3 * C[2] + p4 * C[3];

				orient_t = o1 * C[0] + o2 * C[1] + o3 * C[2] + o4 * C[3];

				T[0] = 3 * pow(t, 2);
				T[1] = 2 * t;
				T[2] = 1;
				T[3] = 0;

				glm::vec4 C1 = cardinalBasisMatrix * T;

				tangent = p1 * C1[0] + p2 * C1[1] + p3 * C1[2] + p4 * C1[3];

				break;
			case B_SPLINE:
				C = bSplineBasisMatrix * T;
				qt = p1 * C[0] + p2 * C[1] + p3 * C[2] + p4 * C[3];

				orient_t = o1 * C[0] + o2 * C[1] + o3 * C[2] + o4 * C[3];

				T[0] = 3 * pow(t, 2);
				T[1] = 2 * t;
				T[2] = 1;
				T[3] = 0;
				
				break;
			}

			orient_t.normalize();

			Pnt3f cross_t = (qt - qt0) * orient_t;
			cross_t.normalize();
			cross_t = cross_t * 2.5f;

			tangent.normalize();

			glBegin(GL_LINES);
			if (!doingShadows)
				glColor3ub(40, 30, 40);
			glVertex3f(qt0.x + cross_t.x, qt0.y + cross_t.y, qt0.z + cross_t.z);
			glVertex3f(qt.x + cross_t.x, qt.y + cross_t.y, qt.z + cross_t.z);

			glVertex3f(qt0.x - cross_t.x, qt0.y - cross_t.y, qt0.z - cross_t.z);
			glVertex3f(qt.x - cross_t.x, qt.y - cross_t.y, qt.z - cross_t.z);
			glEnd();
			glLineWidth(4);

			Pnt3f AC, AB;

			AC = Pnt3f(qt.x - qt0.x, qt.y - qt0.y, qt.z - qt0.z);
			AB = Pnt3f(1.0f, 0.0f, 0.0f);

			float cos_y = (float)(AC.x * AB.x + AC.y * AB.y + AC.z * AB.z)
				/ (abs(sqrt(pow(AC.x, 2) + pow(AC.y, 2) + pow(AC.z, 2))) * abs(sqrt(pow(AB.x, 2) + pow(AB.y, 2) + pow(AB.z, 2))));

			float angle_y = acos(cos_y) * 180.0 / M_PI;

			if ((AC.x < 0 && AC.z > 0) || (AC.x > 0 && AC.z > 0))
				angle_y = -angle_y;

			AB = Pnt3f(0.0f, 1.0f, 0.0f);

			float cos = (float)(orient_t.x * AB.x + orient_t.y * AB.y + orient_t.z * AB.z)
				/ (abs(sqrt(pow(orient_t.x, 2) + pow(orient_t.y, 2) + pow(orient_t.z, 2))) * abs(sqrt(pow(AB.x, 2) + pow(AB.y, 2) + pow(AB.z, 2))));

			float angle = acos(cos) * 180.0 / M_PI;

			if (j % 100 == 0)
			{
				glPushMatrix();
				glTranslatef(qt.x, qt.y, qt.z);
				glRotatef(angle_y, 0.0f, 1.0f, 0.0f);
				glRotatef(angle, 1.0f, 0.0f, 0.0f);
				
				//Down
				glBegin(GL_QUADS);
				if (!doingShadows)
					glColor3ub(100, 80, 100);
				glVertex3f(-1.5, 0, 5);
				glVertex3f(1.5, 0, 5);
				glVertex3f(1.5, 0, -5);
				glVertex3f(-1.5, 0, -5);
				glEnd();

				//Up
				glBegin(GL_QUADS);
				if (!doingShadows)
					glColor3ub(40, 40, 40);
				glVertex3f(-1.5, 1, 5);
				glVertex3f(1.5, 1, 5);
				glVertex3f(1.5, 1, -5);
				glVertex3f(-1.5, 1, -5);
				glEnd();

				//Left
				glBegin(GL_QUADS);
				if (!doingShadows)
					glColor3ub(100, 80, 100);
				glVertex3f(-1.5, 0, 5);
				glVertex3f(-1.5, 1, 5);
				glVertex3f(-1.5, 1, -5);
				glVertex3f(-1.5, 0, -5);
				glEnd();

				//Right
				glBegin(GL_QUADS);
				if (!doingShadows)
					glColor3ub(100, 80, 100);
				glVertex3f(1.5, 0, 5);
				glVertex3f(1.5, 1, 5);
				glVertex3f(1.5, 1, -5);
				glVertex3f(1.5, 0, -5);
				glEnd();

				//Front
				glBegin(GL_QUADS);
				if (!doingShadows)
					glColor3ub(100, 80, 100);
				glVertex3f(-1.5, 1, 5);
				glVertex3f(1.5, 1, 5);
				glVertex3f(1.5, 0, 5);
				glVertex3f(-1.5, 0, 5);
				glEnd();

				//Front
				glBegin(GL_QUADS);
				if (!doingShadows)
					glColor3ub(100, 80, 100);
				glVertex3f(-1.5, 1, -5);
				glVertex3f(1.5, 1, -5);
				glVertex3f(1.5, 0, -5);
				glVertex3f(-1.5, 0, -5);
				glEnd();

				glPopMatrix();
			}	
		}
	}
}