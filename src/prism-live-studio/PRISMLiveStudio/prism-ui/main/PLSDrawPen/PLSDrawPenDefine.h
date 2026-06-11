#pragma once
#include <vector>
#include <string>

//prism point(int) for line
struct PointI {
	int x;
	int y;
};

//prism point(float) for line
struct PointF {
	float x;
	float y;
};

//prism vector(float) for line
struct VectorF {
	float x;
	float y;
};

using PointFPtrs = std::vector<PointF *>;
using PointFs = std::vector<PointF>;
using PointFsPointer = std::vector<PointF> *;
using IntPtrs = std::vector<int *>;

enum class ShapeType { ST_STRAIGHT_ARROW = 0, ST_LINE, ST_RECTANGLE, ST_ROUND, TRIANGLE };
enum class DrawType { DT_PEN, DT_HIGHLIGHTER, DT_GLOW_PEN, DT_2DSHAPE, DT_RUBBER };

constexpr auto LINE_0 = 4;
constexpr auto LINE_1 = 8;
constexpr auto LINE_2 = 10;
constexpr auto LINE_3 = 14;
constexpr auto LINE_4 = 18;

#define COLOR_(r, g, b, a) color_to_int(r, g, b, a)

#define C_SOLID_COLOR_0 COLOR_(6, 247, 216, 255)   //*#06f7d8*/
#define C_SOLID_COLOR_1 COLOR_(1, 165, 247, 255)   //*#01a5f7*/
#define C_SOLID_COLOR_2 COLOR_(60, 95, 255, 255)   //*#3c5fff*/
#define C_SOLID_COLOR_3 COLOR_(111, 217, 111, 255) //*#6fd96f*/
#define C_SOLID_COLOR_4 COLOR_(0, 0, 0, 255)       //*#000000*/
#define C_SOLID_COLOR_5 COLOR_(255, 255, 255, 255) //*#ffffff*/
#define C_SOLID_COLOR_6 COLOR_(255, 77, 77, 255)   //*#ff4d4d*/
#define C_SOLID_COLOR_7 COLOR_(246, 19, 101, 255)  //*#f61365*/
#define C_SOLID_COLOR_8 COLOR_(252, 64, 172, 255)  //*#fc40ac*/
#define C_SOLID_COLOR_9 COLOR_(219, 6, 246, 255)   //*#db06f6*/
#define C_SOLID_COLOR_10 COLOR_(254, 241, 61, 255) //*#fef13d*/
#define C_SOLID_COLOR_11 COLOR_(252, 173, 37, 255) //*#fcad25*/

static inline uint32_t color_to_int(float r, float g, float b, float a)
{
	auto shift = [&](unsigned val, int _shift) { return ((val & 0xff) << _shift); };
	return shift((unsigned int)r, 0) | shift((unsigned int)g, 8) | shift((unsigned int)b, 16) | shift((unsigned int)a, 24);
}

static const std::vector<int> lineWidth{LINE_0, LINE_1, LINE_2, LINE_3, LINE_4};
static const std::vector<uint32_t> colors{C_SOLID_COLOR_0, C_SOLID_COLOR_1, C_SOLID_COLOR_2, C_SOLID_COLOR_3, C_SOLID_COLOR_4,  C_SOLID_COLOR_5,
					  C_SOLID_COLOR_6, C_SOLID_COLOR_7, C_SOLID_COLOR_8, C_SOLID_COLOR_9, C_SOLID_COLOR_10, C_SOLID_COLOR_11};

static inline void colorf_from_rgba(float &r, float &g, float &b, float &a, uint32_t rgba)
{
	r = (float)((double)(rgba & 0xFF) * (1.0 / 255.0));
	rgba >>= 8;
	g = (float)((double)(rgba & 0xFF) * (1.0 / 255.0));
	rgba >>= 8;
	b = (float)((double)(rgba & 0xFF) * (1.0 / 255.0));
	rgba >>= 8;
	a = (float)((double)(rgba & 0xFF) * (1.0 / 255.0));
}

static inline void colorI_from_rgba(int &r, int &g, int &b, int &a, uint32_t rgba)
{
	r = (int)((double)(rgba & 0xFF));
	rgba >>= 8;
	g = (int)((double)(rgba & 0xFF));
	rgba >>= 8;
	b = (int)((double)(rgba & 0xFF));
	rgba >>= 8;
	a = (int)((double)(rgba & 0xFF));
}
