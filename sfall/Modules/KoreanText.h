/*
 *    sfall
 *    Korean CP949 text renderer experiment
 */

#pragma once

#include "Module.h"

namespace sfall
{

class KoreanText : public Module {
public:
	const char* name() { return "KoreanText"; }
	void init();
	void exit() override;
};

}
