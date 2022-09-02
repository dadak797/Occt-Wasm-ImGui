#pragma once

#include <Standard_Type.hxx>
#include <Quantity_Color.hxx>
#include <TopTools_IndexedMapOfShape.hxx>

#include <string>

class AIS_ColoredShape;


class Geometry
{
public:
    // Constructors
    Geometry(const char* name);
    Geometry(const char* name, Handle(AIS_ColoredShape) shape);
    Geometry(const Geometry& other);  // Copy Constructor
    Geometry(Geometry&& other);  // Move Constructor

    // Destructors
    ~Geometry();

    // Operators
    Geometry& operator=(const Geometry& other);  // Copy Operator
    Geometry& operator=(Geometry&& other);  // Move Operator
    
    // Getters
    int GetID() const { return m_ID; }
    std::string GetName() const { return m_Name; }
    Quantity_Color GetColor() const { return m_Color; }
    Handle(AIS_ColoredShape) GetShape() const { return m_AISShape; }
    bool HasShape() const;
    const TopTools_IndexedMapOfShape* GetVertexMap() const { return m_pVertexMap; }
    const TopTools_IndexedMapOfShape* GetEdgeMap() const { return m_pEdgeMap; }
    const TopTools_IndexedMapOfShape* GetFaceMap() const { return m_pFaceMap; }

    // Setters
    void SetColor(Quantity_Color color);
    void SetShape(Handle(AIS_ColoredShape) shape);

    void CreateIndexedMap();

private:
    int m_ID;
    std::string m_Name;
    Handle(AIS_ColoredShape) m_AISShape;
    Quantity_Color m_Color;
    TopTools_IndexedMapOfShape* m_pVertexMap { nullptr };
    TopTools_IndexedMapOfShape* m_pEdgeMap { nullptr };
    TopTools_IndexedMapOfShape* m_pFaceMap { nullptr };

    static int s_LastID;
};