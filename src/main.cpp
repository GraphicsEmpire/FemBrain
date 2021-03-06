#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <map>

#include <tbb/task_scheduler_init.h>
#include "base/MathBase.h"
#include "base/Logger.h"
#include "base/Profiler.h"
#include "base/FileDirectory.h"
#include "base/SettingsScript.h"
#include "base/CmdLineParser.h"


#include "graphics/AppScreen.h"
#include "graphics/selectgl.h"
#include "graphics/ShaderManager.h"
#include "graphics/Gizmo.h"
#include "graphics/SceneGraph.h"
#include "graphics/CLMeshBuffer.h"
#include "graphics/Lerping.h"
#include "graphics/MorphingSphere.h"
#include "graphics/SGQuad.h"
#include "graphics/SGSphere.h"
#include "graphics/SGVoxels.h"

#include "implicit/SketchMachine.h"
#include "implicit/OclPolygonizer.h"
#include "implicit/RBF.h"

#include "deformable/Deformable.h"
#include "deformable/AvatarProbe.h"
#include "deformable/AvatarScalpel.h"
#include "deformable/TetGenExporter.h"
#include "deformable/MassSpringSystem.h"
#include "deformable/Cutting.h"
#include "deformable/VolMeshSamples.h"
#include "deformable/Cutting_CPU.h"
#include "deformable/VolMeshSamples.h"
#include "deformable/VolMeshExport.h"
#include "deformable/CollisionDetection.h"
#include "graphics/SGBulletSoftRigidDynamics.h"

#include "graphics/Intersections.h"
#include "volumetricMeshLoader.h"
#include "generateSurfaceMesh.h"
#include "settings.h"

//#include <vexcl/vector.hpp>

using namespace std;
using namespace PS;
using namespace PS::INTERSECTIONS;
using namespace PS::FILESTRINGUTILS;
using namespace PS::CL;
using namespace PS::SG;


//Visual Cues
AvatarProbe* g_lpProbe = NULL;
AvatarScalpel* g_lpScalpel = NULL;
GLMeshBuffer* g_lpDrawNormals = NULL;
CmdLineParser g_cmdLineParser;

//SpringDumble* g_lpMassSpring = NULL;
//Particles* g_lpParticles = NULL;

//SimulationObjects
//PS::CL::FastRBF*	g_lpFastRBF = NULL;
Deformable* 		g_lpDeformable = NULL;
PS::SG::SGBulletSoftRigidDynamics* g_lpWorld = NULL;

//Info Lines
AppSettings g_appSettings;


////////////////////////////////////////////////
//Function Prototype
void Close();
void draw();

void timestep();
void MousePress(int button, int state, int x, int y);
void MouseMove(int x, int y);
void MousePassiveMove(int x, int y);
void MouseWheel(int button, int dir, int x, int y);

void ApplyDeformations(U32 dof, double* displacements);

void NormalKey(unsigned char key, int x, int y);
void SpecialKey(int key, int x, int y);

//Settings
bool LoadSettings(const AnsiStr& strSimFP);
bool SaveSettings(const AnsiStr& strSimFP);
////////////////////////////////////////////////////////////////////////////////////////
void draw()
{
	//Draws Everything in the scenegraph
	TheSceneGraph::Instance().draw();

	//Draw Sketch Machine Stuff
	if(g_appSettings.drawAABB)
		TheSketchMachine::Instance().drawBBox();

	//Draw the Gizmo Manager
	TheGizmoManager::Instance().draw();


	glutSwapBuffers();
}

void MousePress(int button, int state, int x, int y)
{
    TheGizmoManager::Instance().mousePress(button, state, x, y);
    TheSceneGraph::Instance().mousePress(button, state, x, y);

	//Set Values
	g_appSettings.screenDragStart = vec2i(x, y);
	g_appSettings.screenDragEnd = vec2i(x, y);

	if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN)
	{
		vec3f expand(0.2);
		Ray ray = TheSceneGraph::Instance().screenToWorldRay(x, y);
		int idxVertex = g_lpDeformable->getVolMesh()->selectNode(ray);

		//select vertex
		if (g_lpDeformable->getVolMesh()->isNodeIndex(idxVertex)) {
			g_lpDeformable->setPulledVertex(idxVertex);
			LogInfoArg1("Selected Vertex Index = %d ", idxVertex);
		}

		//select fixed node
		if (g_appSettings.selectFixedNodes) {
			//Edit Fixed Vertices
			g_appSettings.vFixedNodes.push_back(idxVertex);
			LogInfoArg1("Added last selection to fixed vertices. count = %d",
					g_appSettings.vFixedNodes.size());
		}
	}

	//Update selection
	glutPostRedisplay();
}


void MousePassiveMove(int x, int y)
{
}

void MouseMove(int x, int y)
{
	TheGizmoManager::Instance().mouseMove(x, y);
	TheSceneGraph::Instance().mouseMove(x, y);

	glutPostRedisplay();
}

void MouseWheel(int button, int dir, int x, int y)
{
	TheSceneGraph::Instance().mouseWheel(button, dir, x, y);
	glutPostRedisplay();
}

void ApplyDeformations(U32 dof, double* displacements) {
	tbb::tick_count tsStart = tbb::tick_count::now();

	if(TheSketchMachine::Instance().polygonizer())
		TheSketchMachine::Instance().polygonizer()->applyFemDisplacements(dof, displacements);

	//Compute MAX Displacement
	/*
	double dx, dy, dz;
	dx = dy = dz = 0.0;
	for(U32 i=0; i < dof; i+=3) {
		dx = MATHMAX(dx, displacements[i]);
		dy = MATHMAX(dy, displacements[i+1]);
		dz = MATHMAX(dz, displacements[i+2]);
	}

	//Transform voxels
	TheSceneGraph::Instance().get("voxels")->transform()->translate(vec3f(dx, dy, dz));
	*/
	//if(g_lpFastRBF)
		//g_lpFastRBF->applyFemDisplacements(dof, displacements);

	g_appSettings.msAnimApplyDisplacements = (tbb::tick_count::now() - tsStart).seconds() * 1000.0;
}

void Close()
{
	//Flush events to db and text log files
	TheDataBaseLogger::Instance().flushAndWait();
	PS::TheEventLogger::Instance().flush();

	//Cleanup
	cout << "Cleanup Memory objects" << endl;

	SAFE_DELETE(g_lpDrawNormals);
	//SAFE_DELETE(g_lpFastRBF);
	SAFE_DELETE(g_lpDeformable);
	SAFE_DELETE(g_lpProbe);
	SAFE_DELETE(g_lpScalpel);

}

void NormalKey(unsigned char key, int x, int y)
{
	switch(key)
	{
	case('+'):{
		g_appSettings.cellsize += 0.01;
		LogInfoArg1("Changed cellsize to: %.2f", g_appSettings.cellsize);
		TheSketchMachine::Instance().polygonizer()->setCellSize(g_appSettings.cellsize);
		TheSketchMachine::Instance().sync();
		break;
	}
	case('-'):{
		g_appSettings.cellsize -= 0.01;
		LogInfoArg1("Changed cellsize to: %.2f", g_appSettings.cellsize);
		TheSketchMachine::Instance().polygonizer()->setCellSize(g_appSettings.cellsize);
		TheSketchMachine::Instance().sync();
		break;
	}

	case('a'): {
		g_appSettings.drawAvatar = !g_appSettings.drawAvatar;
		LogInfoArg1("Draw avatar: %d", g_appSettings.drawAvatar);
	}
	break;

	case('c'): {
		g_appSettings.cutTool = !g_appSettings.cutTool;

		if(g_appSettings.cutTool) {
			LogInfo("Cut tool is on");

			g_lpProbe->setVisible(false);
			g_lpScalpel->setVisible(true);
			TheGizmoManager::Instance().setFocusedNode(g_lpScalpel);
		}
		else {
			LogInfo("Probe tool is on");

			g_lpProbe->setVisible(true);
			g_lpScalpel->setVisible(false);
			TheGizmoManager::Instance().setFocusedNode(g_lpProbe);
		}
	}
	break;

	case('f'): {
		g_appSettings.selectFixedNodes = !g_appSettings.selectFixedNodes;
		if(g_appSettings.selectFixedNodes) {
			g_appSettings.vFixedNodes.resize(0);
			LogInfo("Begin selecting fixed nodes now!");
		}
	}
	break;

	case('g'):{
		TheGizmoManager::Instance().setType(gtTranslate);
		g_appSettings.hapticMode = hmDynamic;
	}
	break;

	case('s'):{
		TheGizmoManager::Instance().setType(gtScale);
		g_appSettings.hapticMode = hmSceneEdit;
		break;
	}

	case('r'):{
		TheGizmoManager::Instance().setType(gtRotate);
		g_appSettings.hapticMode = hmSceneEdit;
		break;
	}

	case('x'):{
		TheGizmoManager::Instance().setAxis(axisX);
	}
	break;
	case('y'):{
		TheGizmoManager::Instance().setAxis(axisY);
	}
	break;
	case('z'):{
		TheGizmoManager::Instance().setAxis(axisZ);
	}
	break;
	case('d'): {
		g_appSettings.drawGround = !g_appSettings.drawGround;
		LogInfoArg1("Draw ground checkerboard set to: %d", g_appSettings.drawGround);
		TheSceneGraph::Instance().get("floor")->setVisible(g_appSettings.drawGround);
	}
	break;
	case('v'): {
		g_appSettings.drawVoxels = !g_appSettings.drawVoxels;
		LogInfoArg1("Draw voxels set to: %d", g_appSettings.drawVoxels);
		TheSceneGraph::Instance().get("voxels")->setVisible(g_appSettings.drawVoxels);
	}
	break;

	case('n'): {
		g_appSettings.drawNormals = !g_appSettings.drawNormals;
		LogInfoArg1("Draw Model Normals set to: %d", g_appSettings.drawNormals);
	}
	break;

	case('p'): {
		LogInfo("print scene graph tree");
		TheSceneGraph::Instance().print();
	}
	break;

	case('['):{
		TheSceneGraph::Instance().camera().incrZoom(0.5f);
	}
	break;
	case(']'):{
		TheSceneGraph::Instance().camera().incrZoom(-0.5f);
	}
	break;

	case(27):
	{
		//Saving Settings and Exit
		LogInfo("Saving settings and exit.");
		SaveSettings(g_appSettings.strSimFilePath);
		glutLeaveMainLoop();
	}
	break;


	}


	//Update Screen
	glutPostRedisplay();
}

void SpecialKey(int key, int x, int y)
{
	switch(key)
	{
		case(GLUT_KEY_F1):
		{
			g_appSettings.drawTetMesh = (g_appSettings.drawTetMesh + 1) % disCount;
			bool wireframe = (g_appSettings.drawTetMesh == disWireFrame);

			Deformable* tissue = dynamic_cast<Deformable*>(TheSceneGraph::Instance().get("tissue"));
			tissue->setVisible(g_appSettings.drawTetMesh);
			tissue->setWireFrameMode(wireframe);
			LogInfoArg1("Draw tetmesh: %d", g_appSettings.drawTetMesh);
		}
		break;

		case(GLUT_KEY_F2):
		{
			g_appSettings.drawIsoSurface = (g_appSettings.drawIsoSurface + 1) % disCount;
			bool wireframe = (g_appSettings.drawIsoSurface == disWireFrame);

			//Set Visibility
			TheSketchMachine::Instance().polygonizer()->setVisible(g_appSettings.drawIsoSurface > 0);
			TheSketchMachine::Instance().polygonizer()->setWireFrameMode(wireframe);
			LogInfoArg1("Draw isosurf mode: %d", g_appSettings.drawIsoSurface);
		}
		break;

		case(GLUT_KEY_F3):
		{
			g_appSettings.drawAABB = !g_appSettings.drawAABB;
			LogInfoArg1("Draw the AABB: %d", g_appSettings.drawAABB);
			break;
		}

		case(GLUT_KEY_F4):
		{
			//Set UIAxis
			int axis = (int)TheGizmoManager::Instance().axis();
			axis = (axis + 1) % axisCount;
			TheGizmoManager::Instance().setAxis((GizmoAxis)axis);
			LogInfoArg1("Change haptic axis to %d", TheGizmoManager::Instance().axis());
			break;
		}

		case(GLUT_KEY_F5):
		{
			GizmoAxis axis = TheGizmoManager::Instance().axis();
			vec3f inc(0,0,0);
			if(axis < axisFree)
				inc.setElement(axis, -0.1);
			else
				inc = vec3f(-0.1, -0.1, -0.1);
			inc = inc * 0.5;
			TheGizmoManager::Instance().transform()->scale(inc);
			break;
		}


		case(GLUT_KEY_F6):
		{
			GizmoAxis axis = TheGizmoManager::Instance().axis();
			vec3f inc(0,0,0);
			if(axis < axisFree)
				inc.setElement(axis, 0.1);
			else
				inc = vec3f(0.1, 0.1, 0.1);
			inc = inc * 0.5;
			TheGizmoManager::Instance().transform()->scale(inc);
			break;
		}


		case(GLUT_KEY_F7):
		{
			int radius = MATHMAX(g_lpDeformable->getHapticForceRadius() - 1, 1);
			g_lpDeformable->setHapticForceRadius(radius);
			LogInfoArg1("Decrease haptic force radius: %d", radius);
			break;
		}

		case(GLUT_KEY_F8):
		{
			int radius = MATHMIN(g_lpDeformable->getHapticForceRadius() + 1, 10);
			g_lpDeformable->setHapticForceRadius(radius);
			LogInfoArg1("Increase haptic force radius: %d", radius);
			break;
		}

		case(GLUT_KEY_F9):
		{
			g_appSettings.drawAffineWidgets = !g_appSettings.drawAffineWidgets;
			TheGizmoManager::Instance().setVisible(g_appSettings.drawAffineWidgets);
			LogInfoArg1("Draw affine widgets: %d", g_appSettings.drawAffineWidgets);
			break;
		}

		case(GLUT_KEY_F10):
		{
			g_lpDeformable->resetDeformations();
			break;
		}

		case(GLUT_KEY_F11):
		{
			g_lpProbe->setPickMode(!g_lpProbe->getPickMode());
			LogInfoArg1("Pick mode for scalpel is %d", g_lpProbe->getPickMode());
			break;
		}

	}

	//Modifier
	TheSceneGraph::Instance().setModifier(glutGetModifiers());

	glutPostRedisplay();
}


void timestep()
{
	//Advance timestep in scenegraph
	TheSceneGraph::Instance().timestep();

	/*
	g_appSettings.ctFrames++;

	//Sample For Logs and Info
	if(g_appSettings.ctFrames - g_appSettings.ctAnimLogger > ANIMATION_TIME_SAMPLE_INTERVAL) {

		//Store counter
		g_appSettings.ctAnimLogger = g_appSettings.ctFrames;

		//Log Database
		if(g_appSettings.logSql)
		{
			if(g_lpDeformable->isVolumeChanged())
			{
				DBLogger::Record rec;
				g_lpDeformable->statFillRecord(rec);

				//StatInfo
				rec.ctSolverThreads = g_appSettings.ctSolverThreads;
				rec.animFPS = g_appSettings.fps;
				rec.cellsize = g_appSettings.cellsize;
				rec.msAnimTotalFrame = g_appSettings.msFrameTime;
				rec.msAnimSysSolver  = g_lpDeformable->getSolverTime() * 1000.0;
				rec.msAnimApplyDisplacements = g_appSettings.msAnimApplyDisplacements;
				rec.msPolyTriangleMesh 		 = g_appSettings.msPolyTriangleMesh;
				rec.msPolyTetrahedraMesh 	 = g_appSettings.msPolyTetrahedraMesh;
				rec.msRBFCreation 			 = g_appSettings.msRBFCreation;
				rec.msRBFEvaluation = 0.0;
				TheDataBaseLogger::Instance().append(rec);

				g_appSettings.ctLogsCollected ++;
			}
		}
	}
	*/

	//Animation Frame Prepared now display
	glutPostRedisplay();
}

bool LoadSettings(const AnsiStr& strSimFP)
{
	if(!PS::FILESTRINGUTILS::FileExists(strSimFP)) {
		LogErrorArg1("SIM file not found at: %s", strSimFP.c_str());
		return false;
	}

	//Loading Settings
	LogInfo("Loading Settings from the ini file.");
	SettingsScript cfg(strSimFP, SettingsScript::fmRead);

	//READ MODEL FILE
	bool isRelativePath = cfg.readBool("MODEL", "RELATIVEPATH", true);
	AnsiStr strSimFileDir = PS::FILESTRINGUTILS::ExtractFilePath(strSimFP);
	if(isRelativePath)
		g_appSettings.strModelFilePath = strSimFileDir + cfg.readString("MODEL", "INPUTFILE");
	else
		g_appSettings.strModelFilePath = cfg.readString("MODEL", "INPUTFILE");


	int ctFixed = 	cfg.readInt("MODEL", "FIXEDVERTICESCOUNT", 0);
	if(ctFixed > 0) {
		if(!cfg.readIntArray("MODEL", "FIXEDVERTICES", ctFixed, g_appSettings.vFixedNodes))
			LogError("Unable to read specified number of fixed vertices!");
	}

	//System settings
	g_appSettings.groundLevel = cfg.readInt("SYSTEM", "GROUNDLEVEL", 0.0f);
	g_appSettings.hapticForceCoeff = cfg.readInt("SYSTEM", "FORCECOEFF", DEFAULT_FORCE_COEFF);
	g_appSettings.logSql = cfg.readBool("SYSTEM", "LOGSQL", g_appSettings.logSql);
	g_appSettings.cellsize = cfg.readFloat("SYSTEM", "CELLSIZE", DEFAULT_CELL_SIZE);
	g_appSettings.hapticNeighborhoodPropagationRadius = cfg.readInt("AVATAR", "RADIUS");
	g_appSettings.gravity = cfg.readBool("SYSTEM", "GRAVITY", true);

	//Avatar
	g_appSettings.avatarPos = cfg.readVec3f("AVATAR", "POS");
	g_appSettings.avatarThickness = cfg.readVec3f("AVATAR", "THICKNESS");
	g_appSettings.avatarAxis = cfg.readInt("AVATAR", "AXIS");


	//Loading Camera
	if(cfg.hasSection("CAMERA") >= 0)
	{
		ArcBallCamera& cam = TheSceneGraph::Instance().camera();
		cam.setRoll(cfg.readFloat("CAMERA", "ROLL"));
		cam.setTilt(cfg.readFloat("CAMERA", "TILT"));
		cam.setZoom(cfg.readFloat("CAMERA", "ZOOM"));
		cam.setCenter(cfg.readVec3f("CAMERA", "CENTER"));
		cam.setOrigin(cfg.readVec3f("CAMERA", "ORIGIN"));
		cam.setPan(cfg.readVec2f("CAMERA", "PAN"));
	}

	//DISPLAY SETTINGS
	g_appSettings.drawAABB = cfg.readBool("DISPLAY", "AABB", true);
	g_appSettings.drawAffineWidgets = cfg.readBool("DISPLAY", "AFFINEWIDGETS", true);
	g_appSettings.drawGround = cfg.readBool("DISPLAY", "GROUND", true);
	g_appSettings.drawNormals = cfg.readBool("DISPLAY", "NORMALS", true);
	g_appSettings.drawVoxels = cfg.readBool("DISPLAY", "VOXELS", true);
	g_appSettings.drawTetMesh = cfg.readInt("DISPLAY", "TETMESH", disFull);
	g_appSettings.drawIsoSurface = cfg.readInt("DISPLAY", "ISOSURF", disFull);

	return true;
}

bool SaveSettings(const AnsiStr& strSimFP)
{
	LogInfo("Saving settings to ini file");
	SettingsScript cfg(strSimFP, SettingsScript::fmReadWrite);

	ArcBallCamera& cam = TheSceneGraph::Instance().camera();
	cfg.writeFloat("CAMERA", "ROLL", cam.getRoll());
	cfg.writeFloat("CAMERA", "TILT", cam.getTilt());
	cfg.writeFloat("CAMERA", "ZOOM", cam.getZoom());
	cfg.writeVec3f("CAMERA", "CENTER", cam.getCenter());
	cfg.writeVec3f("CAMERA", "ORIGIN", cam.getOrigin());
	cfg.writeVec2f("CAMERA", "PAN", cam.getPan());

	//Write Cellsize
	cfg.writeFloat("SYSTEM", "CELLSIZE", g_appSettings.cellsize);
	cfg.writeBool("SYSTEM", "GRAVITY", g_appSettings.drawGround);

	//Avatar
	cfg.writeVec3f("AVATAR", "POS", g_lpProbe->transform()->getTranslate());
	cfg.writeVec3f("AVATAR", "THICKNESS", g_lpProbe->transform()->getScale());
	cfg.writeInt("AVATAR", "AXIS", TheGizmoManager::Instance().axis());

	//DISPLAY SETTINGS
	cfg.writeBool("DISPLAY", "AABB", g_appSettings.drawAABB);
	cfg.writeBool("DISPLAY", "AFFINEWIDGETS", g_appSettings.drawAffineWidgets);
	cfg.writeBool("DISPLAY", "GROUND", g_appSettings.drawGround);
	cfg.writeBool("DISPLAY", "NORMALS", g_appSettings.drawNormals);
	cfg.writeBool("DISPLAY", "VOXELS", g_appSettings.drawVoxels);
	cfg.writeInt("DISPLAY", "TETMESH", g_appSettings.drawTetMesh);
	cfg.writeInt("DISPLAY", "ISOSURF", g_appSettings.drawIsoSurface);


	//MODEL PROPS
	if(g_appSettings.selectFixedNodes) {
		cfg.writeInt("MODEL", "FIXEDVERTICESCOUNT", g_appSettings.vFixedNodes.size());
		cfg.writeIntArray("MODEL", "FIXEDVERTICES", g_appSettings.vFixedNodes);
	}

	return true;
}


void cutCompleted() {
	LogInfo("Cut completed. Sync meshes");
	g_lpDeformable->syncForceModel();
}

//Main Loop of Application
int main(int argc, char* argv[])
{
	//Init Task Schedular
	tbb::task_scheduler_init init(task_scheduler_init::default_num_threads());
	//tbb::task_scheduler_init init(1);

	printf("***************************************************************************************\n");
	printf("FEMBrain - GPU-Accelerated Animation of Implicit Surfaces using Finite Element Methods.\n");
	printf("[Pourya Shirazian] Email: pouryash@cs.uvic.ca\n");


	//SIM files define the simulation parameters
	//Parse Input Args
	g_cmdLineParser.add_option("file", "[.sim file] Specify simulation file path.", Value(AnsiStr("../data/models/tumor.sim")));
	g_cmdLineParser.add_option("threads", "number of threads", Value((int)tbb::task_scheduler_init::default_num_threads()));
	if(g_cmdLineParser.parse(argc, argv) < 0)
		exit(0);

	g_appSettings.ctSolverThreads = g_cmdLineParser.value<int>("threads");
	g_appSettings.strSimFilePath = ExtractFilePath(GetExePath()) + g_cmdLineParser.value<AnsiStr>("file");


	//Setup the event logger
	//PS::TheEventLogger::Instance().setWriteFlags(PS_LOG_WRITE_EVENTTYPE | PS_LOG_WRITE_TIMESTAMP | PS_LOG_WRITE_SOURCE | PS_LOG_WRITE_TO_SCREEN);
	PS::TheEventLogger::Instance().setWriteFlags(PS_LOG_WRITE_EVENTTYPE | PS_LOG_WRITE_SOURCE | PS_LOG_WRITE_TO_SCREEN);
	PS::TheProfiler::Instance().setWriteFlags(Profiler::pbInjectToLogger);
	//TheProfiler::Instance().setInjectToLogFlag(false);
	LogInfo("Starting FemBrain");

	//Initialize app
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH | GLUT_STENCIL);
	glutInitWindowSize(WINDOW_WIDTH, WINDOW_HEIGHT);
	glutCreateWindow("Deformable Tissue Modeling - PhD Project - Pourya Shirazian");
	glutDisplayFunc(draw);
	glutReshapeFunc(def_resize);
	glutMouseFunc(MousePress);
	glutPassiveMotionFunc(MousePassiveMove);
	glutMotionFunc(MouseMove);
	glutMouseWheelFunc(MouseWheel);
	glutKeyboardFunc(NormalKey);
	glutSpecialFunc(SpecialKey);
	glutCloseFunc(Close);
	glutIdleFunc(timestep);

	//initialize gl
	def_initgl();

	//Build Shaders for drawing the mesh
	AnsiStr strRoot = ExtractOneLevelUp(ExtractFilePath(GetExePath()));
	AnsiStr strShaderRoot = strRoot + "data/shaders/";
	AnsiStr strMeshesRoot = strRoot + "data/models/mesh/";
	AnsiStr strTexRoot = strRoot + "data/textures/";

	//Load Shaders
	TheShaderManager::Instance().addFromFolder(strShaderRoot.cptr());

	//Load Settings
	if(!LoadSettings(g_appSettings.strSimFilePath))
		exit(0);

	//Scenebox
	TheSceneGraph::Instance().addSceneBox(AABB(vec3f(-10,-10,-16), vec3f(10,10,16)));

	//create bullet world
	g_lpWorld = new SGBulletSoftRigidDynamics();
	TheSceneGraph::Instance().add(g_lpWorld);

	//floor
	{
		btCollisionShape* shape = new btStaticPlaneShape(btVector3(0, 1, 0), 1);
		shape->setMargin(0.04);

		btDefaultMotionState* motionState = new btDefaultMotionState(btTransform(btQuaternion(0,0,0,1), btVector3(0,0,0)));
		btRigidBody::btRigidBodyConstructionInfo rigidBodyCI(
			0,                  // mass
			motionState,        // initial position
			shape,              // collision shape of body
			btVector3(0,0,0)    // local inertia
		);

		btRigidBody *rigidBody = new btRigidBody(rigidBodyCI);

		Geometry g;
		g.addCube(vec3f(-8, -0.5, -8), vec3f(8, 0.0, 8));
		g.addPerVertexColor(vec4f(0.5, 0.5, 0.5, 1.0));
		SGBulletRigidMesh* floor = new SGBulletRigidMesh();
		floor->setName("floor");
		floor->setup(g, rigidBody);

		TheSceneGraph::Instance().add(floor);
		g_lpWorld->addRigidBody(floor);
	}




	//Textured Ground
	/*
	TheTexManager::Instance().add(strTexRoot + "wood.png");
	SGQuad* woodenFloor = new SGQuad(16.0f, 16.0f, TheTexManager::Instance().get("wood"));
	woodenFloor->setName("floor");
	woodenFloor->transform()->translate(vec3f(0, -1.0f, 0));
	woodenFloor->transform()->rotate(vec3f(1.0f, 0.0f, 0.0f), 90.0f);
	TheSceneGraph::Instance().add(woodenFloor);


	Geometry g;
	g.addCube(vec3f(-8, 0.0, -8), vec3f(8, 0.01, 8));
	g.addPerVertexColor(vec4f(0.5,0.5,0.5,1));
	SGBulletRigidMesh* floor = new SGBulletRigidMesh(g, 0.0);
	floor->setName("floor");
	TheSceneGraph::Instance().add(floor);
	g_lpWorld->addRigidBody(floor);
*/

	//Light source
	vec4f lightPos;
	glGetLightfv(GL_LIGHT0, GL_POSITION, lightPos.ptr());
	SGSphere* s = new SGSphere(0.3f, 8, 8);
	s->setName("light0");
	s->transform()->translate(vec3f(&lightPos[0]));
	TheSceneGraph::Instance().add(s);


	//Check the model file extension
	AnsiStr strFileExt = ExtractFileExt(AnsiStr(g_appSettings.strModelFilePath.c_str()));

	//Mesh
	U32 ctVertices = 0;
	U32 ctTriangles = 0;
	vector<float> vertices;
	vector<U32> elements;

	if(strFileExt == AnsiStr("obj")) {
		LogInfoArg1("Loading input mesh from obj file at: %s", g_appSettings.strModelFilePath.c_str());
		Mesh* lpMesh = new Mesh(g_appSettings.strModelFilePath.c_str());
		AABB aabb = lpMesh->computeBoundingBox();
		//Move to origin
		lpMesh->move(aabb.center() * -1);

		//No triangle polygonization time
		g_appSettings.msPolyTriangleMesh = 0;

		tbb::tick_count tsStart = tbb::tick_count::now();
		//g_lpFastRBF = new FastRBF(lpMesh);
		g_appSettings.msRBFCreation = (tbb::tick_count::now() - tsStart).seconds() * 1000.0;

		//g_lpDrawNormals = g_lpFastRBF->prepareMeshBufferNormals();

		//Readback mesh
		MeshNode* lpNode = lpMesh->getNode(0);
		if(lpNode != NULL)
			lpNode->readbackMeshV3T3(ctVertices, vertices, ctTriangles, elements);


		SAFE_DELETE(lpMesh);
	}
	else
	{
		LogInfoArg1("Loading input BlobTree file at: %s", g_appSettings.strModelFilePath.c_str());

		if(!TheSketchMachine::Instance().loadBlob(g_appSettings.strModelFilePath)) {
			LogErrorArg1("Unable to read Blob file at: %s", g_appSettings.strModelFilePath.c_str());
			exit(-1);
		}

		LogInfoArg1("Polygonizing BlobTree file with cellsize: %.2f", g_appSettings.cellsize);
		TheSketchMachine::Instance().polygonizer()->setCellSize(g_appSettings.cellsize);
		TheSketchMachine::Instance().polygonizer()->setVisible(g_appSettings.drawIsoSurface);
		TheSketchMachine::Instance().polygonizer()->setWireFrameMode(g_appSettings.drawIsoSurface == disWireFrame);
		TheSketchMachine::Instance().sync();

		//Show voxels
		SGVoxels* voxels = new SGVoxels(TheSketchMachine::Instance().polygonizer()->surfaceVoxels(), g_appSettings.cellsize);
		voxels->setName("voxels");
		voxels->setWireFrameMode(true);
		voxels->setVisible(g_appSettings.drawVoxels);
		TheSceneGraph::Instance().add(voxels);

		//Create the RBF representation
		//g_lpFastRBF = new FastRBF(lpBlobRender);
		//g_lpFastRBF->setShaderEffectProgram(g_uiPhongShader);
		//g_lpDrawNormals = g_lpFastRBF->prepareMeshBufferNormals();

		//Read Back Polygonized Mesh
		if(!TheSketchMachine::Instance().polygonizer()->readbackMeshV3T3(ctVertices, vertices, ctTriangles, elements))
			LogError("Unable to read mesh from RBF in the format of V3T3");
	}


	{
		//TetMesh
		vector<U32> tetElements;
		vector<double> tetVertices;

		//Produce Quality Tetrahedral Mesh using TetGen for now
		tbb::tick_count tsStart = tbb::tick_count::now();

		TetGenExporter::tesselate(vertices, elements, tetVertices, tetElements);
		g_appSettings.msPolyTetrahedraMesh = (tbb::tick_count::now() - tsStart).seconds() * 1000.0;

		{
			AnsiStr strVegFilePath = FILESTRINGUTILS::ChangeFileExt(g_appSettings.strModelFilePath, ".veg");
			VolMesh* vmesh = new VolMesh(tetVertices, tetElements);
			bool bres = VolMeshIO::writeVega(vmesh, strVegFilePath);
			SAFE_DELETE(vmesh);

			if(bres)
				LogInfoArg1("Saved veg file to: %s", strVegFilePath.cptr());
		}

		//Deformable
		VolMesh* tempMesh = PS::MESH::VolMeshSamples::CreateTwoTetra();
		vector<int> fixed;
		g_lpDeformable = new Deformable(*tempMesh, fixed);
		g_lpDeformable->setGravity(true);
		g_lpDeformable->getVolMesh()->setFlagDetectCutNodes(false);
		g_lpDeformable->setHapticForceRadius(g_appSettings.hapticNeighborhoodPropagationRadius);
		g_lpDeformable->setName("tissue");
		g_lpDeformable->setCollisionObject(TheSceneGraph::Instance().get("floor"));
		g_lpDeformable->setVisible(g_appSettings.drawTetMesh);
		g_lpDeformable->setWireFrameMode(g_appSettings.drawTetMesh == disWireFrame);
		TheSceneGraph::Instance().add(g_lpDeformable);
		SAFE_DELETE(tempMesh);


		//Probe
		g_lpProbe = new AvatarProbe(g_lpDeformable);
		g_lpProbe->transform()->translate(g_appSettings.avatarPos);
		g_lpProbe->transform()->scale(g_appSettings.avatarThickness);
		g_lpProbe->setVisible(false);
		TheSceneGraph::Instance().add(g_lpProbe);


		//scalpel
		g_lpScalpel = new AvatarScalpel(g_lpDeformable->getVolMesh());
		g_lpScalpel->transform()->translate(g_appSettings.avatarPos);
		g_lpScalpel->setOnCutEventHandler(cutCompleted);
		TheSceneGraph::Instance().add(g_lpScalpel);


		//create rigid bodies
		{
			Geometry g1;
			g1.addCube(vec3f(-2.0, 5.0, 2.0), 1);
			g1.addPerVertexColor(vec4f(0,0,1,1));
			SGBulletRigidMesh* acube = new SGBulletRigidMesh(g1, 1.0);
			TheSceneGraph::Instance().add(acube);
			g_lpWorld->addRigidBody(acube);
		}


		//set focused node for affine gizmo
		TheGizmoManager::Instance().setFocusedNode(g_lpScalpel);
		TheGizmoManager::Instance().setAxis((GizmoAxis)g_appSettings.avatarAxis);

		//log report
		char chrMsg[1024];

		GPUPoly* poly = TheSketchMachine::Instance().polygonizer();
		sprintf(chrMsg, "MESH CELLSIZE:%.3f, ISOSURF: V# %d, TRI# %d, VOLUME: V# %d, TET# %d",
					g_appSettings.cellsize,
					poly->countVertices(),
					poly->countTriangles(),
					g_lpDeformable->getVolMesh()->countNodes(),
					g_lpDeformable->getVolMesh()->countCells());
		TheSceneGraph::Instance().headers()->addHeaderLine("mesh", chrMsg);
	}

	//Draw the first time
	glutPostRedisplay();

	//Run App
	glutMainLoop();

	return 0;
}
