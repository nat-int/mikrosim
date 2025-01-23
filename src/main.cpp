#include <stdlib.h>
#include "app.hpp"

int main() {
	srand(u32(time(nullptr)));
	mikrosim_window w;
	w.loop();
	return 0;
}

