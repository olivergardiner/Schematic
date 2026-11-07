#ifndef TST_GROUPTRANSFORMMODEL_H
#define TST_GROUPTRANSFORMMODEL_H
#include <QObject>

class TstGroupTransformModel : public QObject
{
    Q_OBJECT
private slots:
    void rigidInternalRoutesAndOrder();
    void branchesMoveOrAnchorByIdentity();
    void boundaryWiresAndAtomicRejection();
    void quadrantCompositionAndRoundTrip();
    void invalidRequestsDoNotMutate();
};
#endif
