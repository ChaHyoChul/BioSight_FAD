#include "Application.h"

int main()
{
    Application::Initialize();

    for(;;)
    {
        Application::RunCycle();
    }
}
