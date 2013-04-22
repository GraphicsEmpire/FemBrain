/*
 * PS_Mesh.cpp
 *
 *  Created on: Jul 4, 2011
 *      Author: pourya
 */

#include <assert.h>
#include <fstream>
#include <algorithm>
#include "PS_Mesh.h"
#include "../PS_Base/PS_FileDirectory.h"
#include "../PS_Base/PS_ErrorManager.h"
#include "../PS_Base/PS_Logger.h"

using namespace PS;
using namespace PS::FILESTRINGUTILS;

namespace PS{
namespace MESH{

MeshMaterial::MeshMaterial() {
    m_isSerializeable = true;
}

MeshMaterial::MeshMaterial(const char* chrFilePath) {
    m_isSerializeable = true;
    this->load(chrFilePath);
}

MeshMaterial::~MeshMaterial() {

}

bool MeshMaterial::load(const char *chrFilePath) {
    std::vector<DAnsiStr> content;
    if(!ReadTextFile(chrFilePath, content))
        return false;

    //Process content line by line
    DAnsiStr strLine;
    std::vector<DAnsiStr> words;
    for(U32 i=0; i<content.size(); i++)
    {
        strLine = content[i];
        if(strLine.firstChar() == '#')
            continue;
        if(strLine.decompose(' ', words) == 0)
            continue;

        int ctWords = (int)words.size();

        if((words[0] == "newmtl")&&(ctWords == 2))
        {
            this->strMTLName = string(words[1].cptr());
        }
        else if((words[0] == "Ns")&&(ctWords == 2))
        {
            this->shininess = static_cast<float>(atof(words[1].ptr()));
        }
        else if((words[0] == "Ka")&&(ctWords == 4))
        {
            this->ambient.x = static_cast<float>(atof(words[1].ptr()));
            this->ambient.y = static_cast<float>(atof(words[2].ptr()));
            this->ambient.z = static_cast<float>(atof(words[3].ptr()));
        }
        else if((words[0] == "Kd")&&(ctWords == 4))
        {
            this->diffused.x = static_cast<float>(atof(words[1].ptr()));
            this->diffused.y = static_cast<float>(atof(words[2].ptr()));
            this->diffused.z = static_cast<float>(atof(words[3].ptr()));
        }
        else if((words[0] == "Ks")&&(ctWords == 4))
        {
            this->specular.x = static_cast<float>(atof(words[1].ptr()));
            this->specular.y = static_cast<float>(atof(words[2].ptr()));
            this->specular.z = static_cast<float>(atof(words[3].ptr()));
        }
        else if((words[0] == "Ni")&&(ctWords == 2))
        {
            this->refractIndex = static_cast<float>(atof(words[1].ptr()));
        }
        else if((words[0] == "d")&&(ctWords == 2))
        {
            //d factor for transparency
            this->trans = static_cast<float>(atof(words[1].ptr()));
        }
        else if((words[0] == "illum")&&(ctWords == 2))
        {
            this->illuminationModel = static_cast<int>(atoi(words[1].ptr()));
        }
        else if((words[0] == "map_Kd")&&(ctWords == 2))
        {
            this->strTexName = string(words[1].cptr());
        }
    }
    content.resize(0);
    return true;

}

bool MeshMaterial::store(const char *chrFilePath) {
	std::vector<DAnsiStr> vContent;

	if(strMTLName.length() > 0)
		vContent.push_back(printToAStr("# PS::MESH Material File: %s", strMTLName.c_str()));
	else
		vContent.push_back(DAnsiStr("# PS::MESH Material File"));
	vContent.push_back("# Material Count: 1");
	vContent.push_back("newmtl");
	vContent.push_back(printToAStr("Ns %f", shininess));
	vContent.push_back(printToAStr("Ka %f %f %f", ambient.x, ambient.y, ambient.z));
	vContent.push_back(printToAStr("Kd %f %f %f", diffused.x, diffused.y, diffused.z));
	vContent.push_back(printToAStr("Ks %f %f %f", specular.x, specular.y, specular.z));

	vContent.push_back(printToAStr("Ni %f", refractIndex));
	vContent.push_back(printToAStr("d %f", trans));
	vContent.push_back(printToAStr("illum %d", illuminationModel));

	if(strTexName.length() > 0)
		vContent.push_back(printToAStr("map_Kd %s", strTexName.c_str()));

	return WriteTextFile(DAnsiStr(chrFilePath), vContent);
}





MeshNode::MeshNode()
{
    init();
}

MeshNode::MeshNode(const string &name) {
    init();
    strNodeName = name;
}

MeshNode::~MeshNode()
{
	arrIndices.resize(0);
	arrTexCoords.resize(0);
	arrNormals.resize(0);
	arrVertices.resize(0);
}

void MeshNode::init() {
    strNodeName = "UNTITLED";
    szUnitFace = 3;
    szUnitTexCoord = 2;
    szUnitVertex   = 3;
    lpMaterial = NULL;
}

bool MeshNode::readbackMeshV3T3(U32& ctVertices, vector<float>& vertices,
									U32& ctTriangles, vector<U32>& elements) {
	if(szUnitVertex != 3 || szUnitFace != 3)
		return false;

	ctVertices = this->countVertices();
	ctTriangles = this->countFaces();

	vertices.assign(this->arrVertices.begin(), this->arrVertices.end());
	elements.assign(this->arrIndices.begin(), this->arrIndices.end());
	return true;
}


int MeshNode::add(const vec3f& v, VertexAttribType vat, int step)
{
	if(step > 3 || step < 0)
		return -1;

	int idxAdded = -1;
	switch(vat) {
	case(vatPosition):
	{
		for(int i=0; i<step; i++)
			arrVertices.push_back(v.e[i]);
		idxAdded = (int)this->countVertices() - 1;
	}
	break;

	case(vatNormal):
	{
		for(int i=0; i<step; i++)
			arrNormals.push_back(v.e[i]);
		idxAdded = (int)this->countNormals() - 1;
	}
	break;

	case(vatTexCoord):
	{
		for(int i=0; i<step; i++)
			arrTexCoords.push_back(v.e[i]);
		idxAdded = (int)this->countTexCoords() - 1;
	}
	break;

	default: {
		LogError("Unhandled Vertex Attrib Type");
	}

	}

	return idxAdded;
}

void MeshNode::add(const vector<float>& arrAttribs, VertexAttribType vat, int step) {
	switch(vat) {
	case(vatPosition): {
		szUnitVertex = step;
		arrVertices.assign(arrAttribs.begin(), arrAttribs.end());
	}
	break;

	case(vatTexCoord): {
		szUnitTexCoord = step;
		arrTexCoords.assign(arrAttribs.begin(), arrAttribs.end());
	}
	break;


	case(vatNormal): {
		arrNormals.assign(arrAttribs.begin(), arrAttribs.end());
	}
	break;
	}
}

int MeshNode::addVertex(const vec3f& v)
{
	assert(szUnitVertex == 3);
	arrVertices.push_back(v.x);
	arrVertices.push_back(v.y);
	arrVertices.push_back(v.z);
	return (int)countVertices() - 1;
}

int MeshNode::addNormal(const vec3f& n)
{
	arrNormals.push_back(n.x);
	arrNormals.push_back(n.y);
	arrNormals.push_back(n.z);
	return (int)countNormals() - 1;
}

int MeshNode::addTexCoord2(const vec2f& t)
{
	assert(szUnitTexCoord == 2);
	arrTexCoords.push_back(t.x);
	arrTexCoords.push_back(t.y);
	return (int)countTexCoords() - 1;
}

int MeshNode::addTexCoord3(const vec3f& t)
{
	assert(szUnitTexCoord == 3);
	arrTexCoords.push_back(t.x);
	arrTexCoords.push_back(t.y);
	arrTexCoords.push_back(t.z);
	return (int)countTexCoords() - 1;
}

void MeshNode::addFaceIndex(U32 index)
{
	arrIndices.push_back(index);
}

void MeshNode::addFaceIndices(const vector<U32>& arrFaces, int unitFace) {
	szUnitFace = unitFace;
	arrIndices.assign(arrFaces.begin(), arrFaces.end());
}

int MeshNode::countFaceIndexErrors() const
{
	size_t ctVertices = countVertices();

	int ctErrors = 0;
	for(size_t i=0; i<arrIndices.size(); i++)
	{
		if(arrIndices[i] >= ctVertices)
			ctErrors++;
	}

	return ctErrors;
}

void MeshNode::computeVertexNormalsFromFaces() {
    if(szUnitFace != 3)
        return;

    //Per each vertex we need to compute all the neighboring faces
    vec3u32 face;
    vec3f v[3];
    vec3f n[3];
    vec3f nf;
    U32 ctFaces = countFaces();
    arrNormals.resize(arrVertices.size());
    std::fill(arrNormals.begin(), arrNormals.end(), 0.0f);
    for(U32 i=0; i<ctFaces; i++) {
        face.load(&arrIndices[i*3]);
        v[0] = getVertex(face.x);
        v[1] = getVertex(face.y);
        v[2] = getVertex(face.z);

        //cross(b-a, c-a)
        //Face Normal
        nf = vec3f::cross(v[1] - v[0], v[2] - v[0]);
        n[0] = getNormal(face.x) + nf;
        n[1] = getNormal(face.y) + nf;
        n[2] = getNormal(face.z) + nf;

        n[0].store(&arrNormals[face.x*3]);
        n[1].store(&arrNormals[face.y*3]);
        n[2].store(&arrNormals[face.z*3]);
    }

    //Normalize all normals
    U32 ctVertices = this->countVertices();
    for(U32 i=0; i<ctVertices; i++) {
        nf = getNormal(i);
        nf.normalize();
        nf.store(&arrNormals[i*3]);
    }
}


vec3f MeshNode::getVertex(int idxVertex) const
{
    //assert(szUnitVertex == 3);
    return vec3f(&arrVertices[idxVertex*szUnitVertex]);
}

vec3f MeshNode::getNormal(int idxVertex) const
{
    return vec3f(&arrNormals[idxVertex*3]);
}

vec2f MeshNode::getTexCoord2(int idxVertex) const
{
    //assert(szUnitTexCoord == 2);
    return vec2f(&arrTexCoords[idxVertex * szUnitTexCoord]);
}

vec3f MeshNode::getTexCoord3(int idxVertex) const
{
    //assert(szUnitTexCoord == 2);
    return vec3f(&arrTexCoords[idxVertex * szUnitTexCoord]);
}

AABB MeshNode::computeBoundingBox() const
{
	AABB oct;
	size_t ctVertices = this->countVertices();
	if(ctVertices == 0)
		return oct;

	vec3f lo = this->getVertex(0);
	vec3f hi = this->getVertex(0);

	for(size_t i=1; i<ctVertices; i++)
	{
		vec3f v = this->getVertex(i);
		if(v.x < lo.x)	lo.x = v.x;
		if(v.y < lo.y)	lo.y = v.y;
		if(v.z < lo.z)	lo.z = v.z;

		if(v.x > hi.x)	hi.x = v.x;
		if(v.y > hi.y)	hi.y = v.y;
		if(v.z > hi.z)	hi.z = v.z;
	}

	oct.set(lo, hi);
	return oct;
}

void MeshNode::getVertexAttrib(U32& count, vector<float>& arrAttribs, VertexAttribType vat) const {
	switch(vat) {
	case(vatPosition): {
		count = countVertices();
		arrAttribs.assign(arrVertices.begin(), arrVertices.end());
	}
	break;

	case(vatTexCoord): {
		count = countTexCoords();
		arrAttribs.assign(arrTexCoords.begin(), arrTexCoords.end());
	}
	break;

	case(vatNormal): {
		count = countVertices();
		arrAttribs.assign(arrNormals.begin(), arrNormals.end());
	}
	break;
	}
}

void MeshNode::getFaces(U32& count, vector<U32>& faces) const {
	count = countFaces();
	faces.assign(arrIndices.begin(), arrIndices.end());
}

void MeshNode::move(const vec3f& d) {
    U32 count = arrVertices.size() / 3;
    for(U32 i=0; i<count; i++) {
        float* pValues = &arrVertices[i * 3];
        vec3f v = vec3f(pValues) + d;
        v.store(pValues);
    }
}

void MeshNode::scale(const vec3f& s) {
    U32 count = arrVertices.size() / 3;
    for(U32 i=0; i<count; i++) {
        float* pValues = &arrVertices[i * 3];
        vec3f v = vec3f(pValues);
        v.x *= s.x;
        v.y *= s.y;
        v.z *= s.z;
        v.store(pValues);
    }
}

void MeshNode::rotate(const quat& q) {
    quat qInv = q.inverted();
    U32 count = arrVertices.size() / 3;
    for(U32 i=0; i<count; i++) {
        float* pValues = &arrVertices[i * 3];
        vec3f v = vec3f(pValues);
        v = q.transform(qInv, v);
        v.store(pValues);
    }
}


void MeshNode::fitToBBox(const AABB& box) {

    vec3f e = this->computeBoundingBox().extent();    
    bool isDegenerate = ((e.x < EPSILON)||(e.y < EPSILON)||(e.z < EPSILON));


    //To keep the aspect ratio contant scale down the model to min BBOX
    if(!isDegenerate) {
        vec3f s = vec3f::div(box.extent(), e);
        float sMin = MATHMIN(MATHMIN(s.x, s.y), s.z);
        scale(vec3f(sMin));
    }

    AABB nb = computeBoundingBox();
    move(box.lower() - nb.lower());

    //Compare boxes now
    nb = computeBoundingBox();
}



/*
void CMeshNode::add(obj_vector* v, ArrayType where)
{
	if(where == atVertex)
	{
		for(int i=0;i<szUnitVertex;i++)
			arrVertices.push_back(static_cast<float>(v->e[i]));
	}
	else if(where == atNormal)
	{
		for(int i=0;i<3;i++)
			arrNormals.push_back(static_cast<float>(v->e[i]));
	}
	else if(where == atTexture)
	{
		for(int i=0;i<szUnitTexCoord;i++)
			arrTexCoords.push_back(static_cast<float>(v->e[i]));
	}
	else if(where == atFaceIndex)
	{
		for(int i=0;i<szUnitFace;i++)
			arrIndices.push_back(static_cast<float>(v->e[i]));
	}
}
*/
/////////////////////////////////////////////////////////////////////////
Mesh::Mesh() {

}

Mesh::Mesh(const char* chrFileName)
{
    load(chrFileName);
}

Mesh::~Mesh()
{
	for(size_t i=0; i<m_nodes.size(); i++)
		SAFE_DELETE(m_nodes[i]);
	m_nodes.resize(0);

	for(size_t i=0; i<m_materials.size(); i++)
		SAFE_DELETE(m_materials[i]);
	m_materials.resize(0);
}

void Mesh::addNode(MeshNode* lpMeshNode)
{
	if(lpMeshNode != NULL)
		m_nodes.push_back(lpMeshNode);
}

MeshNode*	Mesh::getNode(int idx) const
{
	if(idx >=0 && idx < (int)m_nodes.size())
		return m_nodes[idx];
	else
		return NULL;
}

void Mesh::addMeshMaterial(MeshMaterial* lpMaterial)
{
    if(lpMaterial != NULL) {
		m_materials.push_back(lpMaterial);
        m_mapMaterial.insert(std::pair<string, MeshMaterial*>(string(lpMaterial->name()), lpMaterial));
    }
}

MeshMaterial* Mesh::getMaterial(int idx) const
{
	if(idx >= 0 && idx < (int)m_materials.size())
		return m_materials[idx];
	else
		return NULL;
}

MeshMaterial* Mesh::getMaterial(const string& strName) const
{
    std::map<string, MeshMaterial*>::const_iterator it;
    it = m_mapMaterial.find(strName);
    if(it != m_mapMaterial.end())
        return it->second;
    else
        return NULL;
}

bool Mesh::load(const char* chrFilePath)
{
    m_strFilePath = string(chrFilePath);
    DAnsiStr strExt = ExtractFileExt(DAnsiStr(m_strFilePath.c_str()));
    bool bres = false;
    if(strExt == "obj") {
        bres = loadObjGlobalVertexNormal(m_strFilePath.c_str());
        computeMissingNormals();
    }
	else
		ReportError("Invalid file format.");
    return bres;
}

bool Mesh::store(const char* chrFilePath) {
	ofstream fp;
	fp.open(chrFilePath);


	//Store All Materials
	DAnsiStr strFilePath = ExtractFilePath(DAnsiStr(chrFilePath));
	DAnsiStr strMtlPath = strFilePath;
	for(U32 i=0; i<countMaterials(); i++) {

		if(getMaterial(i)->name().length() > 0)
			strMtlPath += DAnsiStr(getMaterial(i)->name().c_str()) + ".mtl";
		else
			strMtlPath += printToAStr("mtlnode%d.mtl", i+1);

		getMaterial(i)->store(strMtlPath.cptr());
	}



	//Face indices are at index 1
	MeshNode* lpNode = NULL;
	for(U32 iNode=0; iNode < countNodes(); iNode++) {
		lpNode = getNode(iNode);

		if(lpNode == NULL)
			continue;

		//Write All vertices
		fp << "# Generated by PS::MESH\n";
		fp << "# Number of vertices: " << lpNode->countVertices() << endl;
		fp << "# Number of texture coordinates: " << lpNode->countTexCoords() << endl;
		fp << "# Number of normals: " << lpNode->countNormals() << endl;
		fp << "# Number of faces: " << lpNode->countFaces() << endl;
		fp << "# Group " << iNode+1 << " of " << countNodes() << endl;

		//Vertices
		U32 ctVertices;
		U32 szUnitVertex = lpNode->getUnitVertex();
		vector<float> attribs;
		lpNode->getVertexAttrib(ctVertices, attribs, vatPosition);
		for(U32 i=0; i < ctVertices; i++) {
			fp << "v";
			for(U32 j=0; j<szUnitVertex; j++) {
				fp << " " << attribs[i * szUnitVertex + j];
			}

			fp << endl;
		}

		//Normals
		for(U32 i=0; i < lpNode->countVertices(); i++) {

			vec3f n = lpNode->getNormal(i);
			fp << "vn " << n.x << " " << n.y << " " << n.z << endl;
		}

		//TexCoords
		if(lpNode->getUnitTexCoord() == 2) {
			for(U32 i=0; i < lpNode->countTexCoords(); i++) {
				vec2f t = lpNode->getTexCoord2(i);
				fp << "vt " << t.x << " " << t.y << endl;
			}
		}
		else if(lpNode->getUnitTexCoord() == 3) {
			for(U32 i=0; i < lpNode->countTexCoords(); i++) {
				vec3f t = lpNode->getTexCoord3(i);
				fp << "vt " << t.x << " " << t.y << " " << t.z << endl;
			}
		}
		else
			LogErrorArg1("Unsupported texcoords found in MeshGroup: %d", iNode);

		//Faces
		U32 ctFaces = 0;
		vector<U32> arrIndices;
		lpNode->getFaces(ctFaces, arrIndices);
		if(lpNode->getUnitFace() == 3) {
			vec3u32 f;
			vec3u32 inc(1);
			for(U32 i=0; i<ctFaces; i++) {
				f.load(&arrIndices[i * 3]);
				f = f + inc;
				fp << "f " << f.x << " " << f.y << " " << f.z << endl;
			}
		}
		else if(lpNode->getUnitFace() == 4) {
			vec4u32 f;
			vec4u32 inc(1);
			for(U32 i=0; i<ctFaces; i++) {
				f.load(&arrIndices[i * 4]);
				f = f + inc;
				fp << "f " << f.x << " " << f.y << " " << f.z << " " << f.w << endl;
			}
		}

	}



	fp.close();
	return true;
}

AABB Mesh::computeBoundingBox() const
{
	AABB box;
    if(this->countNodes() == 0)
		return box;

	box = m_nodes[0]->computeBoundingBox();
	for(size_t i=1; i<m_nodes.size(); i++)
		box = box.united(m_nodes[i]->computeBoundingBox());
	return box;
}

void Mesh::computeMissingNormals() {
    for(int i=0; i<this->countNodes(); i++) {
        MeshNode* aNode = this->getNode(i);
        if(aNode->countVertices() > 0 && aNode->countNormals() == 0)
            aNode->computeVertexNormalsFromFaces();
    }
}

void Mesh::move(const vec3f &d) {
    for(U32 i=0; i<this->countNodes(); i++)
        this->getNode(i)->move(d);
}

void Mesh::scale(const vec3f& s) {
    for(U32 i=0; i<this->countNodes(); i++)
        this->getNode(i)->scale(s);
}

void Mesh::rotate(const quat& q) {
    for(U32 i=0; i<this->countNodes(); i++)
        this->getNode(i)->rotate(q);
}

void Mesh::fitToBBox(const AABB& box) {
    for(U32 i=0; i<this->countNodes(); i++)
        this->getNode(i)->fitToBBox(box);
}

bool Mesh::loadObj(const char* chrFileName)
{
	std::vector<DAnsiStr> content;
    if(!ReadTextFile(chrFileName, content))
		return false;

	//Process content line by line
	DAnsiStr strLine;
	std::vector<DAnsiStr> words;
	MeshNode* aNode = NULL;

	U32 ctVertices = 0;
	U32 ctVertexOffset = 0;
	for(size_t i=0; i<content.size(); i++)
	{
		strLine = content[i];

		if(strLine.firstChar() == '#')
			continue;
		if(strLine.decompose(' ', words) == 0)
			continue;

		int ctWords = (int)words.size();

		if((words[0] == "o")&&(ctWords == 2))
		{
			//Create a new mesh node
			aNode = new MeshNode();
            aNode->strNodeName = string(words[1].ptr());
			aNode->szUnitFace     = 3;
			aNode->szUnitTexCoord = 2;
			aNode->szUnitVertex   = 3;
			m_nodes.push_back(aNode);

			ctVertexOffset = ctVertices;
		}
		else if((words[0] == "mtllib")&&(ctWords == 2))
		{
			DAnsiStr strFP = DAnsiStr(chrFileName);
			strFP = ExtractFilePath(strFP) + words[1];

            //Load Material
            MeshMaterial* lpMtrl = new MeshMaterial(strFP.cptr());
            lpMtrl->setName(string(words[1].cptr()));
            m_materials.push_back(lpMtrl);
		}
		else if((words[0] == "v")&&(aNode)&&(ctWords == 4))
		{
			vec3f v;
			v.x = static_cast<float>(atof(words[1].ptr()));
			v.y = static_cast<float>(atof(words[2].ptr()));
			v.z = static_cast<float>(atof(words[3].ptr()));
			aNode->add(v, vatPosition, 3);

			//Added Vertex
			ctVertices++;
		}
		else if((words[0] == "vn")&&(aNode)&&(ctWords == 4))
		{
			vec3f v;
			v.x = static_cast<float>(atof(words[1].ptr()));
			v.y = static_cast<float>(atof(words[2].ptr()));
			v.z = static_cast<float>(atof(words[3].ptr()));
			aNode->add(v, vatNormal, 3);
		}
		else if((words[0] == "vt")&&(aNode))
		{
			if(ctWords - 1 != aNode->szUnitTexCoord)
				ReportError("Number of TexCoords doesnot match");
			else
			{
				for(int i=1; i<ctWords; i++)
				{
					float f = static_cast<float>(atof(words[i].ptr()));
					aNode->arrTexCoords.push_back(f);
				}
			}
		}
		else if((words[0] == "usemtl")&&(aNode)&&(ctWords == 2))
		{
            aNode->lpMaterial = getMaterial(string(words[1].cptr()));
		}
		else if((words[0] == "f")&&(aNode)&&(ctWords > 3))
		{
			int szUnit = ctWords - 1;
			int f[4];
			for(int i=0; i<szUnit; i++)
				f[i] = (atoi(words[i + 1].ptr()) - (int)ctVertexOffset) - 1;

			if(szUnit == 3)
			{
				aNode->arrIndices.push_back(f[0]);
				aNode->arrIndices.push_back(f[1]);
				aNode->arrIndices.push_back(f[2]);
			}
			else if(szUnit == 4)
			{
				aNode->arrIndices.push_back(f[0]);
				aNode->arrIndices.push_back(f[1]);
				aNode->arrIndices.push_back(f[2]);

				aNode->arrIndices.push_back(f[0]);
				aNode->arrIndices.push_back(f[2]);
				aNode->arrIndices.push_back(f[3]);
			}

		}
		else if((words[0] == "s")&&(ctWords == 2))
		{
			if(words[1] == "off")
			{

			}
		}
	}


	//Check all nodes for vertex and normal data
	for(size_t i=0; i<m_nodes.size(); i++)
	{
		MeshNode* lpNode = m_nodes[i];
		if(lpNode->arrVertices.size() != lpNode->arrNormals.size())
		{
			DAnsiStr strMsg = printToAStr("MESHNODE[%d] NAME=%s Does Not have equal number of vertices and normals! [V=%d, N=%d]",
                                          i, lpNode->getName().c_str(), lpNode->countVertices(), lpNode->countNormals());
			ReportError(strMsg.ptr());
		}
	}

	words.resize(0);
	content.resize(0);
	return (m_nodes.size() > 0);
}

bool Mesh::loadObjGlobalVertexNormal(const char* chrFileName)
{
	std::vector<DAnsiStr> content;
    if(!ReadTextFile(chrFileName, content))
		return false;

	//Process content line by line
	DAnsiStr strLine;
	std::vector<DAnsiStr> words;
	std::vector<bool> arrProcessed;
	arrProcessed.resize(content.size());
	for(size_t i=0; i<arrProcessed.size(); i++)
		arrProcessed[i] = false;

	MeshNode* lpGlobalNode = new MeshNode();
	lpGlobalNode->szUnitFace = 3;
	lpGlobalNode->szUnitTexCoord = 2;
	lpGlobalNode->szUnitVertex = 3;

	//Add all vertices, normals and texcoords to global node
	for(size_t i=0; i<content.size(); i++)
	{
		strLine = content[i];

		if(strLine.firstChar() == '#')
		{
			arrProcessed[i] = true;
			continue;
		}
		if(strLine.decompose(' ', words) == 0)
		{
			arrProcessed[i] = true;
			continue;
		}

		int ctWords = (int)words.size();

		if((words[0] == "v")&&(lpGlobalNode)&&(ctWords == 4))
		{
			vec3f v;
			v.x = static_cast<float>(atof(words[1].ptr()));
			v.y = static_cast<float>(atof(words[2].ptr()));
			v.z = static_cast<float>(atof(words[3].ptr()));
			lpGlobalNode->add(v, vatPosition, 3);
			arrProcessed[i] = true;
		}
		else if((words[0] == "vn")&&(lpGlobalNode)&&(ctWords == 4))
		{
			vec3f v;
			v.x = static_cast<float>(atof(words[1].ptr()));
			v.y = static_cast<float>(atof(words[2].ptr()));
			v.z = static_cast<float>(atof(words[3].ptr()));
			lpGlobalNode->add(v, vatNormal, 3);
			arrProcessed[i] = true;
		}
		else if((words[0] == "vt")&&(lpGlobalNode))
		{
			if(ctWords - 1 != lpGlobalNode->szUnitTexCoord)
				ReportError("Number of TexCoords doesnot match");
			else
			{
				for(int i=1; i<ctWords; i++)
				{
					float f = static_cast<float>(atof(words[i].ptr()));
					lpGlobalNode->arrTexCoords.push_back(f);
				}
			}
		}
	}

	//Now we can process faces and assemble them from the list of vertices normals
	MeshNode* aNode = NULL;
	for(size_t i=0; i<content.size(); i++)
	{
		strLine = content[i];

		if(arrProcessed[i])
			continue;
		if(strLine.firstChar() == '#')
			continue;
		if(strLine.decompose(' ', words) == 0)
			continue;


		int ctWords = (int)words.size();

		if((words[0] == "o")&&(ctWords == 2))
		{
			//Create a new mesh node
            aNode = new MeshNode(string(words[1].cptr()));
			m_nodes.push_back(aNode);
		}
		else if((words[0] == "mtllib")&&(ctWords == 2))
		{
			DAnsiStr strFP = DAnsiStr(chrFileName);
			strFP = ExtractFilePath(strFP) + words[1];

            //Load Material
            MeshMaterial* lpMtrl = new MeshMaterial(strFP.cptr());
            lpMtrl->setName(string(words[1].cptr()));
            m_materials.push_back(lpMtrl);
		}
		else if((words[0] == "usemtl")&&(aNode)&&(ctWords == 2))
		{
            aNode->lpMaterial = getMaterial(string(words[1].cptr()));
		}
        else if((words[0] == "f")&&(ctWords > 3))
		{
			int idxVertex[4];
			int idxTexCoords[4];
			int idxNormal[4];
			bool bHasTexCoords = false;
			bool bHasNormals = false;
			int szUnit = ctWords - 1;

            if(aNode == NULL) {
                aNode = new MeshNode();
                m_nodes.push_back(aNode);
            }

			//Process line of face
			for(int j=0; j<szUnit; j++)
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
						bHasTexCoords = (lpGlobalNode->arrTexCoords.size() > 0);
						bHasNormals   = (lpGlobalNode->arrNormals.size() > 0);
					}
					else
					{
						idxVertex[j] 	= atoi(segments[0].ptr()) - 1;
						idxNormal[j] 	= atoi(segments[1].ptr()) - 1;
						bHasNormals = (lpGlobalNode->arrNormals.size() > 0);
					}
				}
				else
				{
					idxVertex[j] = atoi(words[j + 1].ptr()) - 1;
					idxNormal[j] = idxVertex[j];
				}

				vec3f v = lpGlobalNode->getVertex(idxVertex[j]);
				idxVertex[j] = aNode->addVertex(v);
				if(bHasNormals)
				{
					vec3f n = lpGlobalNode->getNormal(idxNormal[j]);
					idxNormal[j] = aNode->addNormal(n);
				}

				if(bHasTexCoords)
				{
					vec2f tex = lpGlobalNode->getTexCoord2(idxTexCoords[j]);
					aNode->addTexCoord2(tex);
				}
			}

			if(szUnit == 3)
			{
				aNode->arrIndices.push_back(idxVertex[0]);
				aNode->arrIndices.push_back(idxVertex[1]);
				aNode->arrIndices.push_back(idxVertex[2]);
			}
			else if(szUnit == 4)
			{
				aNode->arrIndices.push_back(idxVertex[0]);
				aNode->arrIndices.push_back(idxVertex[1]);
				aNode->arrIndices.push_back(idxVertex[2]);

				aNode->arrIndices.push_back(idxVertex[0]);
				aNode->arrIndices.push_back(idxVertex[2]);
				aNode->arrIndices.push_back(idxVertex[3]);
			}

		}
		else if((words[0] == "s")&&(ctWords == 2))
		{
			if(words[1] == "off")
			{

			}
		}
	}


	//Check all nodes for vertex and normal data
	for(size_t i=0; i<m_nodes.size(); i++)
	{
		MeshNode* lpNode = m_nodes[i];
		if(lpNode->arrVertices.size() != lpNode->arrNormals.size())
		{
			DAnsiStr strMsg = printToAStr("MESHNODE[%d] NAME=%s Does Not have equal number of vertices and normals! [V=%d, N=%d]",
                                          i, lpNode->getName().c_str(), lpNode->countVertices(), lpNode->countNormals());
			ReportError(strMsg.ptr());
		}
	}

	SAFE_DELETE(lpGlobalNode);
	arrProcessed.resize(0);
	words.resize(0);
	content.resize(0);
	return (m_nodes.size() > 0);
}


////////////////////////////////////////////////////////////////////////////////////////////
/*
CMeshManager::CMeshManager()
{

}

CMeshManager::~CMeshManager()
{
	cleanup();
}

void CMeshManager::cleanup()
{
	CMesh* lpMesh = NULL;
	for(size_t i=0; i<m_storage.size(); i++)
	{
		lpMesh = getResource(i);
		SAFE_DELETE(lpMesh);
		this->setResource(i, NULL);
	}

	m_storage.resize(0);
	m_vFilePaths.resize(0);
}

CMesh* CMeshManager::loadResource(const DAnsiStr& inStrFilePath, DAnsiStr& inoutStrName)
{
	inoutStrName = PS::FILESTRINGUTILS::ExtractFileTitleOnly(inStrFilePath);
	CMesh* aMesh = new CMesh();
	if(aMesh->load(inStrFilePath.cptr()))
	{
		return aMesh;
	}
	else
	{
		SAFE_DELETE(aMesh);
		return NULL;
	}
}
*/

}
}
