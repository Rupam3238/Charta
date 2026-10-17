#include <iostream>

class TestPart {
public:
    TestPart() : m_saveDepth(0), m_autoSaveInProgress(false) {}

    bool canAutoSaveNow() const {
        return !(m_autoSaveInProgress || m_saveDepth > 0);
    }

    void saveAs() {
        ++m_saveDepth;
        // Simulate QScopeGuard behavior
        struct SaveDepthGuard {
            TestPart* part;
            ~SaveDepthGuard() { --part->m_saveDepth; }
        } guard{this};

        // Simulate save operation
        std::cout << "Saving... m_saveDepth = " << m_saveDepth << std::endl;
        // guard goes out of scope here, calling decrement
    }

    int m_saveDepth;
    bool m_autoSaveInProgress;
};

int main() {
    TestPart part;

    std::cout << "Initial m_saveDepth: " << part.m_saveDepth << std::endl;
    std::cout << "Initial canAutoSaveNow: " << (part.canAutoSaveNow() ? "true" : "false") << std::endl;

    part.saveAs();

    std::cout << "After saveAs m_saveDepth: " << part.m_saveDepth << std::endl;
    std::cout << "After saveAs canAutoSaveNow: " << (part.canAutoSaveNow() ? "true" : "false") << std::endl;

    return 0;
}