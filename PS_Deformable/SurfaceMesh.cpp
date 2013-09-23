/*
 * PS_SurfaceMesh.cpp
 *
 *  Created on: Dec 15, 2012
 *      Author: pourya
 */
#include "SurfaceMesh.h"
#include <fstream>
#include "PS_Graphics/PS_Mesh.h"
#include "PS_Base/PS_Logger.h"
#include "GL/glew.h"

using namespace PS;

namespace PS {
namespace FEM {

SurfaceMesh::SurfaceMesh() {
	m_drawMode = dtFaces | dtEdges | dtVertices | dtFixedVertices | dtPickedVertices;
	m_lpMemFaces = m_lpMemNormal = m_lpMemVertices = NULL;
}

SurfaceMesh::SurfaceMesh(const char* chrObjFilePath) {
	m_drawMode = dtFaces | dtEdges | dtVertices | dtFixedVertices | dtPickedVertices;
	m_lpMemFaces = m_lpMemNormal = m_lpMemVertices = NULL;
	assert(readFromDisk(chrObjFilePath));
	computeAABB();
	setupDrawBuffers();
}

SurfaceMesh::~SurfaceMesh() {

	cleanupDrawBuffers();
	m_vCurPos.resize(0);
	m_vRestPos.resize(0);
	m_vNormals.resize(0);
	m_faces.resize(0);
}

void SurfaceMesh::cleanupDrawBuffers() {
	SAFE_DELETE(m_lpMemNormal);
	SAFE_DELETE(m_lpMemVertices);
	SAFE_DELETE(m_lpMemFaces);
}

void SurfaceMesh::setupDrawBuffers() {

	cleanupDrawBuffers();

	U32 ctFaces = getFaceCount();
	U32 ctVertices = getVertexCount();
	bool bHasNormals = (m_vNormals.size() == m_vCurPos.size());

	U32 szTotal = m_vCurPos.size() * sizeof(double);
	m_lpMemVertices = new GLMemoryBuffer(mbtPosition, GL_DYNAMIC_DRAW, 3, GL_DOUBLE, szTotal, &m_vCurPos[0]);
	if(bHasNormals)
		m_lpMemNormal = new GLMemoryBuffer(mbtNormal, GL_DYNAMIC_DRAW, 3, GL_DOUBLE, szTotal, &m_vNormals[0]);
	m_lpMemFaces = new GLMemoryBuffer(mbtFaceIndices, GL_DYNAMIC_DRAW, 3, GL_UNSIGNED_INT, m_faces.size() * sizeof(U32),  &m_faces[0]);
}

void SurfaceMesh::draw() {
	//Draw Faces
	if((m_drawMode & dtFaces) != 0) {
	//	glCallList(m_glListFaces);
		glPushAttrib(GL_ALL_ATTRIB_BITS);
		glColor4f(0.8, 0.8, 0.8, 1.0);
		m_lpMemVertices->attach();
		m_lpMemNormal->attach();

		m_lpMemFaces->attach();
		m_lpMemFaces->drawElements(GL_TRIANGLES, m_faces.size());
		m_lpMemFaces->detach();

		m_lpMemNormal->detach();
		m_lpMemVertices->detach();
		glPopAttrib();
	}

	if((m_drawMode & dtEdges) != 0) {
		glPushAttrib(GL_ALL_ATTRIB_BITS);

		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		glLineWidth(3.0f);
		glColor4f(0.0, 0.0, 0.0, 1.0);
		m_lpMemVertices->attach();
		m_lpMemNormal->attach();

		m_lpMemFaces->attach();
		m_lpMemFaces->drawElements(GL_TRIANGLES, m_faces.size());
		m_lpMemFaces->detach();

		m_lpMemNormal->detach();
		m_lpMemVertices->detach();
		glPopAttrib();
	}

	if((m_drawMode & dtVertices) != 0) {
		glPushAttrib(GL_ALL_ATTRIB_BITS);
		glPointSize(5.0f);
		glColor4f(0.0, 1.0, 0.0, 1.0);
		m_lpMemVertices->attach();

		glDrawArrays(GL_POINTS, 0, m_vCurPos.size() / 3);

		m_lpMemVertices->detach();
		glPopAttrib();
	}
}

void SurfaceMesh::setDrawMode(int mode) {
	m_drawMode = mode;
}

int SurfaceMesh::getDrawMode() const {
	return m_drawMode;
}


//Count
U32 SurfaceMesh::getVertexCount() const {
	return (m_vCurPos.size() / 3);
}
U32 SurfaceMesh::getFaceCount() const {
	return (m_faces.size() / 3);
}

//Access
vec3d SurfaceMesh::vertexAt(U32 idx) const {
	return vec3d(&m_vCurPos[idx * 3]);
}

vec3d SurfaceMesh::vertexRestPosAt(U32 idx) const {
	return vec3d(&m_vRestPos[idx * 3]);
}

vec3d SurfaceMesh::normalAt(U32 idx) const {
	return vec3d(&m_vNormals[idx * 3]);
}

vec3u32 SurfaceMesh::faceAt(U32 idx) const {
	return vec3u32(&m_faces[idx * 3]);
}

vec3d SurfaceMesh::faceVertexAt(U32 idxFace, U8 idxWhichCorner) const {
	vec3u32 face = faceAt(idxFace);
	return vertexAt(face.element(idxWhichCorner));
}

//Processes
bool SurfaceMesh::computeAABB() {
	if(getVertexCount() == 0 || getFaceCount() == 0)
		return false;

	U32 ctVertices = getVertexCount();
	vec3d lo = vec3d(&m_vCurPos[0]);
	vec3d hi = lo;

	for(U32 i=1; i<ctVertices; i++) {
		lo = vec3d::minP(lo, vec3d(&m_vCurPos[i * 3]));
		hi = vec3d::maxP(lo, vec3d(&m_vCurPos[i * 3]));
	}

	//Set the bounding box
	m_bbox.set(vec3f(lo.x, lo.y, lo.z), vec3f(hi.x, hi.y, hi.z));

	return true;
}

void SurfaceMesh::resetToRest() {
	//Copy all from rest pos
	m_vCurPos.assign(m_vRestPos.begin(), m_vRestPos.end());
}

void SurfaceMesh::applyDisplacements(double * u) {
	for(U32 i=0; i<m_vCurPos.size(); i++)
		m_vCurPos[i] = m_vRestPos[i] + u[i];

	//Modify vertex buffer
	m_lpMemVertices->modify(0, m_vCurPos.size() * sizeof(double), &m_vCurPos[0]);
}

int SurfaceMesh::findClosestVertex(const vec3d& query, double& dist, vec3d& outP) {
	U32 ctVertices = getVertexCount();
	double minDist = GetMaxLimit<double>();
	int idxFound = -1;
	for(U32 i=0; i<ctVertices; i++) {
		vec3d p = vec3d(&m_vCurPos[i * 3]);
		double dist2 = (query - p).length2();
		if(dist2 < minDist) {
			minDist = dist2;
			outP = p;
			idxFound = i;
		}
	}

	//Distance
	dist = sqrt(minDist);
	return idxFound;
}

//Topology Modification
// Removing vertices will modify triangle indices as well
bool SurfaceMesh::removeVertices(const vector<U32>& vertices) {

	int ctRemovedTriangles = 0;
	//1. Remove all triangles around the vertices
	for(U32 i=0; i<vertices.size(); i++) {
		vector<U32> triangles;
		//First get triangles around vertex
		trianglesAroundVertex(vertices[i], triangles);
		removeTriangles(triangles);

		ctRemovedTriangles += triangles.size();
	}

	//2. Compact and Remove vertices

	return false;
}

bool SurfaceMesh::removeTriangles(const vector<U32>& triangles) {
	if(triangles.size() == 0)
		return false;

	for(U32 i=0; i<triangles.size(); i++) {
		U32 index = triangles[i];
		m_faces.erase(m_faces.begin() + index * 3, m_faces.begin() + index * 3 + 3);
	}

	return true;
}

//Add new vertices to the vertex list
bool SurfaceMesh::addVertices(const vector<double>& vertices, const vector<double>& normals) {
	if((vertices.size() == 0) || (vertices.size() % 3 != 0)) {
		LogErrorArg1("Invalid argument for addVertices. Count = %d", vertices.size());
		return false;
	}

	//Append to the end of both vertex arrays
	m_vRestPos.insert(m_vRestPos.end(), vertices.begin(), vertices.end());
	m_vCurPos.insert(m_vCurPos.end(), vertices.begin(), vertices.end());
	m_vNormals.insert(m_vNormals.end(), normals.begin(), normals.end());

	return true;
}

//Add new triangles to the triangle list
bool SurfaceMesh::addTriangles(const vector<U32>& elements) {
	if(elements.size() == 0 || elements.size() % 3 != 0)
		return false;

	m_faces.insert(m_faces.end(), elements.begin(), elements.end());
	return true;
}


int SurfaceMesh::trianglesAroundVertex(U32 idxVertex, vector<U32>& outTriangles) {
	outTriangles.reserve(128);

	for(U32 i=0; i<m_faces.size(); i++) {
		if(m_faces[i] == idxVertex)
			outTriangles.push_back(i / 3);
	}
	return (int)outTriangles.size();
}

bool SurfaceMesh::readFromDisk(const char* chrObjFilePath) {
	//Read From Disk
	ifstream ifs(chrObjFilePath, ios::in);
	if(!ifs.is_open())
		return false;

	char buffer[2048];
	U32 ctVertices = 0;
	U32 ctNormals = 0;
	U32 ctFaces = 0;

	DAnsiStr strLine;
	vector<DAnsiStr> words;

	//Read Line by Line and Count
	while( !ifs.eof())
	{
		ifs.getline(buffer, 2048);
		strLine.copyFromT(buffer);
		strLine.trim();
		strLine.removeStartEndSpaces();

		if(strLine.firstChar() == '#')
			continue;

		//Decompose line to words
		int ctWords = strLine.decompose(' ', words);

		//Position
		if(words[0] == "v")
			ctVertices++;
		else if(words[0] == "vn")
			ctNormals++;
		else if(words[0] == "f") {
			if(ctFaces == 0)
				LogErrorArg2("Irregular mesh file! Face %d has %d vertices!", ctFaces, ctWords-1);
			ctFaces++;
		}
	}

	//Move to beginning
	ifs.clear();
	ifs.seekg(0, ifs.beg);

	//Vertices
	m_vCurPos.reserve(ctVertices * 3);
	m_vNormals.reserve(ctVertices * 3);
	m_faces.reserve(ctFaces * 3);

	//Read Line by Line and add values
	while( !ifs.eof())
	{
		ifs.getline(buffer, 2048);
		strLine.copyFromT(buffer);
		strLine.trim();
		strLine.removeStartEndSpaces();

		if(strLine.firstChar() == '#')
			continue;

		//Decompose line to words
		int ctWords = strLine.decompose(' ', words);

		//Position
		if(words[0] == "v" && ctWords == 4) {
			m_vCurPos.push_back(atof(words[1].cptr()));
			m_vCurPos.push_back(atof(words[2].cptr()));
			m_vCurPos.push_back(atof(words[3].cptr()));
		}
		else if(words[0] == "vn" && ctWords == 4) {
			m_vNormals.push_back(atof(words[1].cptr()));
			m_vNormals.push_back(atof(words[2].cptr()));
			m_vNormals.push_back(atof(words[3].cptr()));
		}
		else if(words[0] == "f") {

			int idxVertex[4];
			int idxTexCoords[4];
			int idxNormal[4];

			//Process line of face
			for(int j=0; j<ctWords-1; j++)
			{
				std::vector<DAnsiStr> segments;
				strLine = words[j + 1];
				strLine.removeStartEndSpaces();
				strLine.replaceChars('/', ' ');
				if(strLine.decompose(' ', segments) > 1)
				{
					int ctSegs = segments.size();
					if(ctSegs == 3)
					{
						idxVertex[j] 	= atoi(segments[0].ptr()) - 1;
						idxTexCoords[j] = atoi(segments[1].ptr()) - 1;
						idxNormal[j] 	= atoi(segments[2].ptr()) - 1;
					}
					else
					{
						idxVertex[j] 	= atoi(segments[0].ptr()) - 1;
						idxNormal[j] 	= atoi(segments[1].ptr()) - 1;
					}
				}
				else
				{
					idxVertex[j] = atoi(words[j + 1].ptr()) - 1;
					idxNormal[j] = idxVertex[j];
				}

				//Add to index buffer
				m_faces.push_back(idxVertex[j]);
			}
		}
	}

	//Copy vertices to rest-pos
	m_vRestPos.assign(m_vCurPos.begin(), m_vCurPos.end());


	return true;
}

}
}