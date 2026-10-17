#include <QCoreApplication>
#include <QVariantList>
#include "part/part.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    QVariantList args;
    Okular::Part part(nullptr, args);

    // Test initial state
    Q_ASSERT(part.m_saveDepth == 0);
    Q_ASSERT(part.canAutoSaveNow() == true); // Should be able to auto-save initially

    // Test saveDepth increment/decrement
    part.m_saveDepth++;
    Q_ASSERT(part.m_saveDepth == 1);
    Q_ASSERT(part.canAutoSaveNow() == false); // Should not be able to auto-save

    part.m_saveDepth--;
    Q_ASSERT(part.m_saveDepth == 0);
    Q_ASSERT(part.canAutoSaveNow() == true); // Should be able to auto-save again

    return 0;
}