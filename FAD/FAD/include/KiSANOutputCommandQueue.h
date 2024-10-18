#ifndef KISANOUTPUTCOMMANDQUEUE_H_
#define KISANOUTPUTCOMMANDQUEUE_H_

#include "KiSANOutputCommand.h"

class KiSANOutputCommandQueue
{
private:
    KiSANOutputCommand *_kisanOutputCommands;
    int _queueSize;
    int _kisanOutputCommandCount;

public:
    KiSANOutputCommandQueue(int queueSize);
    ~KiSANOutputCommandQueue();

public:
    bool Enqueue(KiSANOutputCommand* kisanOutputCommand);
    KiSANOutputCommand* Peek();
    KiSANOutputCommand* Dequeue();
    int Count();
};

#endif
