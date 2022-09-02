#include "Geometry.hpp"

#include <AIS_ColoredShape.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Vertex.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopExp.hxx>


int Geometry::s_LastID = 0;

Geometry::Geometry(const char* name)
    : m_ID(s_LastID++), m_Name(name), m_AISShape(nullptr)
{
}

Geometry::Geometry(const char* name, Handle(AIS_ColoredShape) shape)
    : m_ID(s_LastID++), m_Name(name), m_AISShape(shape)
{
}

Geometry::Geometry(const Geometry& other)
    : m_ID(other.m_ID), m_Name(other.m_Name), m_AISShape(other.m_AISShape)
{
}

Geometry::Geometry(Geometry&& other)
    : m_ID(other.m_ID), m_Name(std::move(other.m_Name)), m_AISShape(other.m_AISShape)
{
    other.m_ID = -1;
    other.m_AISShape = nullptr;
}

Geometry::~Geometry()
{
    if (m_pVertexMap) {
        delete m_pVertexMap;
        m_pVertexMap = nullptr;
    }
    if (m_pEdgeMap) {
        delete m_pEdgeMap;
        m_pEdgeMap = nullptr;
    }
    if (m_pFaceMap) {
        delete m_pFaceMap;
        m_pFaceMap = nullptr;
    }
    if (!m_AISShape.IsNull()) m_AISShape.Nullify();
}

Geometry& Geometry::operator=(const Geometry& other)
{
    m_ID = other.m_ID;
    m_Name = other.m_Name;
    m_AISShape = other.m_AISShape;
}

Geometry& Geometry::operator=(Geometry&& other)
{
    m_ID = other.m_ID;
    m_Name = std::move(other.m_Name);
    m_AISShape = other.m_AISShape;

    other.m_ID = -1;
    other.m_Name.clear();
    other.m_AISShape = nullptr;
}

bool Geometry::HasShape() const
{ 
    return !m_AISShape.IsNull(); 
}

void Geometry::SetColor(Quantity_Color color)
{
    m_Color = color;
}

void Geometry::SetShape(Handle(AIS_ColoredShape) shape)
{
    m_AISShape = shape;
}

void Geometry::CreateIndexedMap()
{
    if (m_AISShape.IsNull()) return;

    m_pVertexMap = new TopTools_IndexedMapOfShape();
    m_pEdgeMap = new TopTools_IndexedMapOfShape();
    m_pFaceMap = new TopTools_IndexedMapOfShape();

    // Referred Indexing Method in MeshGems Index
    int Face_ID = 0;
    for (TopExp_Explorer Face_It(m_AISShape->Shape(), TopAbs_FACE); Face_It.More(); Face_It.Next()) {
        const TopoDS_Face& TopoFace = TopoDS::Face(Face_It.Current());
        if (m_pFaceMap->FindIndex(TopoFace) > 0) continue;
        m_pFaceMap->Add(TopoFace);
        Face_ID++;

        for (TopExp_Explorer Edge_It(TopoFace, TopAbs_EDGE); Edge_It.More(); Edge_It.Next()) {
            const TopoDS_Edge& TopoEdge = TopoDS::Edge(Edge_It.Current());
            int Edge_ID = m_pEdgeMap->FindIndex(TopoEdge);
            if (Edge_ID <= 0) {
                m_pEdgeMap->Add(TopoEdge); // If Edge_ID > 0 means, the edge is already added in the index-map.
            }

            int Vertex_ID1, Vertex_ID2;
            const TopoDS_Vertex& TopoVertex1 = TopExp::FirstVertex(TopoEdge);
            const TopoDS_Vertex& TopoVertex2 = TopExp::LastVertex(TopoEdge);

            Vertex_ID1 = m_pVertexMap->FindIndex(TopoVertex1);
            if (Vertex_ID1 <= 0)
                m_pVertexMap->Add(TopoVertex1);

            Vertex_ID2 = m_pVertexMap->FindIndex(TopoVertex2);
            if (Vertex_ID2 <= 0)
                m_pVertexMap->Add(TopoVertex2);
        }  // For Edge Iterator
    }  // For Face Iterator
}