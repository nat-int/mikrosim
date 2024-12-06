#include "2d.hpp"

#include "../util.hpp"
#include "context.hpp"
#include "pipeline.hpp"

namespace rend {
/*[INJ_MARK](shaders2d){*/
constexpr const char *shader_code[] = {
	"\3\2#\7\0\0\1\0\v\0\15\0'\0\0\0\0\0\0\0\21\0\2\0\1\0\0\0\v\0\6\0\1\0\0\0GLSL.std.450\0\0\0\0\16\0\3\0\0\0\0\0\1\0\0\0\17\0\10\0\4\0\0\0\4\0\0\0main\0\0\0\0\t\0\0"
	"\0\v\0\0\0\21\0\0\0\20\0\3\0\4\0\0\0\7\0\0\0\3\0\3\0\2\0\0\0\302\1\0\0\4\0\n\0GL_GOOGLE_cpp_style_line_directive\0\0\4\0\10\0GL_GOOGLE_include_directive\0\5\0\4"
	"\0\4\0\0\0main\0\0\0\0\5\0\3\0\t\0\0\0oc\0\0\5\0\3\0\v\0\0\0col\0\5\0\3\0\17\0\0\0p\0\0\0\5\0\3\0\21\0\0\0vp\0\0G\0\4\0\t\0\0\0\36\0\0\0\0\0\0\0G\0\4\0\v\0\0\0\36"
	"\0\0\0\1\0\0\0G\0\4\0\21\0\0\0\36\0\0\0\0\0\0\0\23\0\2\0\2\0\0\0!\0\3\0\3\0\0\0\2\0\0\0\26\0\3\0\6\0\0\0 \0\0\0\27\0\4\0\7\0\0\0\6\0\0\0\4\0\0\0 \0\4\0\10\0\0\0"
	"\3\0\0\0\7\0\0\0;\0\4\0\10\0\0\0\t\0\0\0\3\0\0\0 \0\4\0\n\0\0\0\1\0\0\0\7\0\0\0;\0\4\0\n\0\0\0\v\0\0\0\1\0\0\0\27\0\4\0\15\0\0\0\6\0\0\0\2\0\0\0 \0\4\0\16\0\0\0"
	"\7\0\0\0\15\0\0\0 \0\4\0\20\0\0\0\1\0\0\0\15\0\0\0;\0\4\0\20\0\0\0\21\0\0\0\1\0\0\0+\0\4\0\6\0\0\0\23\0\0\0\0\0\0@,\0\5\0\15\0\0\0\24\0\0\0\23\0\0\0\23\0\0\0+\0"
	"\4\0\6\0\0\0\26\0\0\0\0\0\200?,\0\5\0\15\0\0\0\27\0\0\0\26\0\0\0\26\0\0\0+\0\4\0\6\0\0\0\31\0\0\0\0\0\0\0+\0\4\0\6\0\0\0\32\0\0\0\n\327#<\25\0\4\0 \0\0\0 \0\0\0"
	"\0\0\0\0+\0\4\0 \0\0\0!\0\0\0\3\0\0\0 \0\4\0\"\0\0\0\3\0\0\0\6\0\0\0\66\0\5\0\2\0\0\0\4\0\0\0\0\0\0\0\3\0\0\0\370\0\2\0\5\0\0\0;\0\4\0\16\0\0\0\17\0\0\0\7\0\0\0"
	"=\0\4\0\7\0\0\0\f\0\0\0\v\0\0\0>\0\3\0\t\0\0\0\f\0\0\0=\0\4\0\15\0\0\0\22\0\0\0\21\0\0\0\205\0\5\0\15\0\0\0\25\0\0\0\22\0\0\0\24\0\0\0\203\0\5\0\15\0\0\0\30\0\0"
	"\0\25\0\0\0\27\0\0\0>\0\3\0\17\0\0\0\30\0\0\0=\0\4\0\15\0\0\0\33\0\0\0\17\0\0\0=\0\4\0\15\0\0\0\34\0\0\0\17\0\0\0\224\0\5\0\6\0\0\0\35\0\0\0\33\0\0\0\34\0\0\0\203"
	"\0\5\0\6\0\0\0\36\0\0\0\26\0\0\0\35\0\0\0\f\0\10\0\6\0\0\0\37\0\0\0\1\0\0\0\61\0\0\0\31\0\0\0\32\0\0\0\36\0\0\0A\0\5\0\"\0\0\0#\0\0\0\t\0\0\0!\0\0\0=\0\4\0\6\0\0"
	"\0$\0\0\0#\0\0\0\205\0\5\0\6\0\0\0%\0\0\0$\0\0\0\37\0\0\0A\0\5\0\"\0\0\0&\0\0\0\t\0\0\0!\0\0\0>\0\3\0&\0\0\0%\0\0\0\375\0\1\08\0\1",
	"\3\2#\7\0\0\1\0\v\0\15\0N\0\0\0\0\0\0\0\21\0\2\0\1\0\0\0\v\0\6\0\1\0\0\0GLSL.std.450\0\0\0\0\16\0\3\0\0\0\0\0\1\0\0\0\17\0\10\0\4\0\0\0\4\0\0\0main\0\0\0\0#\0\0"
	"\0G\0\0\0I\0\0\0\20\0\3\0\4\0\0\0\7\0\0\0\3\0\3\0\2\0\0\0\302\1\0\0\4\0\n\0GL_GOOGLE_cpp_style_line_directive\0\0\4\0\10\0GL_GOOGLE_include_directive\0\5\0\4\0\4"
	"\0\0\0main\0\0\0\0\5\0\7\0\f\0\0\0median(f1;f1;f1;\0\0\0\0\5\0\3\0\t\0\0\0r\0\0\0\5\0\3\0\n\0\0\0g\0\0\0\5\0\3\0\v\0\0\0b\0\0\0\5\0\3\0\33\0\0\0msd\0\5\0\3\0\37"
	"\0\0\0tex\0\5\0\3\0#\0\0\0vp\0\0\5\0\3\0&\0\0\0psd\0\5\0\4\0'\0\0\0param\0\0\0\5\0\4\0,\0\0\0param\0\0\0\5\0\4\0\60\0\0\0param\0\0\0\5\0\3\0\65\0\0\0opc\0\5\0\3"
	"\0\67\0\0\0PT\0\0\6\0\5\0\67\0\0\0\0\0\0\0spxr\0\0\0\0\5\0\4\09\0\0\0push\0\0\0\0\5\0\3\0G\0\0\0oc\0\0\5\0\3\0I\0\0\0col\0G\0\4\0\37\0\0\0\"\0\0\0\0\0\0\0G\0\4\0"
	"\37\0\0\0!\0\0\0\0\0\0\0G\0\4\0#\0\0\0\36\0\0\0\0\0\0\0H\0\5\0\67\0\0\0\0\0\0\0#\0\0\0@\0\0\0G\0\3\0\67\0\0\0\2\0\0\0G\0\4\0G\0\0\0\36\0\0\0\0\0\0\0G\0\4\0I\0\0"
	"\0\36\0\0\0\1\0\0\0\23\0\2\0\2\0\0\0!\0\3\0\3\0\0\0\2\0\0\0\26\0\3\0\6\0\0\0 \0\0\0 \0\4\0\7\0\0\0\7\0\0\0\6\0\0\0!\0\6\0\10\0\0\0\6\0\0\0\7\0\0\0\7\0\0\0\7\0\0"
	"\0\27\0\4\0\31\0\0\0\6\0\0\0\4\0\0\0 \0\4\0\32\0\0\0\7\0\0\0\31\0\0\0\31\0\t\0\34\0\0\0\6\0\0\0\1\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\1\0\0\0\0\0\0\0\33\0\3\0\35\0\0\0"
	"\34\0\0\0 \0\4\0\36\0\0\0\0\0\0\0\35\0\0\0;\0\4\0\36\0\0\0\37\0\0\0\0\0\0\0\27\0\4\0!\0\0\0\6\0\0\0\2\0\0\0 \0\4\0\"\0\0\0\1\0\0\0!\0\0\0;\0\4\0\"\0\0\0#\0\0\0\1"
	"\0\0\0\25\0\4\0(\0\0\0 \0\0\0\0\0\0\0+\0\4\0(\0\0\0)\0\0\0\0\0\0\0+\0\4\0(\0\0\0-\0\0\0\1\0\0\0+\0\4\0(\0\0\0\61\0\0\0\2\0\0\0+\0\4\0\6\0\0\0\66\0\0\0\0\0\0?\36"
	"\0\3\0\67\0\0\0\6\0\0\0 \0\4\08\0\0\0\t\0\0\0\67\0\0\0;\0\4\08\0\0\09\0\0\0\t\0\0\0\25\0\4\0:\0\0\0 \0\0\0\1\0\0\0+\0\4\0:\0\0\0;\0\0\0\0\0\0\0 \0\4\0<\0\0\0\t\0"
	"\0\0\6\0\0\0+\0\4\0\6\0\0\0C\0\0\0\0\0\0\0+\0\4\0\6\0\0\0D\0\0\0\0\0\200? \0\4\0F\0\0\0\3\0\0\0\31\0\0\0;\0\4\0F\0\0\0G\0\0\0\3\0\0\0 \0\4\0H\0\0\0\1\0\0\0\31\0"
	"\0\0;\0\4\0H\0\0\0I\0\0\0\1\0\0\0\66\0\5\0\2\0\0\0\4\0\0\0\0\0\0\0\3\0\0\0\370\0\2\0\5\0\0\0;\0\4\0\32\0\0\0\33\0\0\0\7\0\0\0;\0\4\0\7\0\0\0&\0\0\0\7\0\0\0;\0\4"
	"\0\7\0\0\0'\0\0\0\7\0\0\0;\0\4\0\7\0\0\0,\0\0\0\7\0\0\0;\0\4\0\7\0\0\0\60\0\0\0\7\0\0\0;\0\4\0\7\0\0\0\65\0\0\0\7\0\0\0=\0\4\0\35\0\0\0 \0\0\0\37\0\0\0=\0\4\0!\0"
	"\0\0$\0\0\0#\0\0\0W\0\5\0\31\0\0\0%\0\0\0 \0\0\0$\0\0\0>\0\3\0\33\0\0\0%\0\0\0A\0\5\0\7\0\0\0*\0\0\0\33\0\0\0)\0\0\0=\0\4\0\6\0\0\0+\0\0\0*\0\0\0>\0\3\0'\0\0\0+"
	"\0\0\0A\0\5\0\7\0\0\0.\0\0\0\33\0\0\0-\0\0\0=\0\4\0\6\0\0\0/\0\0\0.\0\0\0>\0\3\0,\0\0\0/\0\0\0A\0\5\0\7\0\0\0\62\0\0\0\33\0\0\0\61\0\0\0=\0\4\0\6\0\0\0\63\0\0\0"
	"\62\0\0\0>\0\3\0\60\0\0\0\63\0\0\09\0\7\0\6\0\0\0\64\0\0\0\f\0\0\0'\0\0\0,\0\0\0\60\0\0\0>\0\3\0&\0\0\0\64\0\0\0A\0\5\0<\0\0\0=\0\0\09\0\0\0;\0\0\0=\0\4\0\6\0\0"
	"\0>\0\0\0=\0\0\0=\0\4\0\6\0\0\0?\0\0\0&\0\0\0\203\0\5\0\6\0\0\0@\0\0\0?\0\0\0\66\0\0\0\205\0\5\0\6\0\0\0A\0\0\0>\0\0\0@\0\0\0\201\0\5\0\6\0\0\0B\0\0\0\66\0\0\0A"
	"\0\0\0\f\0\10\0\6\0\0\0E\0\0\0\1\0\0\0+\0\0\0B\0\0\0C\0\0\0D\0\0\0>\0\3\0\65\0\0\0E\0\0\0=\0\4\0\31\0\0\0J\0\0\0I\0\0\0=\0\4\0\6\0\0\0K\0\0\0\65\0\0\0P\0\7\0\31"
	"\0\0\0L\0\0\0D\0\0\0D\0\0\0D\0\0\0K\0\0\0\205\0\5\0\31\0\0\0M\0\0\0J\0\0\0L\0\0\0>\0\3\0G\0\0\0M\0\0\0\375\0\1\08\0\1\0\66\0\5\0\6\0\0\0\f\0\0\0\0\0\0\0\10\0\0\0"
	"\67\0\3\0\7\0\0\0\t\0\0\0\67\0\3\0\7\0\0\0\n\0\0\0\67\0\3\0\7\0\0\0\v\0\0\0\370\0\2\0\15\0\0\0=\0\4\0\6\0\0\0\16\0\0\0\t\0\0\0=\0\4\0\6\0\0\0\17\0\0\0\n\0\0\0\f"
	"\0\7\0\6\0\0\0\20\0\0\0\1\0\0\0%\0\0\0\16\0\0\0\17\0\0\0=\0\4\0\6\0\0\0\21\0\0\0\t\0\0\0=\0\4\0\6\0\0\0\22\0\0\0\n\0\0\0\f\0\7\0\6\0\0\0\23\0\0\0\1\0\0\0(\0\0\0"
	"\21\0\0\0\22\0\0\0=\0\4\0\6\0\0\0\24\0\0\0\v\0\0\0\f\0\7\0\6\0\0\0\25\0\0\0\1\0\0\0%\0\0\0\23\0\0\0\24\0\0\0\f\0\7\0\6\0\0\0\26\0\0\0\1\0\0\0(\0\0\0\20\0\0\0\25"
	"\0\0\0\376\0\2\0\26\0\0\08\0\1",
	"\3\2#\7\0\0\1\0\v\0\15\0\20\0\0\0\0\0\0\0\21\0\2\0\1\0\0\0\v\0\6\0\1\0\0\0GLSL.std.450\0\0\0\0\16\0\3\0\0\0\0\0\1\0\0\0\17\0\10\0\4\0\0\0\4\0\0\0main\0\0\0\0\t\0"
	"\0\0\v\0\0\0\17\0\0\0\20\0\3\0\4\0\0\0\7\0\0\0\3\0\3\0\2\0\0\0\302\1\0\0\4\0\n\0GL_GOOGLE_cpp_style_line_directive\0\0\4\0\10\0GL_GOOGLE_include_directive\0\5\0"
	"\4\0\4\0\0\0main\0\0\0\0\5\0\3\0\t\0\0\0oc\0\0\5\0\3\0\v\0\0\0col\0\5\0\3\0\17\0\0\0vp\0\0G\0\4\0\t\0\0\0\36\0\0\0\0\0\0\0G\0\4\0\v\0\0\0\36\0\0\0\1\0\0\0G\0\4\0"
	"\17\0\0\0\36\0\0\0\0\0\0\0\23\0\2\0\2\0\0\0!\0\3\0\3\0\0\0\2\0\0\0\26\0\3\0\6\0\0\0 \0\0\0\27\0\4\0\7\0\0\0\6\0\0\0\4\0\0\0 \0\4\0\10\0\0\0\3\0\0\0\7\0\0\0;\0\4"
	"\0\10\0\0\0\t\0\0\0\3\0\0\0 \0\4\0\n\0\0\0\1\0\0\0\7\0\0\0;\0\4\0\n\0\0\0\v\0\0\0\1\0\0\0\27\0\4\0\15\0\0\0\6\0\0\0\2\0\0\0 \0\4\0\16\0\0\0\1\0\0\0\15\0\0\0;\0\4"
	"\0\16\0\0\0\17\0\0\0\1\0\0\0\66\0\5\0\2\0\0\0\4\0\0\0\0\0\0\0\3\0\0\0\370\0\2\0\5\0\0\0=\0\4\0\7\0\0\0\f\0\0\0\v\0\0\0>\0\3\0\t\0\0\0\f\0\0\0\375\0\1\08\0\1",
	"\3\2#\7\0\0\1\0\v\0\15\0+\0\0\0\0\0\0\0\21\0\2\0\1\0\0\0\v\0\6\0\1\0\0\0GLSL.std.450\0\0\0\0\16\0\3\0\0\0\0\0\1\0\0\0\17\0\v\0\0\0\0\0\4\0\0\0main\0\0\0\0\15\0\0"
	"\0\31\0\0\0$\0\0\0%\0\0\0'\0\0\0)\0\0\0\3\0\3\0\2\0\0\0\302\1\0\0\4\0\n\0GL_GOOGLE_cpp_style_line_directive\0\0\4\0\10\0GL_GOOGLE_include_directive\0\5\0\4\0\4\0"
	"\0\0main\0\0\0\0\5\0\6\0\v\0\0\0gl_PerVertex\0\0\0\0\6\0\6\0\v\0\0\0\0\0\0\0gl_Position\0\6\0\7\0\v\0\0\0\1\0\0\0gl_PointSize\0\0\0\0\6\0\7\0\v\0\0\0\2\0\0\0gl_"
	"ClipDistance\0\6\0\7\0\v\0\0\0\3\0\0\0gl_CullDistance\0\5\0\3\0\15\0\0\0\0\0\0\0\5\0\3\0\21\0\0\0PT\0\0\6\0\5\0\21\0\0\0\0\0\0\0proj\0\0\0\0\5\0\3\0\23\0\0\0p\0"
	"\0\0\5\0\3\0\31\0\0\0pos\0\5\0\3\0$\0\0\0ftp\0\5\0\3\0%\0\0\0tp\0\0\5\0\4\0'\0\0\0fcol\0\0\0\0\5\0\3\0)\0\0\0col\0H\0\5\0\v\0\0\0\0\0\0\0\v\0\0\0\0\0\0\0H\0\5\0"
	"\v\0\0\0\1\0\0\0\v\0\0\0\1\0\0\0H\0\5\0\v\0\0\0\2\0\0\0\v\0\0\0\3\0\0\0H\0\5\0\v\0\0\0\3\0\0\0\v\0\0\0\4\0\0\0G\0\3\0\v\0\0\0\2\0\0\0H\0\4\0\21\0\0\0\0\0\0\0\5\0"
	"\0\0H\0\5\0\21\0\0\0\0\0\0\0#\0\0\0\0\0\0\0H\0\5\0\21\0\0\0\0\0\0\0\7\0\0\0\20\0\0\0G\0\3\0\21\0\0\0\2\0\0\0G\0\4\0\31\0\0\0\36\0\0\0\0\0\0\0G\0\4\0$\0\0\0\36\0"
	"\0\0\0\0\0\0G\0\4\0%\0\0\0\36\0\0\0\1\0\0\0G\0\4\0'\0\0\0\36\0\0\0\1\0\0\0G\0\4\0)\0\0\0\36\0\0\0\2\0\0\0\23\0\2\0\2\0\0\0!\0\3\0\3\0\0\0\2\0\0\0\26\0\3\0\6\0\0"
	"\0 \0\0\0\27\0\4\0\7\0\0\0\6\0\0\0\4\0\0\0\25\0\4\0\10\0\0\0 \0\0\0\0\0\0\0+\0\4\0\10\0\0\0\t\0\0\0\1\0\0\0\34\0\4\0\n\0\0\0\6\0\0\0\t\0\0\0\36\0\6\0\v\0\0\0\7\0"
	"\0\0\6\0\0\0\n\0\0\0\n\0\0\0 \0\4\0\f\0\0\0\3\0\0\0\v\0\0\0;\0\4\0\f\0\0\0\15\0\0\0\3\0\0\0\25\0\4\0\16\0\0\0 \0\0\0\1\0\0\0+\0\4\0\16\0\0\0\17\0\0\0\0\0\0\0\30"
	"\0\4\0\20\0\0\0\7\0\0\0\4\0\0\0\36\0\3\0\21\0\0\0\20\0\0\0 \0\4\0\22\0\0\0\t\0\0\0\21\0\0\0;\0\4\0\22\0\0\0\23\0\0\0\t\0\0\0 \0\4\0\24\0\0\0\t\0\0\0\20\0\0\0\27"
	"\0\4\0\27\0\0\0\6\0\0\0\2\0\0\0 \0\4\0\30\0\0\0\1\0\0\0\27\0\0\0;\0\4\0\30\0\0\0\31\0\0\0\1\0\0\0+\0\4\0\6\0\0\0\33\0\0\0\0\0\0\0+\0\4\0\6\0\0\0\34\0\0\0\0\0\200"
	"? \0\4\0!\0\0\0\3\0\0\0\7\0\0\0 \0\4\0#\0\0\0\3\0\0\0\27\0\0\0;\0\4\0#\0\0\0$\0\0\0\3\0\0\0;\0\4\0\30\0\0\0%\0\0\0\1\0\0\0;\0\4\0!\0\0\0'\0\0\0\3\0\0\0 \0\4\0(\0"
	"\0\0\1\0\0\0\7\0\0\0;\0\4\0(\0\0\0)\0\0\0\1\0\0\0\66\0\5\0\2\0\0\0\4\0\0\0\0\0\0\0\3\0\0\0\370\0\2\0\5\0\0\0A\0\5\0\24\0\0\0\25\0\0\0\23\0\0\0\17\0\0\0=\0\4\0\20"
	"\0\0\0\26\0\0\0\25\0\0\0=\0\4\0\27\0\0\0\32\0\0\0\31\0\0\0Q\0\5\0\6\0\0\0\35\0\0\0\32\0\0\0\0\0\0\0Q\0\5\0\6\0\0\0\36\0\0\0\32\0\0\0\1\0\0\0P\0\7\0\7\0\0\0\37\0"
	"\0\0\35\0\0\0\36\0\0\0\33\0\0\0\34\0\0\0\221\0\5\0\7\0\0\0 \0\0\0\26\0\0\0\37\0\0\0A\0\5\0!\0\0\0\"\0\0\0\15\0\0\0\17\0\0\0>\0\3\0\"\0\0\0 \0\0\0=\0\4\0\27\0\0\0"
	"&\0\0\0%\0\0\0>\0\3\0$\0\0\0&\0\0\0=\0\4\0\7\0\0\0*\0\0\0)\0\0\0>\0\3\0'\0\0\0*\0\0\0\375\0\1\08\0\1",
	"\3\2#\7\0\0\1\0\v\0\15\0\30\0\0\0\0\0\0\0\21\0\2\0\1\0\0\0\v\0\6\0\1\0\0\0GLSL.std.450\0\0\0\0\16\0\3\0\0\0\0\0\1\0\0\0\17\0\10\0\4\0\0\0\4\0\0\0main\0\0\0\0\t\0"
	"\0\0\v\0\0\0\24\0\0\0\20\0\3\0\4\0\0\0\7\0\0\0\3\0\3\0\2\0\0\0\302\1\0\0\4\0\n\0GL_GOOGLE_cpp_style_line_directive\0\0\4\0\10\0GL_GOOGLE_include_directive\0\5\0"
	"\4\0\4\0\0\0main\0\0\0\0\5\0\3\0\t\0\0\0oc\0\0\5\0\3\0\v\0\0\0col\0\5\0\3\0\20\0\0\0tex\0\5\0\3\0\24\0\0\0vp\0\0G\0\4\0\t\0\0\0\36\0\0\0\0\0\0\0G\0\4\0\v\0\0\0\36"
	"\0\0\0\1\0\0\0G\0\4\0\20\0\0\0\"\0\0\0\0\0\0\0G\0\4\0\20\0\0\0!\0\0\0\0\0\0\0G\0\4\0\24\0\0\0\36\0\0\0\0\0\0\0\23\0\2\0\2\0\0\0!\0\3\0\3\0\0\0\2\0\0\0\26\0\3\0\6"
	"\0\0\0 \0\0\0\27\0\4\0\7\0\0\0\6\0\0\0\4\0\0\0 \0\4\0\10\0\0\0\3\0\0\0\7\0\0\0;\0\4\0\10\0\0\0\t\0\0\0\3\0\0\0 \0\4\0\n\0\0\0\1\0\0\0\7\0\0\0;\0\4\0\n\0\0\0\v\0"
	"\0\0\1\0\0\0\31\0\t\0\15\0\0\0\6\0\0\0\1\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\1\0\0\0\0\0\0\0\33\0\3\0\16\0\0\0\15\0\0\0 \0\4\0\17\0\0\0\0\0\0\0\16\0\0\0;\0\4\0\17\0\0"
	"\0\20\0\0\0\0\0\0\0\27\0\4\0\22\0\0\0\6\0\0\0\2\0\0\0 \0\4\0\23\0\0\0\1\0\0\0\22\0\0\0;\0\4\0\23\0\0\0\24\0\0\0\1\0\0\0\66\0\5\0\2\0\0\0\4\0\0\0\0\0\0\0\3\0\0\0"
	"\370\0\2\0\5\0\0\0=\0\4\0\7\0\0\0\f\0\0\0\v\0\0\0=\0\4\0\16\0\0\0\21\0\0\0\20\0\0\0=\0\4\0\22\0\0\0\25\0\0\0\24\0\0\0W\0\5\0\7\0\0\0\26\0\0\0\21\0\0\0\25\0\0\0\205"
	"\0\5\0\7\0\0\0\27\0\0\0\f\0\0\0\26\0\0\0>\0\3\0\t\0\0\0\27\0\0\0\375\0\1\08\0\1",
};
constexpr unsigned int shader_sizes[] = { 972, 1960, 516, 1416, 712, };
constexpr unsigned int circle_frag = 0;
constexpr unsigned int text_frag = 1;
constexpr unsigned int solid_frag = 2;
constexpr unsigned int v2d_vert = 3;
constexpr unsigned int textured_frag = 4;
constexpr unsigned int shader_count = 5;

/*}[INJ_MARK](shaders2d)*/

	void quad6v2d(std::vector<vertex2d> &out, glm::vec2 a, glm::vec2 b, const glm::vec4 &col, glm::vec2 uva, glm::vec2 uvb) {
		out.push_back(vertex2d{a, uva, col});
		out.push_back(vertex2d{{a.x, b.y}, { uva.x, uvb.y }, col});
		out.push_back(vertex2d{{b.x, a.y}, { uvb.x, uva.y }, col});
		out.push_back(vertex2d{{b.x, a.y}, { uvb.x, uva.y }, col});
		out.push_back(vertex2d{{a.x, b.y}, { uva.x, uvb.y }, col});
		out.push_back(vertex2d{b, uvb, col});
	}
	std::vector<vertex2d> quad6v2d(glm::vec2 a, glm::vec2 b, const glm::vec4 &col, glm::vec2 uva, glm::vec2 uvb) {
		std::vector<vertex2d> out;
		quad6v2d(out, a, b, col, uva, uvb);
		return out;
	}

	renderer2d::renderer2d(const context &ctx, const vk::raii::Device &device, vk::RenderPass render_pass, u32 width, u32 height) :
		ctx(ctx), device(device), texture_dsl(nullptr), solid_pipeline_layout(nullptr), textured_pipeline_layout(nullptr),
		text_pipeline_layout(nullptr), solid_pipeline(nullptr), circle_pipeline(nullptr), textured_pipeline(nullptr), text_pipeline(nullptr), linear_sampler(nullptr),
		nearest_sampler(nullptr) {
		vk::DescriptorSetLayoutBinding texture_dsl_binding{0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment, {}};
		texture_dsl = device.createDescriptorSetLayout({{}, texture_dsl_binding});
		ctx.set_object_name(*device, *texture_dsl, "2d renderer texture descriptor set layout");
		
		vk::PushConstantRange push_range{vk::ShaderStageFlagBits::eVertex, 0, sizeof(glm::mat4)};
		solid_pipeline_layout = device.createPipelineLayout({{}, {}, push_range});
		textured_pipeline_layout = device.createPipelineLayout({{}, *texture_dsl, push_range});
		vk::PushConstantRange push_range_text{vk::ShaderStageFlagBits::eFragment, sizeof(glm::mat4), sizeof(text_push)};
		std::array<vk::PushConstantRange, 2> push_constant_ranges_text{ push_range, push_range_text };
		text_pipeline_layout = device.createPipelineLayout({{}, *texture_dsl, push_constant_ranges_text});
		ctx.set_object_name(*device, *solid_pipeline_layout, "2d renderer solid pipeline layout");
		ctx.set_object_name(*device, *textured_pipeline_layout, "2d renderer textured pipeline layout");
		ctx.set_object_name(*device, *text_pipeline_layout, "2d renderer text pipeline layout");

		for (unsigned int i = 0; i < shader_count; i++) {
			shaders.push_back(device.createShaderModule({{}, shader_sizes[i], reinterpret_cast<const uint32_t *>(shader_code[i])}));
		}
		ctx.set_object_name(*device, *shaders[v2d_vert], "2d renderer vertex shader module");
		ctx.set_object_name(*device, *shaders[solid_frag], "2d renderer solid fragment shader module");
		ctx.set_object_name(*device, *shaders[circle_frag], "2d renderer circle fragment shader module");
		ctx.set_object_name(*device, *shaders[textured_frag], "2d renderer textured fragment shader module");
		ctx.set_object_name(*device, *shaders[text_frag], "2d renderer text fragment shader module");

		{
			rend::graphics_pipeline_create_maker pcm;
			pcm.set_shaders_gl({ vk::ShaderStageFlagBits::eVertex, vk::ShaderStageFlagBits::eFragment }, { *shaders[v2d_vert], *shaders[solid_frag] });
			pcm.add_vertex_input_attribute(0, vglm<glm::vec2>::format).add_vertex_input_attribute(1, vglm<glm::vec2>::format, offsetof(vertex2d, uv))
				.add_vertex_input_attribute(2, vglm<glm::vec4>::format, offsetof(vertex2d, color)).add_vertex_input_binding(sizeof(vertex2d), vk::VertexInputRate::eVertex);
			pcm.set_attachment_basic_blending();
			pcm.rasterization_state.cullMode = vk::CullModeFlagBits::eNone;
			solid_pipeline = device.createGraphicsPipeline(nullptr, pcm.get(*solid_pipeline_layout, render_pass));
			pcm.set_shaders_gl({ vk::ShaderStageFlagBits::eVertex, vk::ShaderStageFlagBits::eFragment }, { *shaders[v2d_vert], *shaders[circle_frag] });
			circle_pipeline = device.createGraphicsPipeline(nullptr, pcm.get(*solid_pipeline_layout, render_pass));
			pcm.set_shaders_gl({ vk::ShaderStageFlagBits::eVertex, vk::ShaderStageFlagBits::eFragment }, { *shaders[v2d_vert], *shaders[textured_frag] });
			textured_pipeline = device.createGraphicsPipeline(nullptr, pcm.get(*textured_pipeline_layout, render_pass));
			pcm.set_shaders_gl({ vk::ShaderStageFlagBits::eVertex, vk::ShaderStageFlagBits::eFragment }, { *shaders[v2d_vert], *shaders[text_frag] });
			text_pipeline = device.createGraphicsPipeline(nullptr, pcm.get(*text_pipeline_layout, render_pass));
		}
		ctx.set_object_name(*device, *solid_pipeline, "2d renderer solid pipeline");
		ctx.set_object_name(*device, *circle_pipeline, "2d renderer circle pipeline");
		ctx.set_object_name(*device, *textured_pipeline, "2d renderer textured pipeline");
		ctx.set_object_name(*device, *text_pipeline, "2d renderer text pipeline");

		linear_sampler = device.createSampler({{}, vk::Filter::eLinear, vk::Filter::eLinear, vk::SamplerMipmapMode::eNearest,
			vk::SamplerAddressMode::eClampToBorder, vk::SamplerAddressMode::eClampToBorder, vk::SamplerAddressMode::eClampToBorder, 0.f, false, 0.f, false});
		nearest_sampler = device.createSampler({{}, vk::Filter::eNearest, vk::Filter::eLinear, vk::SamplerMipmapMode::eNearest,
			vk::SamplerAddressMode::eClampToBorder, vk::SamplerAddressMode::eClampToBorder, vk::SamplerAddressMode::eClampToBorder, 0.f, false, 0.f, false});
		ctx.set_object_name(*device, *linear_sampler, "2d renderer linear sampler");
		ctx.set_object_name(*device, *nearest_sampler, "2d renderer nearest sampler");
		resize(width, height);
	}
	void renderer2d::push_projection(vk::CommandBuffer cmds, const glm::mat4 &proj) const {
		cmds.pushConstants(*solid_pipeline_layout, vk::ShaderStageFlagBits::eVertex, 0, sizeof(glm::mat4), &proj);
	}
	void renderer2d::push_textured_projection(vk::CommandBuffer cmds, const glm::mat4 &proj) const {
		cmds.pushConstants(*textured_pipeline_layout, vk::ShaderStageFlagBits::eVertex, 0, sizeof(glm::mat4), &proj);
	}
	void renderer2d::push_text_projection(vk::CommandBuffer cmds, const glm::mat4 &proj) const {
		cmds.pushConstants(*text_pipeline_layout, vk::ShaderStageFlagBits::eVertex, 0, sizeof(glm::mat4), &proj);
	}
	void renderer2d::push_text_constants(vk::CommandBuffer cmds, const text_push &push) const {
		cmds.pushConstants(*text_pipeline_layout, vk::ShaderStageFlagBits::eFragment, sizeof(glm::mat4), sizeof(text_push), &push);
	}
	void renderer2d::bind_texture(vk::CommandBuffer cmds, vk::DescriptorSet ds) const {
		cmds.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *textured_pipeline_layout, 0, ds, {});
	}
	void renderer2d::bind_texture_text(vk::CommandBuffer cmds, vk::DescriptorSet ds) const {
		cmds.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *text_pipeline_layout, 0, ds, {});
	}
	void renderer2d::bind_solid_pipeline(vk::CommandBuffer cmds) const { cmds.bindPipeline(vk::PipelineBindPoint::eGraphics, *solid_pipeline); }
	void renderer2d::bind_circle_pipeline(vk::CommandBuffer cmds) const { cmds.bindPipeline(vk::PipelineBindPoint::eGraphics, *circle_pipeline); }
	void renderer2d::bind_textured_pipeline(vk::CommandBuffer cmds) const { cmds.bindPipeline(vk::PipelineBindPoint::eGraphics, *textured_pipeline); }
	void renderer2d::bind_text_pipeline(vk::CommandBuffer cmds) const { cmds.bindPipeline(vk::PipelineBindPoint::eGraphics, *text_pipeline); }
	glm::mat4 renderer2d::proj(anchor2d a) const {
		return projs[usize(a)];
	}
	void renderer2d::resize(f32 width, f32 height) {
		const f32 wh = width * .5f;
		const f32 hh = height * .5f;
		projs[usize(anchor2d::center)] = glm::ortho(-wh, wh, -hh, hh);
		projs[usize(anchor2d::top)] = glm::ortho(-wh, wh, 0.f, height);
		projs[usize(anchor2d::topright)] = glm::ortho(-width, 0.f, 0.f, height);
		projs[usize(anchor2d::right)] = glm::ortho(-width, 0.f, -hh, hh);
		projs[usize(anchor2d::bottomright)] = glm::ortho(-width, 0.f, -height, 0.f);
		projs[usize(anchor2d::bottom)] = glm::ortho(-wh, wh, -height, 0.f);
		projs[usize(anchor2d::bottomleft)] = glm::ortho(0.f, width, -height, 0.f);
		projs[usize(anchor2d::left)] = glm::ortho(0.f, width, -hh, hh);
		projs[usize(anchor2d::topleft)] = glm::ortho(0.f, width, 0.f, height);
	}
	const context &renderer2d::get_ctx() const { return ctx; }
	const vk::raii::Device &renderer2d::get_device() const { return device; }
	vk::Sampler renderer2d::get_linear_sampler() const { return *linear_sampler; }
	vk::Sampler renderer2d::get_nearest_sampler() const { return *nearest_sampler; }

	std::ostream &operator<<(std::ostream &os, const vertex2d &v) {
		os << "vertex(" << glm::to_string(v.position) << ", uv " << glm::to_string(v.uv) << ", ~#";
		for (usize i = 0; i < 4; i++) {
			u8 c = u8(v.color[i] * 255);
			os << hex_chars[c / 16] << hex_chars[c % 16];
		}
		os << ")";
		return os;
	}
}
