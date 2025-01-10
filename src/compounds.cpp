#include "compounds.hpp"


f32 &compounds::at(usize compound, usize particle) {
	return concentrations[compound][particle];
}

