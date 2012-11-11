#include <GL/glew.h>
#include <GL/freeglut.h>
#include <iostream>

#include <string>
#include <vector>
#include <fstream>

#include "PS_Base/PS_MathBase.h"
#include "PS_Base/PS_Logger.h"
#include "PS_Base/PS_FileDirectory.h"
#include "PS_BlobTreeRender/PS_OclPolygonizer.h"
#include "PS_Graphics/PS_ArcBallCamera.h"
#include "PS_Graphics/PS_GLFuncs.h"
#include "PS_Graphics/PS_GLSurface.h"
#include "PS_Graphics/PS_SketchConfig.h"
#include "PS_Graphics/AffineWidgets.h"
#include "PS_Graphics/PS_Vector.h"

#include "PS_Deformable/PS_Deformable.h"
#include "PS_Deformable/PS_VegWriter.h"
#include "PS_Deformable/Avatar.h"

using namespace std;
using namespace PS;
using namespace PS::FILESTRINGUTILS;
using namespace PS::HPC;

#define WINDOW_WIDTH 1024
#define WINDOW_HEIGHT 800
#define FOVY 45.0
#define ZNEAR 0.01
#define ZFAR 100.0
#define DEFAULT_TIMER_MILLIS 33

#define INDEX_CAMERA_INFO 0
#define INDEX_HAPTIC_INFO 1
#define INDEX_GPU_INFO 	   2


enum HAPTICMODES {hmDynamic, hmAddFixed, hmRemoveFixed, hmCount};

//Application Settings
class AppSettings{
public:

	//Set to default values in constructor
	AppSettings(){
		this->bDrawWireFrame = false;
		this->bPanCamera = false;
		this->bShowElements = false;
		this->idxCollisionFace = -1;
		this->millis = DEFAULT_TIMER_MILLIS;
		this->hapticMode = 0;
		this->toolThickness = 0.1;
	}

public:
	bool bPanCamera;
	bool bDrawWireFrame;
	bool bShowElements;

	int idxCollisionFace;
	U32   millis;
	int appWidth;
	int appHeight;
	int hapticMode;
	double toolThickness;

	vec3d worldAvatarPos;
	vec3d worldDragStart;
	vec3d worldDragEnd;
	vec2i screenDragStart;
	vec2i screenDragEnd;
};

//Global Variables
AvatarCube* g_lpAvatarCube = NULL;
TranslateWidget*	g_lpTranslateWidget = NULL;
PS::ArcBallCamera g_arcBallCam;
PS::HPC::GPUPoly* g_lpBlobRender = NULL;
Deformable* g_lpDeformable = NULL;

//Info Lines
std::vector<std::string> g_infoLines;

GLSurface* g_lpSurface = NULL;
AppSettings g_appSettings;
GLuint g_uiShader;


////////////////////////////////////////////////
//Function Prototype
void Draw();
void DrawBox(const vec3f& lo, const vec3f& hi, const vec3f& color, float lineWidth);
void Resize(int w, int h);
void TimeStep(int t);
void MousePress(int button, int state, int x, int y);
void MouseMove(int x, int y);
void MouseWheel(int button, int dir, int x, int y);
bool ScreenToWorld(const svec3f& screenP, svec3f& worldP);

void Keyboard(int key, int x, int y);
void DrawText(const char* chrText, int x, int y);
string GetGPUInfo();

//Settings
void LoadSettings();
void SaveSettings();
////////////////////////////////////////////////
//Vertex Shader Code
const char * g_lpVertexShaderCode = 
	"varying vec3 N;"
	"varying vec3 V; "
	"void main(void) {"
	"gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;"
	"gl_FrontColor = gl_Color;"
	"N = normalize(gl_NormalMatrix * gl_Normal);"
	"V = vec3(gl_ModelViewMatrix * gl_Vertex); }";

//Fragment Shader Code
const char* g_lpFragShaderCode =
	"varying vec3 N;"
	"varying vec3 V;"
	"void main(void) {"
	"vec3 L = normalize(gl_LightSource[0].position.xyz - V);"
	"vec3 E = normalize(-V);"
	"vec3 R = normalize(-reflect(L, N));"
	"vec4 Iamb = 0.5 * gl_LightSource[0].ambient * gl_Color;"
	"vec4 Idif = (gl_LightSource[0].diffuse * gl_Color) * max(dot(N,L), 0.0);"
	"vec4 Ispec = (gl_LightSource[0].specular * (vec4(0.8, 0.8, 0.8, 0.8) + 0.2 * gl_Color)) * pow(max(dot(R, E), 0.0), 32.0);"
	"gl_FragColor = gl_FrontLightModelProduct.sceneColor + Iamb + Idif + Ispec;	}";

////////////////////////////////////////////////////////////////////////////////////////


void Draw()
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	//Render
	g_arcBallCam.look();

	//glTranslatef(0,0,10);
	/*
	glTranslatef(g_appSettings.pan.x, g_appSettings.pan.y, 0.0f);
	svec3f p = g_arcBallCam.getCoordinates();
	svec3f c = g_arcBallCam.getCenter();
	gluLookAt(p.x, p.y, p.z, c.x, c.y, c.z, 0.0f, 1.0f, 0.0f);
	*/
	//Use Shader Effect
	/*
	glUseProgram(g_uiShader);
	if(g_lpBlobRender)
	{
		g_lpBlobRender->drawBBox();
		g_lpBlobRender->drawMesh(g_appSettings.bDrawWireFrame);
	}
	glUseProgram(0);
	*/


	//Draw Deformable Model
	g_lpDeformable->draw();

	//Draw Interaction Avatar
	if(g_appSettings.hapticMode == hmDynamic && g_lpAvatarCube)
	{
		vec3d wpos = g_appSettings.worldAvatarPos;
		glPushMatrix();
			glTranslated(wpos.x, wpos.y, wpos.z);
			g_lpAvatarCube->draw();

			glScalef(0.4, 0.4, 0.4);
			glDisable(GL_DEPTH_TEST);
			g_lpTranslateWidget->draw();
			glEnable(GL_DEPTH_TEST);
		glPopMatrix();
	}


	//Draw Haptic Line
	if(g_lpDeformable->isHapticInProgress())
	{
		GLint vp[4];
		glGetIntegerv(GL_VIEWPORT, vp);
		//svec3f s1 = g_appSettings.worldDragStart;
		//svec3f s2 = g_appSettings.worldDragEnd;
		vec2i s1 = g_appSettings.screenDragStart;
		vec2i s2 = g_appSettings.screenDragEnd;

		glMatrixMode(GL_PROJECTION);
		glPushMatrix();
		glLoadIdentity();
		glOrtho(0.0, vp[2], vp[3], 0.0, -1.0, 1.0);

		glMatrixMode(GL_MODELVIEW);
		glPushMatrix();
		glLoadIdentity();

		glColor3f(1,0,0);
		glPushAttrib(GL_ALL_ATTRIB_BITS);
		glLineWidth(1.0);

		glBegin(GL_LINES);
			glVertex2f(s1.x, s1.y);
			glVertex2f(s2.x, s2.y);
		glEnd();
		glPopAttrib();

		glMatrixMode(GL_MODELVIEW);
		glPopMatrix();
		glMatrixMode(GL_PROJECTION);
		glPopMatrix();
	}

	//Write Camera Info
	{
		char chrMsg[1024];
		sprintf(chrMsg,"Camera [Roll=%.1f, Tilt=%.1f, PanX=%.2f, PanY=%.2f]",
				g_arcBallCam.getRoll(),
				g_arcBallCam.getTilt(),
				g_arcBallCam.getPan().x,
				g_arcBallCam.getPan().y);
		g_infoLines[INDEX_CAMERA_INFO] = string(chrMsg);
	}

	//Write Model Info
	{
		for(size_t i=0; i<g_infoLines.size(); i++)
			DrawText(g_infoLines[i].c_str(), 10, 20 + i * 15);
	}

	glutSwapBuffers();
}

void DrawBox(const vec3f& lo, const vec3f& hi, const vec3f& color, float lineWidth)
{
	float l = lo.x; float r = hi.x;
	float b = lo.y; float t = hi.y;
	float n = lo.z; float f = hi.z;

	GLfloat vertices [][3] = {{l, b, f}, {l, t, f}, {r, t, f},
							  {r, b, f}, {l, b, n}, {l, t, n},
							  {r, t, n}, {r, b, n}};

	glPushAttrib(GL_ALL_ATTRIB_BITS);
		glColor3f(color.x, color.y, color.z);
		glLineWidth(lineWidth);
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		glBegin(GL_QUADS);
			glVertex3fv(vertices[0]); glVertex3fv(vertices[3]); glVertex3fv(vertices[2]); glVertex3fv(vertices[1]);
			glVertex3fv(vertices[4]); glVertex3fv(vertices[5]); glVertex3fv(vertices[6]); glVertex3fv(vertices[7]);
			glVertex3fv(vertices[3]); glVertex3fv(vertices[0]); glVertex3fv(vertices[4]); glVertex3fv(vertices[7]);
			glVertex3fv(vertices[1]); glVertex3fv(vertices[2]); glVertex3fv(vertices[6]); glVertex3fv(vertices[5]);
			glVertex3fv(vertices[2]); glVertex3fv(vertices[3]); glVertex3fv(vertices[7]); glVertex3fv(vertices[6]);
			glVertex3fv(vertices[5]); glVertex3fv(vertices[4]); glVertex3fv(vertices[0]); glVertex3fv(vertices[1]);
		glEnd();
	glPopAttrib();
}

void DrawText(const char* chrText, int x, int y)
{
	GLint vp[4];
	glGetIntegerv(GL_VIEWPORT, vp);

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	glOrtho(0, vp[2], vp[3], 0, -1, 1);


	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();

	float clFont[] = { 0, 0, 1, 1 };
	DrawString(chrText, x,  y, clFont, GLUT_BITMAP_8_BY_13);

	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
}

void Resize(int w, int h)
{
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(FOVY, (double)w/(double)h, ZNEAR, ZFAR);
	//glOrtho(-2.0f, 2.0f, -2.0f, 2.0f, -2.0f, 2.0f);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
}

int ConvertScreenToWorld(int x, int y, vec3d& world)
{
	GLdouble model[16];
	glGetDoublev(GL_MODELVIEW_MATRIX, model);

	GLdouble proj[16];
	glGetDoublev(GL_PROJECTION_MATRIX, proj);

	GLint view[4];
	glGetIntegerv(GL_VIEWPORT, view);

	int winX = x;
	int winY = view[3] - 1 - y;

	float zValue;
	glReadPixels(winX, winY, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &zValue);

	GLubyte stencilValue;
	glReadPixels(winX, winY, 1, 1, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE,
			&stencilValue);

	GLdouble worldX, worldY, worldZ;
	gluUnProject(winX, winY, zValue, model, proj, view,
					&worldX, &worldY, &worldZ);

	world = vec3d(worldX, worldY, worldZ);

	return stencilValue;
}

void MousePress(int button, int state, int x, int y)
{
	if(button == GLUT_RIGHT_BUTTON)
	{
		if((state == GLUT_DOWN) && (g_appSettings.hapticMode == hmDynamic))
		{
			g_appSettings.screenDragStart = vec2i(x, y);
			g_appSettings.screenDragEnd = vec2i(x, y);

			g_lpDeformable->hapticStart(0);
		}
		else
			g_lpDeformable->hapticEnd();
	}
	else if(button == GLUT_LEFT_BUTTON){
		if(state == GLUT_DOWN)
		{
			vec3d wpos;
			int stencilValue = ConvertScreenToWorld(x, y, wpos);
			if (stencilValue == 1)
			{
				vec3d closestVertex;
				int idxVertex = g_lpDeformable->pickVertex(wpos, closestVertex);
				g_lpDeformable->setPulledVertex(idxVertex);
				LogInfoArg1("Selected Vertex Index = %d ", idxVertex);
			}
		}

/*
			int stencilValue = ConvertScreenToWorld(x, y, g_appSettings.worldAvatarPos);
			if (stencilValue == 1)
			{
				double closestVertex[3];
				int idxVertex = g_lpDeformable->pickVertex(worldX, worldY, worldZ, &closestVertex[0]);

				if(g_appSettings.hapticMode == hmForce)
				{
					if(g_lpDeformable->hapticStart(idxVertex))
					{
						g_appSettings.worldDragStart = vec3d(&closestVertex[0]);
						g_appSettings.screenDragStart = vec2i(x, y);
					}
				}
				else if(g_appSettings.hapticMode == hmAddFixed)
				{

				}
				else if(g_appSettings.hapticMode == hmRemoveFixed)
				{

				}
			}
*/

	}

	//Camera
	g_arcBallCam.mousePress(button, state, x, y);

	//Update selection
	glutPostRedisplay();
}

/*!
 *
 */
void MouseMove(int x, int y)
{
	if(g_lpDeformable->isHapticInProgress() && g_appSettings.hapticMode == hmDynamic)
	{
		vec3d ptPrevAvatarPos = g_appSettings.worldAvatarPos;

		double dx = x - g_appSettings.screenDragStart.x;
		double dy = g_appSettings.screenDragStart.y - y;
		dx *= 0.001;
		dy *= 0.001;

		g_appSettings.screenDragStart = vec2i(x, y);

		string strAxis;
		switch(TheUITransform::Instance().axis)
		{
		case uiaX:
			g_appSettings.worldAvatarPos.x += dx;
			strAxis = "X";
			break;
		case uiaY:
			g_appSettings.worldAvatarPos.y += dy;
			strAxis = "Y";
			break;
		case uiaZ:
			g_appSettings.worldAvatarPos.z += dx;
			strAxis = "Z";
			break;

		case uiaFree:
			g_appSettings.worldAvatarPos = g_appSettings.worldAvatarPos + vec3d(dx, dy, 0.0);
			strAxis = "FREE";
			break;
		}

		vec3d wpos = g_appSettings.worldAvatarPos;

		char buffer[1024];
		sprintf(buffer, "HAPTIC DELTA=(%.4f, %.4f), AVATAR=(%.4f, %0.4f, %.4f), AXIS=%s PRESS F4 To Change.",
				 dx, dy, wpos.x, wpos.y, wpos.z, strAxis.c_str());
		g_infoLines[INDEX_HAPTIC_INFO] = string(buffer);

		vec3d lower = g_lpAvatarCube->lower() + wpos;
		vec3d upper = g_lpAvatarCube->upper() + wpos;

		vector<vec3d> arrVertices;
		vector<int> arrIndices;
		g_lpDeformable->pickVertices(lower, upper, arrVertices, arrIndices);
		for(U32 i=0; i<arrIndices.size() ;i++)
			printf("Collision Index = %d \n", arrIndices[i]);

		//Collided Now
		if(arrIndices.size() > 0)
		{
			//Set Affected Vertices
			//Check against six faces of avatar to find the intersection
			//Front Face Normal
			vec3d n[6];
			vec3d s[6];

			//X. Left and Right
			n[0] = vec3d(-1, 0, 0);
			n[1] = vec3d(1, 0, 0);

			//Y. Bottom and Top
			n[2] = vec3d(0, -1, 0);
			n[3] = vec3d(0, 1, 0);

			//Z. Back and Front
			n[4] = vec3d(0, 0, -1);
			n[5] = vec3d(0, 0, 1);

			//Sample Point to use
			s[0] = lower;
			s[1] = upper;
			s[2] = lower;
			s[3] = upper;
			s[4] = lower;
			s[5] = upper;

			//Previously Detected Face?
			if(g_appSettings.idxCollisionFace < 0)
			{
				//Detect Collision Face
				double minDot = GetMaxLimit<double>();
				int idxMin = 0;
				for(int i=0; i<(int)arrVertices.size(); i++)
				{
					vec3d p = arrVertices[i];
					for(int j=0; j<6; j++)
					{
						double dot = vec3d::dot(s[j] - p, n[j]);
						if(dot < minDot)
						{
							minDot = dot;
							idxMin = j;
						}
					}
					g_appSettings.idxCollisionFace = idxMin;
					//vec3d q = p - n[idxMin] * minDot;
				}
			}

			//Compute Displacement
			vector<vec3d> arrDisplacements;
			arrDisplacements.resize(arrVertices.size());
			int idxFace = g_appSettings.idxCollisionFace;
			for(int i=0; i< (int)arrVertices.size(); i++)
			{
				double dot = vec3d::dot(s[idxFace] - arrVertices[i], n[idxFace]);
				string arrFaces [] = {"LEFT", "RIGHT", "BOTTOM", "TOP", "NEAR", "FAR"};
				printf("Face[%d] = %s, dot = %.4f\n", idxFace,
						arrFaces[idxFace].c_str(), dot);
				arrDisplacements[i] = n[idxFace] * dot;
			}

			//Apply displacements to the model
			g_lpDeformable->hapticSetCurrentDisplacements(arrIndices, arrDisplacements);
		}
		else
			g_appSettings.idxCollisionFace = -1;
	}
	/*
	if(g_lpDeformable->isHapticInProgress())
	{
		vec3f ptDelta(x - g_appSettings.screenDragStart.x,
					   g_appSettings.screenDragStart.y - y, 0);
		//for(int i=0; i<3 && i != g_appSettings.axis; i++)
			//vsetElement3f(ptDelta, i, 0.0f);
		string strAxis = "X";
		if(TheUITransform::Instance().axis == uiaX)
		{
			strAxis = "X";
			ptDelta.y = 0.0f;
			ptDelta.z = 0.0f;
		}
		else if(TheUITransform::Instance().axis == uiaY)
		{
			strAxis = "Y";
			ptDelta.x = 0.0f;
			ptDelta.z = 0.0f;
		}
		else if(TheUITransform::Instance().axis == uiaZ)
		{
			strAxis = "Z";
			ptDelta.y = 0.0f;
			ptDelta.x = 0.0f;
			ptDelta.z = ptDelta.x;
		}
		else
			strAxis = "FREE";

		//Scale
		ptDelta = ptDelta * 0.0001;

		char buffer[1024];
		sprintf(buffer, "HAPTIC DELTA VECTOR=(%.4f, %.4f, %.4f), AXIS=%s", ptDelta.x, ptDelta.y, ptDelta.z, strAxis.c_str());
		g_infoLines[INDEX_HAPTIC_INFO] = string(buffer);
		g_appSettings.screenDragEnd = vec2i(x, y);

		g_lpDeformable->hapticSetCurrentDisplacement(ptDelta.x, ptDelta.y, ptDelta.z);
	}
	*/

	g_arcBallCam.mouseMove(x, y);
	glutPostRedisplay();
}

void MouseWheel(int button, int dir, int x, int y)
{
	g_arcBallCam.mouseWheel(button, dir, x, y);
	glutPostRedisplay();
}

void Close()
{
	//Cleanup
	cout << "Cleanup Memory objects" << endl;
	SAFE_DELETE(g_lpSurface);
	SAFE_DELETE(g_lpBlobRender);
	SAFE_DELETE(g_lpDeformable);
	SAFE_DELETE(g_lpAvatarCube);

	PS::TheEventLogger::Instance().flush();
}

string QueryOGL(GLenum name)
{
	string strOut = "N/A";
	const char* lpRes = reinterpret_cast<const char*>(glGetString(name));
	if(lpRes == NULL)
		return strOut;
	else
	{
		strOut = string(lpRes);
		return strOut;
	}
}

string GetGPUInfo()
{
	string strVendorName = QueryOGL(GL_VENDOR);
	string strRenderer   = QueryOGL(GL_RENDERER);
	string strVersion	 = QueryOGL(GL_VERSION);
	string strExtensions = QueryOGL(GL_EXTENSIONS);
	cout << "GPU VENDOR: " << strVendorName << endl;
	cout << "GPU RENDERER: " << strRenderer << endl;
	cout << "GPU VERSION: " << strVersion << endl;
	
	LogInfoArg1("GPU VENDOR: %s", strVendorName.c_str());
	LogInfoArg1("GPU RENDERER: %s", strRenderer.c_str());
	LogInfoArg1("GPU VERSION: %s", strVersion.c_str());


	if(strcmp(strVendorName.c_str(), "Intel") == 0)
	{
		cout << "WARNING: Integrated GPU is being used!" << endl;
		LogWarning("Non-Discrete GPU selected for rendering!");
	}
	//cout << "GPU EXTENSIONS: " << strExtensions << endl;
	return  string("GPU: ") + strVendorName + ", " + strRenderer + ", " + strVersion;
}

bool ScreenToWorld(const svec3f& screenP, svec3f& worldP)
{
    GLdouble ox, oy, oz;
    GLdouble mv[16];
    GLdouble pr[16];
    GLint vp[4];

    glGetDoublev(GL_MODELVIEW_MATRIX, mv);
    glGetDoublev(GL_PROJECTION_MATRIX, pr);
    glGetIntegerv(GL_VIEWPORT, vp);
    if(gluUnProject(screenP.x, screenP.y, screenP.z, mv, pr, vp, &ox, &oy, &oz) == GL_TRUE)
    {
        worldP = svec3f(ox, oy, oz);
        return true;
    }

    return false;
}

void Keyboard(int key, int x, int y)
{
	switch(key)
	{
		case(GLUT_KEY_F2):
		{
			g_appSettings.bDrawWireFrame = !g_appSettings.bDrawWireFrame;
			LogInfoArg1("Draw wireframe mode: %d", g_appSettings.bDrawWireFrame);
			break;
		}

		case(GLUT_KEY_F3):
		{
			g_appSettings.hapticMode ++;
			g_appSettings.hapticMode = g_appSettings.hapticMode % (int)hmCount;
			DAnsiStr strMode = "Force";

			if(g_appSettings.hapticMode == hmDynamic)
				strMode = "FORCE";
			else if(g_appSettings.hapticMode == hmAddFixed)
				strMode = "ADDFIXED";
			else if(g_appSettings.hapticMode == hmRemoveFixed)
				strMode = "REMFIXED";
			LogInfoArg2("Haptic mode: %d, %s", g_appSettings.hapticMode, strMode.cptr());
			break;
		}

		case(GLUT_KEY_F4):
		{
			//Set UIAxis
			TheUITransform::Instance().axis = (TheUITransform::Instance().axis + 1) % 4;
			LogInfoArg1("Change haptic axis to %d", TheUITransform::Instance().axis);
			g_lpTranslateWidget->createWidget();

			break;
		}

		case(GLUT_KEY_F5):
		{
			g_appSettings.toolThickness -= 0.1;
			g_appSettings.toolThickness = MATHMIN(g_appSettings.toolThickness, 0.1);
			LogInfoArg1("Decrease tool thickness to: %.4f", g_appSettings.toolThickness);
			double w = g_appSettings.toolThickness * 0.5;
			SAFE_DELETE(g_lpAvatarCube);
			g_lpAvatarCube = new AvatarCube(vec3d(-0.1,-0.2,-w), vec3d(0.1, 0.2, w));
			break;
		}

		case(GLUT_KEY_F6):
		{
			g_appSettings.toolThickness += 0.1;
			g_appSettings.toolThickness = MATHMAX(g_appSettings.toolThickness, 0.1);
			LogInfoArg1("Increase tool thickness to: %.4f", g_appSettings.toolThickness);
			double w = g_appSettings.toolThickness * 0.5;
			SAFE_DELETE(g_lpAvatarCube);
			g_lpAvatarCube = new AvatarCube(vec3d(-0.1,-0.2,-w), vec3d(0.1, 0.2, w));
			break;
		}

		case(GLUT_KEY_F11):
		{
			//Saving Settings and Exit
			SaveSettings();

			LogInfo("Exiting.");
			glutLeaveMainLoop();
			break;
		}

	}

	glutPostRedisplay();
}


void TimeStep(int t)
{
	g_lpDeformable->timestep();
	glutTimerFunc(g_appSettings.millis, TimeStep, 0);
}

void LoadSettings()
{
	LogInfo("Loading Settings from the ini file.");

	CSketchConfig cfg(ChangeFileExt(GetExePath(), ".ini"), CSketchConfig::fmRead);
	vec3f pos = cfg.readVec3f("AVATAR", "POS");
	g_appSettings.worldAvatarPos = vec3d(pos.x, pos.y, pos.z);

	DAnsiStr strVegFile = cfg.readString("MODEL", "VEGFILE", "");
	DAnsiStr strObjFile = cfg.readString("MODEL", "OBJFILE", "");
	DAnsiStr strBlobFile = cfg.readString("MODEL", "BLOBTREEFILE", "");


	int ctFixed = 	cfg.readInt("MODEL", "FIXEDVERTICESCOUNT", 0);
	vector<int> vFixedVertices;
	bool bres = cfg.readIntArray("MODEL", "FIXEDVERTICES", ctFixed, vFixedVertices);
	if(!bres)
		LogError("Unable to read specified number of fixed vertices!");

	//Translation Widget
	TheUITransform::Instance().axis = uiaX;
	g_lpTranslateWidget = new TranslateWidget();

	//Create Deformable Model
	g_lpDeformable = new Deformable(strVegFile.cptr(),
									strObjFile.cptr(),
									vFixedVertices);

	g_lpAvatarCube = new AvatarCube(vec3d(-0.1,-0.2,-0.1), vec3d(0.1, 0.2, 0.1));
	//g_lpAvatar->setShaderEffectProgram(g_uiShader);
	//g_lpAvatar->setEffectType(setFixedFunction);

	/*
	g_lpBlobRender = new GPUPoly();
	g_lpBlobRender->readModel(strFPModel.cptr());
	g_lpBlobRender->runTandem(0.1);
	*/

	//Loading Camera
	if(cfg.hasSection("CAMERA") >= 0)
	{
		g_arcBallCam.setRoll(cfg.readFloat("CAMERA", "ROLL"));
		g_arcBallCam.setTilt(cfg.readFloat("CAMERA", "TILT"));
		g_arcBallCam.setZoom(cfg.readFloat("CAMERA", "ZOOM"));
		g_arcBallCam.setCenter(cfg.readVec3f("CAMERA", "CENTER"));
		g_arcBallCam.setOrigin(cfg.readVec3f("CAMERA", "ORIGIN"));
		g_arcBallCam.setPan(cfg.readVec2f("CAMERA", "PAN"));
	}

	//DISPLAY INFO
	g_infoLines.push_back(string("CAMERA"));
	g_infoLines.push_back(string("HAPTIC"));
	g_infoLines.push_back(GetGPUInfo());

}

void SaveSettings()
{
	LogInfo("Saving settings to ini file");
	CSketchConfig cfg(ChangeFileExt(GetExePath(), ".ini"), CSketchConfig::fmReadWrite);
	cfg.writeFloat("CAMERA", "ROLL", g_arcBallCam.getRoll());
	cfg.writeFloat("CAMERA", "TILT", g_arcBallCam.getTilt());
	cfg.writeFloat("CAMERA", "ZOOM", g_arcBallCam.getCurrentZoom());
	cfg.writeVec3f("CAMERA", "CENTER", g_arcBallCam.getCenter());
	cfg.writeVec3f("CAMERA", "ORIGIN", g_arcBallCam.getOrigin());
	cfg.writeVec2f("CAMERA", "PAN", g_arcBallCam.getPan());
}


//Main Loop of Application
int main(int argc, char* argv[])
{
	//VegWriter::WriteVegFile("/home/pourya/Desktop/disc/disc.1.node");

	//Setup the event logger
	PS::TheEventLogger::Instance().setWriteFlags(PS_LOG_WRITE_EVENTTYPE | PS_LOG_WRITE_TIMESTAMP | PS_LOG_WRITE_SOURCE | PS_LOG_WRITE_TO_SCREEN);
	LogInfo("Starting FemMain Application");
	
	//Initialize app
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH | GLUT_STENCIL);
	glutInitWindowSize(WINDOW_WIDTH, WINDOW_HEIGHT);
	glutCreateWindow("Deformable Tissue Modeling - PhD Project - Pourya Shirazian");
	glutDisplayFunc(Draw);
	glutReshapeFunc(Resize);
	glutMouseFunc(MousePress);
	glutMotionFunc(MouseMove);
	glutMouseWheelFunc(MouseWheel);

	glutSpecialFunc(Keyboard);
	glutCloseFunc(Close);
	glutTimerFunc(g_appSettings.millis, TimeStep, 0);

	//Print GPU INFO
	GetGPUInfo();

	//Setup Shading Environment
	static const GLfloat lightColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	static const GLfloat lightPos[4] = { 5.0f, 5.0f, 10.0f, 0.0f };

	//Setup Light0 Position and Color
	glLightfv(GL_LIGHT0, GL_AMBIENT, lightColor);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, lightColor);
	glLightfv(GL_LIGHT0, GL_SPECULAR, lightColor);
	glLightfv(GL_LIGHT0, GL_POSITION, lightPos);

	//Turn on Light 0
	glEnable(GL_LIGHT0);
	//Enable Lighting
	glEnable(GL_LIGHTING);

	//Enable features we want to use from OpenGL
	glShadeModel(GL_SMOOTH);
	glEnable(GL_POLYGON_SMOOTH);
	glEnable(GL_LINE_SMOOTH);

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glEnable(GL_STENCIL_TEST);
	glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

	//glClearColor(0.45f, 0.45f, 0.45f, 1.0f);
	glClearColor(1.0, 1.0, 1.0, 1.0);

	//Compiling shaders
	GLenum err = glewInit();
	if (err != GLEW_OK)
	{
		//Problem: glewInit failed, something is seriously wrong.
		fprintf(stderr, "Error: %s\n", glewGetErrorString(err));
		exit(1);
	}
	CompileShaderCode(g_lpVertexShaderCode, g_lpFragShaderCode, g_uiShader);

	//Load Settings
	LoadSettings();

	//Surface
	/*
    glDisable(GL_TEXTURE_2D);
	g_lpSurface = new GLSurface(WINDOW_WIDTH, WINDOW_HEIGHT);
	g_lpSurface->attach();
    g_lpSurface->testDrawTriangle();
    g_lpSurface->detach();
    */
   // g_lpSurface->saveAsPPM("/home/pourya/Desktop/110.ppm");


	glutPostRedisplay();

	

	//Run App
	glutMainLoop();

	return 0;
}
