#include "PLSDrawPenDefine.h"
#include <assert.h>

static const std::vector<int> lineWidthNormal{
	4, 8, 10, 14, 18,
};

static const std::vector<int> lineWidthHighlight{
	4, 10, 16, 22, 30,
};

int get_line_width(int index, DrawType type)
{
	if (DrawType::DT_HIGHLIGHTER == type) {
		if (index < 0 || index >= lineWidthHighlight.size()) {
			assert(false);
			index = DEFAULT_LINE_WITDH_INDEX;
		}
		return lineWidthHighlight.at(index);

	} else {
		if (index < 0 || index >= lineWidthNormal.size()) {
			assert(false);
			index = DEFAULT_LINE_WITDH_INDEX;
		}
		return lineWidthNormal.at(index);
	}
}
