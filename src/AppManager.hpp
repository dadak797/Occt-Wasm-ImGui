#include <iostream>

class GeometryManager;


enum class GeomFileType
{
    BREP,
    STEP
};

class AppManager
{
public:
    static AppManager& GetInstance();
    ~AppManager();

    void ImportGeometry(const char* fileName, std::istream& istream, GeomFileType fileType);
    
    void SelectVertexMode();
    void SelectEdgeMode();
    void SelectFaceMode();
    void SelectSolidMode();

private:
    AppManager();
    AppManager(const AppManager& other) = delete;
    AppManager& operator=(const AppManager& other) = delete;

    GeometryManager* m_pGeometryManager;
};