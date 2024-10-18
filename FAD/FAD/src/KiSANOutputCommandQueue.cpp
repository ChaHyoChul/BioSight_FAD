#include "KiSANOutputCommandQueue.h"

KiSANOutputCommandQueue::KiSANOutputCommandQueue(int queueSize)
{
}

KiSANOutputCommandQueue::~KiSANOutputCommandQueue()
{
    delete [] _kisanOutputCommands;
}

bool KiSANOutputCommandQueue::Enqueue(KiSANOutputCommand* kisanOutputCommand)
{
    if(_kisanOutputCommandCount > _queueSize)
    {
        return false;
    }



    return true;
}

KiSANOutputCommand* KiSANOutputCommandQueue::Peek()
{

}

KiSANOutputCommand* KiSANOutputCommandQueue::Dequeue()
{

}

int KiSANOutputCommandQueue::Count()
{
    return _kisanOutputCommandCount;
}
