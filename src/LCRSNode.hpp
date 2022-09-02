template<typename T>
class LCRSNode {
public:
    // Constructor
    LCRSNode() = delete;

    LCRSNode(T data)
        : m_Data(data), m_Child(nullptr), m_Sibling(nullptr) {}

    // Destructor
    ~LCRSNode() {
        DeleteNode(m_Child);
        DeleteNode(m_Sibling);
    }

    T& GetData() {
        return m_Data;
    }

    const T& GetData() const {
        return m_Data;
    }

    void SetChild(LCRSNode* child) {
        m_Child = child;
    }

    LCRSNode* GetChild() const {
        return m_Child;
    }

    void SetSibling(LCRSNode* sibling) {
        m_Sibling = sibling;
    }

    LCRSNode* GetSibling() const {
        return m_Sibling;
    }

private:
    T m_Data;
    LCRSNode* m_Child;
    LCRSNode* m_Sibling;

    void DeleteNode(LCRSNode* node) {
        if (node == nullptr) return;
        delete node;
        node = nullptr;
    }
};