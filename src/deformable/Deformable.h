/*
 * PS_Deformable.h
 *
 *  Created on: Sep 29, 2012
 *      Author: pourya
 */

#ifndef PS_DEFORMABLE_H_
#define PS_DEFORMABLE_H_

#include <vector>
#include "loki/Functor.h"
#include "base/String.h"
#include "base/Vec.h"


#include "graphics/SGMesh.h"
#include "graphics/AABB.h"


#include "SurfaceMesh.h"

#include "corotationalLinearFEM.h"
#include "corotationalLinearFEMForceModel.h"
#include "generateMassMatrix.h"
#include "PS_VolumeConservingIntegrator.h"
#include "OclVolConservedIntegrator.h"
#include "sceneObjectDeformable.h"
#include "graph.h"
#include "DBLogger.h"
#include "CuttableMesh.h"
#include "SGBulletCDShape.h"


using namespace std;
using namespace Loki;
using namespace PS::MATH;
using namespace PS::FEM;
using namespace PS::SG;

#define DEFAULT_FORCE_NEIGHBORHOOD_SIZE 5
#define DEFAULT_THREAD_COUNT 8


//ApplyDeformations
typedef void (*FOnApplyDeformations)(U32 dof, double* displacements);
//typedef Functor<void,  U32, double*> FOnDeformations;

struct FaceIntersection {
	int face;
	vec3d xyz;
	vec3d uvw;
};

struct EdgeIntersection {
	int edge;
	vec3d xyz;
};

/*!
 *	Deformable model
 */
class Deformable : public SGMesh {
public:
	Deformable();

	explicit Deformable(const VolMesh& mesh);

	explicit Deformable(const VolMesh& mesh,
		 	 	 	 	const vector<int>& vFixedVertices);

	virtual ~Deformable();


	//Draw
	void draw();

	//Cutting
	void cleanupCuttingStructures();
	//int  performCuts(const vec3d& s0, const vec3d& s1);

	//TimeStep
	void timestep();

	//Pick a vertex
	int pickVertex(const vec3d& wpos, vec3d& vertex);

	int pickVertices(const vec3d& boxLo, const vec3d& boxHi,
					   vector<vec3d>& arrFoundCoords, vector<int>& arrFoundIndices) const;

	//Fill Record for
	void statFillRecord(DBLogger::Record& rec) const;

	//Haptics
	void setPulledVertex(int index);
	bool hapticStart(int index);
	bool hapticStart(const vec3d& wpos);
	void hapticEnd();
	void hapticSetCurrentForces(const vector<int>& indices,
									const vector<vec3d>& forces);

	bool isHapticInProgress() const {return m_bHapticInProgress;}
	int getHapticForceRadius() const {return m_hapticForceNeighorhoodSize;}
	void setHapticForceRadius(int radius) { m_hapticForceNeighorhoodSize = radius;}


	//Render
	bool renderVertices() const {return m_bRenderVertices;}
	void setRenderVertices(bool bRender) { m_bRenderVertices = bRender;}

	//Damping stiffness
	void setDampingStiffnessCoeff(double s);
	double getDampingStiffnessCoeff() const {return m_dampingStiffnessCoeff;}

	//Damping Mass
	void setDampingMassCoeff(double m);
	double getDampingMassCoeff() const {return m_dampingMassCoeff;}

	//Fixed dofs
	bool addFixedVertex(int index);
	bool removeFixedVertex(int index);
	bool setFixedVertices(const std::vector<int>& vFixedVertices);
	int  getFixedVertices(vector<int>& vFixedVertices);
	bool updateFixedVertices();

	//Volume
	double computeVolume(double* arrStore = NULL, U32 count = 0) const;
	bool isVolumeChanged() const { return  !EssentiallyEquald(this->computeVolume(), m_restVolume, 0.0001);}

	//Access TetMesh for stats
	CuttableMesh* getVolMesh() const { return m_lpVolMesh;}

	//ModelName
	string getModelName() const {return m_strModelName;}
	void setModelName(const string& strModelName) {m_strModelName = strModelName;}

	//Gravity
	void setGravity(bool bGravity) {m_bApplyGravity = bGravity;}
	bool getGravity() const {return m_bApplyGravity;}
	bool applyHapticForces();

	//Collision object
	void setCollisionObject(SGNode* obj) {m_collisionObj = obj;}
	bool collisionDetect();
	void resetDeformations();

	//Set callbacks
	void setDeformCallback(FOnApplyDeformations fOnDeform) {
		m_fOnDeform = fOnDeform;
	}

	double getSolverTime() const { return m_lpIntegrator->GetSystemSolveTime();}


	//Force model sync
	bool syncForceModel();

	/*!
	 * Return: Outputs number of dofs
	 */
	static int FixedVerticesToFixedDOF(std::vector<int>& arrInFixedVertices,
										    std::vector<int>& arrOutFixedDOF);
private:
	/*!
	 * Setup procedure builds deformable model with the specified params
	 */
	void setup(const char* lpVegFilePath,
			    const char* lpObjFilePath,
			    std::vector<int>& vFixedVertices,
			    int ctThreads = 0,
			    const char* lpModelTitle = NULL);

	void init();
	void cleanup();

private:
	//Callbacks
	FOnApplyDeformations m_fOnDeform;

	double m_dampingStiffnessCoeff;
	double m_dampingMassCoeff;
	double m_timeStep;
	double m_restVolume;

	//Modifier
	CuttableMesh* m_lpVolMesh;
	TetMesh* m_lpForceModelTetMesh;

	//Deformable Model
	CorotationalLinearFEM* m_lpDeformable;

	//ForceModel
	ForceModel* m_lpDeformableForceModel;

	//Integrator
	ImplicitNewmarkSparse* m_lpIntegrator;


	//Mass Matrix
	SparseMatrix* m_lpMassMatrix;

	//Time Step Counter
	U32 m_ctTimeStep;

	U32 m_dof;
	double* m_q;
	double* m_qVel;
	double* m_qAcc;
	double* m_arrExtForces;


	//Fixed Vertices
	vector<int> m_vFixedVertices;
	vector<int> m_vFixedDofs;

	//Haptic forces
	bool m_bHapticInProgress;
	int m_hapticForceNeighorhoodSize;
	double m_hapticCompliance;

	//vector<vec3d> m_vHaptic
	vector<vec3d> m_vHapticForces;
	vector<int> m_vHapticIndices;
	int m_idxPulledVertex;
	int m_positiveDefiniteSolver;

	bool m_bRenderFixedVertices;
	bool m_bRenderVertices;
	bool m_bApplyGravity;
	U32 m_ctCollided;

	string m_strModelName;

	SGNode* m_collisionObj;
};

#endif /* PS_DEFORMABLE_H_ */
