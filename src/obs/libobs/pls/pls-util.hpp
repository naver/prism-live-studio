#pragma once
#include <functional>

class CAutoRunWhenSecEnd {
public:
	CAutoRunWhenSecEnd(std::function<void()> f) : func(f) {}
	virtual ~CAutoRunWhenSecEnd() { func(); }

private:
	std::function<void()> func;
};

#define COMBINE2(a, b) a##b
#define COMBINE1(a, b) COMBINE2(a, b)

#define RUN_WHEN_SECTION_END(lambda) \
	CAutoRunWhenSecEnd COMBINE1(autoRunVariable, __LINE__)(lambda);
