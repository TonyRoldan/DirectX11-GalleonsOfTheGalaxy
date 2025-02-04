// handles everything
#include "Application.h"
// program entry point
int main()
{
	Application galleonsOfTheGalaxy;
	if (galleonsOfTheGalaxy.Init()) {
		if (galleonsOfTheGalaxy.Run()) {
			return galleonsOfTheGalaxy.Shutdown() ? 0 : 1;
		}
	}
	return 1;
}