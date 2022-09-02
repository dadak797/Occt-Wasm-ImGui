#pragma once

#include "LCRSNode.hpp"

#include <iostream>


template<typename T>
class LCRSTree {
public:
    // Constructor
    LCRSTree() : m_RootNode(nullptr) {}

    // Destructor
    ~LCRSTree() {
        if (m_RootNode == nullptr) return;
        delete m_RootNode;
        m_RootNode = nullptr;
    }

    LCRSNode<T>* GetRoot() const {
        return m_RootNode;
    }

    // If insertAfter is nullptr, data is set at root node.
    LCRSNode<T>* InsertItem(T data, LCRSNode<T>* insertAfter = nullptr) {
        if (!insertAfter) {
            return SetRoot(data);
        }

        LCRSNode<T>* newNode = new LCRSNode<T>(data);
        LCRSNode<T>* childNode = insertAfter->GetChild();
        if (childNode) {
            while (childNode->GetSibling()) {
                childNode = childNode->GetSibling();
            }
            childNode->SetSibling(newNode);
        }
        else {
            insertAfter->SetChild(newNode);
        }

        return newNode;
    }

    void LoopTree(LCRSNode<T>* node, void (*pFunc)(LCRSNode<T>*, int), int depth = 0)
    {
        if (pFunc) {
            pFunc(node, depth);
        }

        if (node->GetChild()) {
            LoopTree(node->GetChild(), pFunc, depth + 1);
        }

        if (node->GetSibling()) {
            LoopTree(node->GetSibling(), pFunc, depth);
        }
    }

private:
    LCRSNode<T>* m_RootNode;

    LCRSNode<T>* SetRoot(T data) {
        if (m_RootNode) delete m_RootNode;
        m_RootNode = new LCRSNode<T>(data);
        return m_RootNode;
    }
};