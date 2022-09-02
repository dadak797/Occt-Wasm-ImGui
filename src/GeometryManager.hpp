#pragma once

#include <XCAFApp_Application.hxx>
#include <TDocStd_Document.hxx>
#include <TopLoc_Location.hxx>

template <typename T> class LCRSTree; 
template <typename T> class LCRSNode;
class Geometry;
class TDF_Label;

class GeometryManager
{
    using GEOMETRY_NODE = LCRSNode<Geometry>*;
    using GEOMETRY_TREE = LCRSTree<Geometry>*;

public:
    GeometryManager();
    ~GeometryManager();

    bool ImportStepFile(const char* fileName, std::istream& istream);
    void PrintAllGeometryName();
    void DisplayAllGeometry();
    void CreateAllGeometryIndexMap();

    void SelectVertexMode();
    void SelectEdgeMode();
    void SelectFaceMode();
    void SelectSolidMode();

private:
    GEOMETRY_TREE m_pGeometryTree;

    Handle(XCAFApp_Application) m_hXCAFApp;
    Handle(TDocStd_Document) m_hStdDoc;

    bool GetOCCDocFromStepFile(const char* fileName, std::istream& istream);
    bool LoadGeometryFromOCCDoc();
    void IterateFather(const TDF_Label& label, GEOMETRY_NODE node, const int tag, TopLoc_Location loc, bool callByTree = false);
    GEOMETRY_NODE AddGeometryToTree(const TDF_Label& label, GEOMETRY_NODE node, const int tag, TopLoc_Location loc);

    // Getters
    TCollection_AsciiString GetEntryString(const TDF_Label& label);
    TCollection_AsciiString GetNameString(const TDF_Label& label);

    // For Looping Geometry Tree
    static void PrintIDName(GEOMETRY_NODE node, int depth);
    static void PrintGeometryIndexMap(GEOMETRY_NODE node, int depth);
    static void DisplayGeometry(GEOMETRY_NODE node, int depth);
    static void CreateGeometryIndexMap(GEOMETRY_NODE node, int depth);

    static void SelectVertex(GEOMETRY_NODE node, int depth);
    static void SelectEdge(GEOMETRY_NODE node, int depth);
    static void SelectFace(GEOMETRY_NODE node, int depth);
    static void SelectSolid(GEOMETRY_NODE node, int depth);
};