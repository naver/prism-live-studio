/*************************************************************************
 * This file is part of spectralizer
 * github.con/univrsal/spectralizer
 * Copyright 2020 univrsal <universailp@web.de>.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *************************************************************************/

#pragma once

#include <obs-module.h>
#include <vector>

/* clang-format off */

#define UTIL_SWAP(a, b) do { typeof(a) tmp = a; a = b; b = tmp; } while (0)
#define UTIL_MAX(a, b)                  (((a) > (b)) ? (a) : (b))
#define UTIL_MIN(a, b)                  (((a) < (b)) ? (a) : (b))
#define UTIL_CLAMP(lower, x, upper) 	(UTIL_MIN(upper, UTIL_MAX(x, lower)))
#define T_(v)                           obs_module_text(v)

#define T_SOURCE                        T_("Spectralizer.Source")
#define T_MODE_BARS_BASIC               T_("Spectralizer.Mode.BasicBars")
#define T_MODE_BARS_FILLET              T_("Spectralizer.Mode.FilletBars")
#define T_MODE_BARS_LINEAR              T_("Spectralizer.Mode.LinearBars")
#define T_MODE_BARS_GRADIENT            T_("Spectralizer.Mode.GradientBars")
#define T_STEREO                        T_("Spectralizer.Stereo")
#define T_STEREO_SPACE			T_("Spectralizer.Stereo.Space")
#define T_DETAIL                        T_("Spectralizer.Detail")
#define T_AUDIO_SOURCE                  T_("Spectralizer.AudioSource")
#define T_AUDIO_SOURCE_NONE             T_("Spectralizer.AudioSource.None")
#define T_BAR_SETTINGS                  T_("Spectralizer.Bar.Settings")
#define T_BAR_WIDTH                     T_("Spectralizer.Bar.Width")
#define T_BAR_HEIGHT                    T_("Spectralizer.Bar.Height")
#define T_BAR_SPACING                   T_("Spectralizer.Bar.Space")
#define T_COLOR                         T_("Spectralizer.Color")
#define T_SOLID_COLOR			T_COLOR
#define T_GRADIENT_COLOR		T_COLOR
#define T_GRAVITY                       T_("Spectralizer.Gravity")
#define T_FALLOFF			T_("Spectralizer.Falloff")
#define T_FILTER_MODE                   T_("Spectralizer.Filter.Mode")
#define T_FILTER_NONE                  	T_AUDIO_SOURCE_NONE
#define T_FILTER_MONSTERCAT             T_("Spectralizer.Filter.Monstercat")
#define T_FILTER_SGS			T_("Spectralizer.Filter.SGS")
#define T_SGS_PASSES			T_("Spectralizer.Filter.SGS.Passes")
#define T_SGS_POINTS			T_("Spectralizer.Filter.SGS.Points")
#define T_FILTER_STRENGTH    		T_("Spectralizer.Filter.Strength")
#define T_AUTO_SCALE			T_("Spectralizer.Use.AutoScale")
#define T_SCALE_BOOST			T_("Spectralizer.Scale.Boost")
#define T_SCALE_SIZE			T_("Spectralizer.Scale.Size")

constexpr auto S_TAB			= "tab";
constexpr auto S_TEMPLATE_LIST          = "visualizer_template_list";
constexpr auto S_STEREO                 = "stereo";
constexpr auto S_STEREO_SPACE		= "stereo_space";
constexpr auto S_DETAIL                 = "detail";
constexpr auto S_REFRESH_RATE           = "refresh_rate";
constexpr auto S_AUDIO_SOURCE           = "audio_source";
constexpr auto S_BAR_SETTINGS           = "bar_settings";
constexpr auto S_BAR_WIDTH              = "width";
constexpr auto S_BAR_HEIGHT             = "height";
constexpr auto S_SAMPLE_RATE            = "sample_rate";
constexpr auto S_BAR_SPACE              = "bar_space";
constexpr auto S_FILTER_MODE            = "filter_mode";
constexpr auto S_SGS_PASSES		= "sgs_passes";
constexpr auto S_SGS_POINTS		= "sgs_points";
constexpr auto S_GRAVITY                = "gravity";
constexpr auto S_FALLOFF		= "falloff";
constexpr auto S_FILTER_STRENGTH	= "filter_strength";
constexpr auto S_AUTO_CLEAR  		= "auto_clear";
constexpr auto S_AUTO_SCALE		= "use_auto_scale";
constexpr auto S_SCALE_BOOST		= "scale_boost";
constexpr auto S_SCALE_SIZE		= "scale_size";
constexpr auto S_H_LINE_0               = "h_line_0";
constexpr auto S_H_LINE_1               = "h_line_1";
constexpr auto S_H_LINE_2               = "h_line_2";

constexpr auto S_BAR_MODE		       = "bar_mode";
constexpr auto S_COLOR                         = "color";
constexpr auto S_SOLID_COLOR		       = "solid_color";
constexpr auto S_GRADIENT_COLOR		       = "gradation_color";

constexpr auto S_SOLID_COLOR_0                 = "solid_color_0";
constexpr auto S_SOLID_COLOR_1                 = "solid_color_1";
constexpr auto S_SOLID_COLOR_2                 = "solid_color_2";
constexpr auto S_SOLID_COLOR_3                 = "solid_color_3";
constexpr auto S_SOLID_COLOR_4                 = "solid_color_4";
constexpr auto S_SOLID_COLOR_5                 = "solid_color_5";
constexpr auto S_SOLID_COLOR_6                 = "solid_color_6";
constexpr auto S_SOLID_COLOR_7                 = "solid_color_7";
constexpr auto S_SOLID_COLOR_8                 = "solid_color_8";
constexpr auto S_SOLID_COLOR_9                 = "solid_color_9";
constexpr auto S_SOLID_COLOR_10                = "solid_color_10";
constexpr auto S_SOLID_COLOR_11                = "solid_color_11";
constexpr auto S_SOLID_COLOR_12                = "solid_color_12";

constexpr auto S_GRADIENT_COLOR_0              = "dradient_color_0";
constexpr auto S_GRADIENT_COLOR_1              = "dradient_color_1";
constexpr auto S_GRADIENT_COLOR_2              = "dradient_color_2";
constexpr auto S_GRADIENT_COLOR_3              = "dradient_color_3";
constexpr auto S_GRADIENT_COLOR_4              = "dradient_color_4";

constexpr auto P_BASIC_BARS			    = ":/prism-spectralizer/images/img-audiov-template-1@3x.png";
constexpr auto P_FILLET_BARS			= ":/prism-spectralizer/images/img-audiov-template-2@3x.png";
constexpr auto P_LINEAR_BARS			= ":/prism-spectralizer/images/img-audiov-template-3@3x.png";
constexpr auto P_GRADIENT_BARS			= ":/prism-spectralizer/images/img-audiov-template-4@3x.png";

constexpr auto P_SOLID_COLOR_0			= ":/prism-spectralizer/images/img-audiov-color-01.svg";
constexpr auto P_SOLID_COLOR_1			= ":/prism-spectralizer/images/img-audiov-color-03.svg";
constexpr auto P_SOLID_COLOR_2			= ":/prism-spectralizer/images/img-audiov-color-04.svg";
constexpr auto P_SOLID_COLOR_3			= ":/prism-spectralizer/images/img-audiov-color-05.svg";
constexpr auto P_SOLID_COLOR_4			= ":/prism-spectralizer/images/img-audiov-color-06.svg";
constexpr auto P_SOLID_COLOR_5			= ":/prism-spectralizer/images/img-audiov-color-07.svg";
constexpr auto P_SOLID_COLOR_6			= ":/prism-spectralizer/images/img-audiov-color-08.svg";
constexpr auto P_SOLID_COLOR_7			= ":/prism-spectralizer/images/img-audiov-color-09.svg";
constexpr auto P_SOLID_COLOR_8			= ":/prism-spectralizer/images/img-audiov-color-10.svg";
constexpr auto P_SOLID_COLOR_9			= ":/prism-spectralizer/images/img-audiov-color-11.svg";

constexpr auto P_SOLID_COLOR_10			= ":/prism-spectralizer/images/img-audiov-color-12.svg";
constexpr auto P_SOLID_COLOR_11			= ":/prism-spectralizer/images/img-audiov-color-13.svg";
constexpr auto P_SOLID_COLOR_12			= ":/prism-spectralizer/images/img-audiov-color-02.svg";

constexpr auto P_GRADIENT_COLOR_0		= ":/prism-spectralizer/images/img_audiov_gradation_1.png";
constexpr auto P_GRADIENT_COLOR_1		= ":/prism-spectralizer/images/img_audiov_gradation_2.png";
constexpr auto P_GRADIENT_COLOR_2		= ":/prism-spectralizer/images/img_audiov_gradation_3.png";
constexpr auto P_GRADIENT_COLOR_3		= ":/prism-spectralizer/images/img_audiov_gradation_4.png";
constexpr auto P_GRADIENT_COLOR_4		= ":/prism-spectralizer/images/img_audiov_gradation_5.png";

constexpr auto P_GRADIENT_MODE_0		= "images/img-avl-gradation-1.png";
constexpr auto P_GRADIENT_MODE_1		= "images/img-avl-gradation-2.png";
constexpr auto P_GRADIENT_MODE_2		= "images/img-avl-gradation-3.png";
constexpr auto P_GRADIENT_MODE_3		= "images/img-avl-gradation-4.png";
constexpr auto P_GRADIENT_MODE_4		= "images/img-avl-gradation-5.png";

#define COLOR_(r, g, b, a) color_to_int(r, g, b, a)

#define C_SOLID_COLOR_0		COLOR_(255, 255, 255, 255)  //*#ffffff*/
#define C_SOLID_COLOR_1		COLOR_(6, 247, 216, 255)    //*#06f7d8*/
#define C_SOLID_COLOR_2		COLOR_(1, 165, 247, 255)    //*#01a5f7*/
#define C_SOLID_COLOR_3		COLOR_(60, 95, 255, 255)    //*#3c5fff*/
#define C_SOLID_COLOR_4		COLOR_(105, 64, 252, 255)   //*#6940fc*/
#define C_SOLID_COLOR_5		COLOR_(93, 255, 93, 255)    //*#5dff5d*/
#define C_SOLID_COLOR_6		COLOR_(239, 252, 53, 255)   //*#effc35*/
#define C_SOLID_COLOR_7		COLOR_(254, 154, 25, 255)   //*#fe9a19*/
#define C_SOLID_COLOR_8		COLOR_(255, 88, 0, 255)     //*#ff5800*/
#define C_SOLID_COLOR_9 	COLOR_(246, 19, 101, 255)   //*#f61365*/
#define C_SOLID_COLOR_10	COLOR_(252, 64, 172, 255)   //*#fc40ac*/
#define C_SOLID_COLOR_11	COLOR_(219, 6, 146, 255)    //*#db06f6*/
#define C_SOLID_COLOR_12	COLOR_(0, 0, 0, 255)	    //*#000000*/


constexpr auto BAR_MIN_WIDTH	= 1;
constexpr auto BAR_MAX_WIDTH	= 30;
constexpr auto BAR_MIN_HEIGHT  = 10;
constexpr auto BAR_MAX_HEIGHT  = 500;
constexpr auto BAR_MIN_SPACE   = 0;
constexpr auto BAR_BASIC_MAX_SPACE   = 50;
constexpr auto BAR_OTHER_MAX_SPACE   = 30;
constexpr auto BAR_MIN_GRAPH_NUM   = 1;
constexpr auto BAR_MAX_GRAPH_NUM   = 180;

enum class visual_mode
{
    VM_BASIC_BARS = 0,
    VM_FILLET_BARS,
    VM_LINEAR_BARS,
    VM_GRADIENT_BARS
};

enum class bar_mode
{
    BM_BASIC, BM_FILLET, BM_LINEAR, BM_GRADIENT
};

enum class smooting_mode
{
    SM_NONE = 0,
    SM_MONSTERCAT,
    SM_SGS
};

enum class falloff
{
    FO_NONE = 0,
    FO_FILL,
    FO_TOP
};

enum class channel_mode
{
    CM_LEFT = 0,
    CM_RIGHT,
    CM_BOTH
};

enum class solid_color {
	SOLID_COLOR_0 = 0,
	SOLID_COLOR_1,
	SOLID_COLOR_2,
	SOLID_COLOR_3,
	SOLID_COLOR_4,
	SOLID_COLOR_5,
	SOLID_COLOR_6,
	SOLID_COLOR_7,
	SOLID_COLOR_8,
	SOLID_COLOR_9,
	SOLID_COLOR_10,
	SOLID_COLOR_11,
	SOLID_COLOR_12,
};

enum class gradient_color {
	GRADIENT_COLOR_0 = 0,
	GRADIENT_COLOR_1,
	GRADIENT_COLOR_2,
	GRADIENT_COLOR_3,
	GRADIENT_COLOR_4,
};

struct stereo_sample_frame
{
    int16_t l;
    int16_t r;
};

using pcm_stereo_sample = struct stereo_sample_frame;
#define CNST			static const constexpr

namespace defaults {
    CNST bool				stereo			= false;

    CNST uint16_t			cx			= 500;
    CNST uint16_t			cy			= 500;
    CNST uint16_t			margin			= 20;
    CNST uint16_t			fps			= 30;

    CNST uint32_t			sample_rate		= 44100;
    CNST uint32_t			sample_size 		= sample_rate / fps;

    CNST double				lfreq_cut		= 30;
    CNST double				hfreq_cut		= 22050;
    CNST double				falloff_weight		= 95;
    CNST double				gravity			= 80;

    CNST uint32_t			sgs_points		= 3;		/* Should be a odd number */
    CNST uint32_t			sgs_passes		= 2;

    CNST double				mcat_smooth		= 1.5;

    // basic bar
    CNST uint16_t			basic_width		= 8;
    CNST uint16_t			basic_height		= 130;
    CNST uint16_t			basic_space		= 4;
    CNST uint16_t			basic_detail		= 40;
    CNST uint16_t			basic_min_height	= 2;

    CNST uint32_t			basic_color		= 0xffffffff;

    // fillet bar		
    CNST uint16_t			fillet_width		= 5;
    CNST uint16_t			fillet_height		= 40;
    CNST uint16_t			fillet_space		= 3;
    CNST uint16_t			fillet_detail		= 57;
    CNST uint16_t			fillet_min_height	= fillet_width;

    CNST uint32_t			fillet_color		= 0xffffffff;

    // linear bar
    CNST uint16_t			linear_width		= 2;
    CNST uint16_t			linear_height		= 150;
    CNST uint16_t			linear_space		= 13;
    CNST uint16_t			linear_detail		= 33;
    CNST uint16_t			linear_min_height	= 2;

    CNST uint32_t			linear_color		= 0xffffffff;

    // gradient bar
    CNST uint16_t			gradient_width		= 11;
    CNST uint16_t			gradient_height		= 40;
    CNST uint16_t			gradient_space		= 3;
    CNST uint16_t			gradient_detail		= 32;
    CNST uint16_t			gradient_min_height	= 2;

    CNST uint32_t			gradient_color		= 0xffffffff;

    CNST char				*audio_source		= "";

    CNST bool				use_auto_scale		= true;
    CNST double				scale_boost		= 0.0;
    CNST double				scale_size		= 1.0;

};

namespace constants {
    CNST int auto_scale_span 			= 30;
    CNST double auto_scaling_reset_window	= 0.1;
    CNST double auto_scaling_erase_percent 	= 0.75;
    /* Amount of deviation needed between short term and long
     * term moving max height averages to trigger an autoscaling reset */
    CNST double deviation_amount_to_reset 	= 1.0;
}

static inline long long color_to_int(float r, float g, float b, float a)
{
	auto shift = [&](unsigned val, int _shift) { return ((val & 0xff) << _shift); };
	return shift((unsigned int)r, 0) | shift((unsigned int)g, 8) | shift((unsigned int)b, 16) | shift((unsigned int)a, 24);
}

/* clang-format on */
