#include "GeometryManager.hpp"
#include "Geometry.hpp"
#include "LCRSTree.hpp"
#include "WasmOcctView.hpp"

// OCCT
#include <BinXCAFDrivers.hxx>
#include <STEPCAFControl_Reader.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <XCAFDoc_ColorTool.hxx>
#include <Message.hxx>
#include <TDF_Tool.hxx>
#include <TDataStd_TreeNode.hxx>
#include <XCAFDoc.hxx>
#include <XCAFDoc_ShapeTool.hxx>
#include <TDataStd_Name.hxx>
#include <TopLoc_Location.hxx>
#include <Quantity_Color.hxx>
#include <AIS_ColoredShape.hxx>
#include <Prs3d_LineAspect.hxx>
#include <TDF_ChildIterator.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Vertex.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopExp.hxx>

// Standard Libraries
#include <iostream>
#include <utility>


GeometryManager::GeometryManager()
{
    m_pGeometryTree = new LCRSTree<Geometry>();

    m_hXCAFApp = XCAFApp_Application::GetApplication();
    m_hStdDoc = nullptr;

    m_hXCAFApp->NewDocument("BinXCAF", m_hStdDoc);
    BinXCAFDrivers::DefineFormat(m_hXCAFApp);
}

GeometryManager::~GeometryManager()
{
    if (m_pGeometryTree == nullptr) return;
    delete m_pGeometryTree;
    m_pGeometryTree = nullptr;
}

bool GeometryManager::ImportStepFile(const char* fileName, std::istream& istream)
{
    if(!GetOCCDocFromStepFile(fileName, istream)) {
        return false;
    }
    
    // Load Geometry From OCC Document
    bool retStatus = LoadGeometryFromOCCDoc();

    return retStatus;
}

bool GeometryManager::GetOCCDocFromStepFile(const char* fileName, std::istream& istream)
{
    Message::DefaultMessenger()->Send(fileName, Message_Warning);
    
    STEPCAFControl_Reader readerCAF;
    STEPControl_Reader& reader = readerCAF.ChangeReader();
    
    if (XCAFDoc_DocumentTool::IsXCAFDocument(m_hStdDoc)) {
        IFSelect_ReturnStatus status = reader.ReadStream(fileName, istream);
        if (status != IFSelect_RetDone) {
            switch (status)
		    {
		    case IFSelect_RetError:
                Message::DefaultMessenger()->Send("Not a valid Step file", Message_Warning);
    			return false;
		    case IFSelect_RetFail:
                Message::DefaultMessenger()->Send("Reading has failed", Message_Warning);
			    return false;
		    case IFSelect_RetVoid:
                Message::DefaultMessenger()->Send("Nothing to transfer", Message_Warning);
		    	return false;
		    default:
			    return false;
                // Qualifies an execution status :
                // RetVoid  : normal execution which created nothing, or no data to process
                // RetDone  : normal execution with a result
                // RetError : error in command or input data, no execution
                // RetFail  : execution was run and has failed
                // RetStop  : indicates end or stop (such as Raise)
		    }
        }
        if (!readerCAF.Transfer(m_hStdDoc)) {  // Transfer reader data to m_hStdDoc
            Message::DefaultMessenger()->Send("Cannot read any relevant data from the STEP file", Message_Warning);
            return false;
        }
        Message::DefaultMessenger()->Send("Read STEP File done!", Message_Warning);
        return true;
    }
    return false;
}

bool GeometryManager::LoadGeometryFromOCCDoc()
{
    TDF_Label mainLabel = m_hStdDoc->Main();
    TDF_Label shapeLabel = mainLabel.FindChild(mainLabel.Tag(), false);
    int shapeTag = shapeLabel.Tag();

    Geometry rootGeometry("Root");
    GEOMETRY_NODE rootNode = m_pGeometryTree->InsertItem(std::move(rootGeometry));
    
    TopLoc_Location location;
    location.Identity();  // Set location to identity matrix

    IterateFather(shapeLabel, rootNode, shapeTag, location);


    //std::string s = std::to_string(mainLabel.Tag());
    Message::DefaultMessenger()->Send(TCollection_AsciiString(mainLabel.Tag()).ToCString(), Message_Warning);
    //TCollection_AsciiString entryStr;
	//TDF_Tool::Entry(mainLabel, entryStr);
    //Message::DefaultMessenger()->Send(entryStr.ToCString(), Message_Warning);
    Message::DefaultMessenger()->Send(GetEntryString(mainLabel).ToCString(), Message_Warning);
    Message::DefaultMessenger()->Send(GetEntryString(shapeLabel).ToCString(), Message_Warning);

    return true;
}

void GeometryManager::IterateFather(const TDF_Label& label, GEOMETRY_NODE node, const int tag, TopLoc_Location loc, bool callByTree)
{
    Handle(XCAFDoc_ShapeTool) shapeTool = XCAFDoc_DocumentTool::ShapeTool(label);	
	Handle(TDataStd_TreeNode) tree;

    if (label.FindAttribute(XCAFDoc::ShapeRefGUID(), tree)) { // If this label has TreeNode
		if (tree->HasFirst()) { // This Label is reference label
			if (!callByTree) {
				return; // If reference label found, break this loop. (assume that if the first reference found all assemblies are operated.)
			}
		}
		if (tree->HasFather()) {
			// Calculate Location
			TopLoc_Location thisLocation;
			thisLocation = shapeTool->GetLocation(label);
			thisLocation = loc * thisLocation;
			IterateFather(tree->Father()->Label(), node, tag, thisLocation, true); // call by tree
			return;
		}
	}

    GEOMETRY_NODE currentNode = AddGeometryToTree(label, node, tag, loc);

	if (label.HasChild()) {
		//Node<Geometry>* np = geometryTree->GetThisNode();
		for (TDF_ChildIterator itall(label, Standard_False); itall.More(); itall.Next()) {
			TDF_Label childLabel = itall.Value();
			Standard_Integer childTag = childLabel.Tag();			
			IterateFather(childLabel, currentNode, childTag, loc);
		}
	}
}

GeometryManager::GEOMETRY_NODE GeometryManager::AddGeometryToTree(const TDF_Label& label, GEOMETRY_NODE node, const int tag, TopLoc_Location loc)
{
    Handle(XCAFDoc_ShapeTool) shapeTool = XCAFDoc_DocumentTool::ShapeTool(label);
	Handle(XCAFDoc_ColorTool) colorTool = XCAFDoc_DocumentTool::ColorTool(label);
	Handle(TDataStd_TreeNode) tree;
	TopoDS_Shape aShape;
	//CString entryStr, nameStr;
	//Standard_Integer aTag = label.Tag();
	bool isSubShape = false;
	
    TCollection_AsciiString entryStr = GetEntryString(label);
    std::string nameStr = GetNameString(label).ToCString();
	//OutputDebugString(_T("Entry: ") + CString(entryStr) + _T(", Name: ") + CString(nameStr) + _T("\n"));

    TCollection_AsciiString entryNameStr = TCollection_AsciiString("Entry: ") + entryStr + TCollection_AsciiString(", Name: ") + TCollection_AsciiString(nameStr.c_str()) + "\n";
    Message::DefaultMessenger()->Send(entryNameStr.ToCString(), Message_Warning);

	if (nameStr != std::string("Shapes") && entryStr != TCollection_AsciiString("0:1:1")) { // If this Label is not 'Shape Label'
		aShape = shapeTool->GetShape(label);
		// If TDataStd_Name is Empty
		if (nameStr.empty()) {
			std::string geomName = node->GetData().GetName(); // Parent Name		
			std::string geomNum = std::to_string(tag);
            // CString geomNum;
			// geomNum.Format(_T("%d"), tag);
			nameStr = geomName + geomNum;
		}		
		// Make this Geometry Structure
		//Geometry geom(tag, nameStr, entryStr);
        Geometry geom(nameStr.c_str());
		//SubGeometry subGeom;
		if (!aShape.IsNull()) {			
			Quantity_Color col(Quantity_NOC_LIGHTGRAY); // Default Color is Light Gray
			//if (aShape.ShapeType() == TopAbs_FACE) {
			//	subGeom.SetShape(aShape);
			//	isSubShape = true;
			//}
			// Color Handling
			if (label.FindAttribute(XCAFDoc::ColorRefGUID(XCAFDoc_ColorSurf), tree)) {
				if (tree->HasFather()) {
					TDF_Label colorLabel = tree->Father()->Label();
					XCAFDoc_ColorType ctype = XCAFDoc_ColorSurf;
					if (colorTool->GetColor(colorLabel, col)) {
						//if(isSubShape) subGeom.SetColor(col);
						//else geom.SetColor(col);
                        geom.SetColor(col);
					}
				}
			}
			if (label.FindAttribute(XCAFDoc::ColorRefGUID(XCAFDoc_ColorGen), tree)) {
				if (tree->HasFather()) {
					TDF_Label colorLabel = tree->Father()->Label();
					// TODO: SOMETHING NEEDED
				}
			}
			if (label.FindAttribute(XCAFDoc::ColorRefGUID(XCAFDoc_ColorCurv), tree)) {
				if (tree->HasFather()) {
					TDF_Label colorLabel = tree->Father()->Label();
					// TODO: SOMETHING NEEDED
				}
			}
			//if (!isSubShape) {
				Handle(AIS_ColoredShape) shape = new AIS_ColoredShape(aShape);
				// Locate this shape by calculated location data
				shape->SetShape(shape->Shape().Located(loc));
				shape->SetColor(col); // Set Color to AIS_Shape
				// Set Line Color by Shape's Color
				Handle(Prs3d_LineAspect) lineAspect = new Prs3d_LineAspect(col, Aspect_TOL_SOLID, 2.0);
				shape->Attributes()->SetFaceBoundaryAspect(lineAspect);
				shape->Attributes()->SetFaceBoundaryDraw(Standard_True);
                geom.SetColor(col);
				geom.SetShape(shape);
			//}			
		}
		//if (!isSubShape) {
            //lastGeometryIndex++; // Index of The Root Geometry Is 0
            //geom.SetIndex(lastGeometryIndex);
			//geometryTree->AddChild(node, geom);
            //TopAbs_ShapeEnum shapeType = geom.GetShape()->Shape().ShapeType();
            //if (shapeType == TopAbs_SOLID || shapeType == TopAbs_FACE) {
                return m_pGeometryTree->InsertItem(geom, node);
            //}
            //else {
            //    return node;
            //}
		//}
		//else {
		//	if (!subGeom.IsEmpty()) {
		//		node->GetData().AddSubGeometry(subGeom); // Add SubGeometry to Parent Node
		//	}
		//}
	}
    return node;
}

TCollection_AsciiString GeometryManager::GetEntryString(const TDF_Label& label)
{
    TCollection_AsciiString entryStr;
    TDF_Tool::Entry(label, entryStr);
    return entryStr;
}

TCollection_AsciiString GeometryManager::GetNameString(const TDF_Label& label)
{
	Handle(TDataStd_Name) name = new TDataStd_Name();
	TCollection_AsciiString nameStr;
	if (label.FindAttribute(TDataStd_Name::GetID(), name))
	{
		nameStr = name->Get();
	}
	return nameStr;    
}

void GeometryManager::PrintIDName(GEOMETRY_NODE node, int depth) {
    TCollection_AsciiString dash;
    for (int i = 0; i < depth; i++) dash += "-";

    TCollection_AsciiString lbr("["), rbr("]");
    TCollection_AsciiString id(node->GetData().GetID());
    TCollection_AsciiString name(node->GetData().GetName().c_str());
    TCollection_AsciiString shType;

    if (node->GetData().HasShape())
    {
        TopAbs_ShapeEnum type = node->GetData().GetShape()->Shape().ShapeType();
        if (type == TopAbs_COMPOUND) shType = ", Compound";
        else if (type == TopAbs_COMPSOLID) shType = ", CompSolid";
        else if (type == TopAbs_SOLID) shType = ", Solid";
        else if (type == TopAbs_SHELL) shType = ", Shell";
        else if (type == TopAbs_FACE) shType = ", Face";
        else if (type == TopAbs_WIRE) shType = ", Wire";
        else if (type == TopAbs_EDGE) shType = ", Edge";
        else if (type == TopAbs_VERTEX) shType = ", Vertex";
    }        

    Message::DefaultMessenger()->Send(dash + name + lbr + id + rbr + shType, Message_Info);
}

void GeometryManager::PrintGeometryIndexMap(GEOMETRY_NODE node, int depth)
{
    Handle(AIS_ColoredShape) aisShape = node->GetData().GetShape();
    const TopTools_IndexedMapOfShape* pFaceMap = node->GetData().GetFaceMap();
    const TopTools_IndexedMapOfShape* pEdgeMap = node->GetData().GetEdgeMap();
    const TopTools_IndexedMapOfShape* pVertexMap = node->GetData().GetVertexMap();

    if (!aisShape.IsNull()) {
        const TopoDS_Shape shape = aisShape->Shape();
        if (!shape.IsNull()) {
            if (shape.ShapeType() == TopAbs_SOLID) {
                for (TopExp_Explorer face_iter(shape, TopAbs_FACE); face_iter.More(); face_iter.Next()) {
                    TopoDS_Face TopoFace = TopoDS::Face(face_iter.Current());
                    Standard_Integer faceIndex = pFaceMap->FindIndex(TopoFace);
                    Message::DefaultMessenger()->Send(TCollection_AsciiString("Face Index: ") + TCollection_AsciiString(faceIndex), Message_Info);

                    for (TopExp_Explorer Edge_It(TopoFace, TopAbs_EDGE); Edge_It.More(); Edge_It.Next()) {
                        const TopoDS_Edge& TopoEdge = TopoDS::Edge(Edge_It.Current());
                        Standard_Integer edgeIndex = pEdgeMap->FindIndex(TopoEdge);
                        Message::DefaultMessenger()->Send(TCollection_AsciiString("- Edge Index: ") + TCollection_AsciiString(edgeIndex), Message_Info);

                        const TopoDS_Vertex& TopoVertex1 = TopExp::FirstVertex(TopoEdge);
                        const TopoDS_Vertex& TopoVertex2 = TopExp::LastVertex(TopoEdge);

                        Standard_Integer Vertex_ID1 = pVertexMap->FindIndex(TopoVertex1);
                        Standard_Integer Vertex_ID2 = pVertexMap->FindIndex(TopoVertex2);

                        Message::DefaultMessenger()->Send(TCollection_AsciiString("-- Vertex1 Index: ") + TCollection_AsciiString(Vertex_ID1), Message_Info);
                        Message::DefaultMessenger()->Send(TCollection_AsciiString("-- Vertex2 Index: ") + TCollection_AsciiString(Vertex_ID2), Message_Info);
                    }
                }
                /*
                auto itFace = pFaceMap->cbegin();
                for (; itFace != pFaceMap->cend(); itFace++) {
                    Standard_Integer faceIndex = pFaceMap->FindIndex(*itFace);
                    Message::DefaultMessenger()->Send(TCollection_AsciiString("Face Index: ") + TCollection_AsciiString(faceIndex), Message_Info);
                }

                auto itEdge = pEdgeMap->cbegin();
                for (; itEdge != pEdgeMap->cend(); itEdge++) {
                    Standard_Integer edgeIndex = pEdgeMap->FindIndex(*itEdge);
                    Message::DefaultMessenger()->Send(TCollection_AsciiString("Edge Index: ") + TCollection_AsciiString(edgeIndex), Message_Info);
                }

                auto itVertex = pVertexMap->cbegin();
                for (; itVertex != pVertexMap->cend(); itVertex++) {
                    Standard_Integer vertexIndex = pVertexMap->FindIndex(*itVertex);
                    Message::DefaultMessenger()->Send(TCollection_AsciiString("Vertex Index: ") + TCollection_AsciiString(vertexIndex), Message_Info);
                }
                */
            }
        }
    }
}

void GeometryManager::PrintAllGeometryName()
{
    m_pGeometryTree->LoopTree(m_pGeometryTree->GetRoot(), GeometryManager::PrintIDName);
    //m_pGeometryTree->LoopTree(m_pGeometryTree->GetRoot(), GeometryManager::PrintGeometryIndexMap);
}

void GeometryManager::DisplayGeometry(GEOMETRY_NODE node, int depth)
{
    Handle(AIS_ColoredShape) shape = node->GetData().GetShape();
    WasmOcctView& viewer = WasmOcctView::Instance();

    if (!shape.IsNull()) {
        const TopoDS_Shape aShape = shape->Shape();
        if (aShape.ShapeType() == TopAbs_SOLID/* || aShape.ShapeType() == TopAbs_SHELL || aShape.ShapeType() == TopAbs_WIRE*/) {
            viewer.Context()->Display(shape, false);
        }
    }
}

void GeometryManager::DisplayAllGeometry()  // Currently display solid only
{
    m_pGeometryTree->LoopTree(m_pGeometryTree->GetRoot(), GeometryManager::DisplayGeometry);
}

void GeometryManager::CreateGeometryIndexMap(GEOMETRY_NODE node, int depth)
{
    Handle(AIS_ColoredShape) shape = node->GetData().GetShape();
    Geometry& geometry = node->GetData();

    if (!shape.IsNull()) {
        const TopoDS_Shape aShape = shape->Shape();
        if (aShape.ShapeType() == TopAbs_SOLID/* || aShape.ShapeType() == TopAbs_SHELL || aShape.ShapeType() == TopAbs_WIRE*/) {
            geometry.CreateIndexedMap();
            //Message::DefaultMessenger()->Send(TCollection_AsciiString("Index map created!"), Message_Info);
        }
    }
}

void GeometryManager::CreateAllGeometryIndexMap()
{
    m_pGeometryTree->LoopTree(m_pGeometryTree->GetRoot(), GeometryManager::CreateGeometryIndexMap);
}

void GeometryManager::SelectVertex(GEOMETRY_NODE node, int depth)
{
    Handle(AIS_ColoredShape) shape = node->GetData().GetShape();
    WasmOcctView& viewer = WasmOcctView::Instance();

    if (!shape.IsNull()) {
        const TopoDS_Shape aShape = shape->Shape();
        if (aShape.ShapeType() == TopAbs_SOLID/* || aShape.ShapeType() == TopAbs_SHELL || aShape.ShapeType() == TopAbs_WIRE*/) {
            viewer.Context()->Deactivate(shape, AIS_Shape::SelectionMode(viewer.GetSelectionMode()));
            viewer.Context()->Activate(shape, AIS_Shape::SelectionMode(TopAbs_VERTEX));
        }
    }
}

void GeometryManager::SelectVertexMode()
{
    m_pGeometryTree->LoopTree(m_pGeometryTree->GetRoot(), GeometryManager::SelectVertex);

    WasmOcctView& viewer = WasmOcctView::Instance();
    viewer.SetSelectionMode(TopAbs_VERTEX);

    Message::DefaultMessenger()->Send(TCollection_AsciiString("VertexMode"), Message_Info);
}

void GeometryManager::SelectEdge(GEOMETRY_NODE node, int depth)
{
    Handle(AIS_ColoredShape) shape = node->GetData().GetShape();
    WasmOcctView& viewer = WasmOcctView::Instance();

    if (!shape.IsNull()) {
        const TopoDS_Shape aShape = shape->Shape();
        if (aShape.ShapeType() == TopAbs_SOLID) {
            viewer.Context()->Deactivate(shape, AIS_Shape::SelectionMode(viewer.GetSelectionMode()));
            viewer.Context()->Activate(shape, AIS_Shape::SelectionMode(TopAbs_EDGE));
        }
    }
}

void GeometryManager::SelectEdgeMode()
{
    m_pGeometryTree->LoopTree(m_pGeometryTree->GetRoot(), GeometryManager::SelectEdge);

    WasmOcctView& viewer = WasmOcctView::Instance();
    viewer.SetSelectionMode(TopAbs_EDGE);

    Message::DefaultMessenger()->Send(TCollection_AsciiString("EdgeMode"), Message_Info);
}

void GeometryManager::SelectFace(GEOMETRY_NODE node, int depth)
{
    Handle(AIS_ColoredShape) shape = node->GetData().GetShape();
    WasmOcctView& viewer = WasmOcctView::Instance();

    if (!shape.IsNull()) {
        const TopoDS_Shape aShape = shape->Shape();
        if (aShape.ShapeType() == TopAbs_SOLID) {
            viewer.Context()->Deactivate(shape, AIS_Shape::SelectionMode(viewer.GetSelectionMode()));
            viewer.Context()->Activate(shape, AIS_Shape::SelectionMode(TopAbs_FACE));
        }
    }
}

void GeometryManager::SelectFaceMode()
{
    m_pGeometryTree->LoopTree(m_pGeometryTree->GetRoot(), GeometryManager::SelectFace);

    WasmOcctView& viewer = WasmOcctView::Instance();
    //viewer.Context()->Deactivate();
    //viewer.Context()->Activate(AIS_Shape::SelectionMode(TopAbs_FACE));
    viewer.SetSelectionMode(TopAbs_FACE);

    Message::DefaultMessenger()->Send(TCollection_AsciiString("FaceMode"), Message_Info);
}

void GeometryManager::SelectSolid(GEOMETRY_NODE node, int depth)
{
    Handle(AIS_ColoredShape) shape = node->GetData().GetShape();
    WasmOcctView& viewer = WasmOcctView::Instance();

    if (!shape.IsNull()) {
        const TopoDS_Shape aShape = shape->Shape();
        if (aShape.ShapeType() == TopAbs_SOLID) {
            viewer.Context()->Deactivate(shape, AIS_Shape::SelectionMode(viewer.GetSelectionMode()));
            viewer.Context()->Activate(shape, AIS_Shape::SelectionMode(TopAbs_SOLID));
        }
    }
}

void GeometryManager::SelectSolidMode()
{
    m_pGeometryTree->LoopTree(m_pGeometryTree->GetRoot(), GeometryManager::SelectSolid);

    WasmOcctView& viewer = WasmOcctView::Instance();
    //viewer.Context()->Deactivate();
    //viewer.Context()->Activate(AIS_Shape::SelectionMode(TopAbs_SOLID));
    viewer.SetSelectionMode(TopAbs_SOLID);

    Message::DefaultMessenger()->Send(TCollection_AsciiString("SolidMode"), Message_Info);
}