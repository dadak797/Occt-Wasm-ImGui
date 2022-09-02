#include "AppManager.hpp"
#include "GeometryManager.hpp"


AppManager& AppManager::GetInstance()
{
    static AppManager s_Instance;
    return s_Instance;
}

AppManager::AppManager()
{
    m_pGeometryManager = new GeometryManager();
}

AppManager::~AppManager()
{
    if(m_pGeometryManager) {
        delete m_pGeometryManager;
        m_pGeometryManager = nullptr;
    }
}

void AppManager::ImportGeometry(const char* fileName, std::istream& istream, GeomFileType fileType)
{
    if (fileType == GeomFileType::BREP) {

    }
    else if (fileType == GeomFileType::STEP) {
        m_pGeometryManager->ImportStepFile(fileName, istream);
        m_pGeometryManager->DisplayAllGeometry();
        m_pGeometryManager->CreateAllGeometryIndexMap();
        m_pGeometryManager->PrintAllGeometryName();
    }
}

void AppManager::SelectVertexMode()
{
    m_pGeometryManager->SelectVertexMode();
}

void AppManager::SelectEdgeMode()
{
    m_pGeometryManager->SelectEdgeMode();
}

void AppManager::SelectFaceMode()
{
    m_pGeometryManager->SelectFaceMode();
}

void AppManager::SelectSolidMode()
{
    m_pGeometryManager->SelectSolidMode();
}