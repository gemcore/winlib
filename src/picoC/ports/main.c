#include "../picoc.h"

extern int picoc(char *SourceStr);
// Example code for STK-3700
// http://www.energymicro.com/tools/efm32-giant-gecko-starter-kit-efm32gg-stk3700
const char *TestC = \
"#include <stdio.h>\n"							\
"#include <stk.h>\n"							\
"void test(void)\n"								\
"{\n"											\
"	int i;\n"									\
"	printf(\"\\n\");\n"							\
"\n"											\
"	lcdInit();\n"								\
"	lcdWrite(\"EFM32 C\");\n"					\
"	delay_ms(1000);\n"							\
"\n"											\
"	printf(\"Flash PE2 0ms delay\\n\");\n"		\
"	lcdWrite(\"FAST\");\n"						\
"	pinMode(PE2, 4, 0);\n"						\
"	pinMode(PE3, 4, 0);\n"						\
"	for(i = 0; i < 10000; i++)\n"				\
"		pinToggle(PE2);\n"						\
"	delay_ms(1000);\n"							\
"\n"											\
"	printf(\"Flash PE2 500ms delay\\n\");\n"	\
"	lcdWrite(\"500ms\");\n"						\
"	for(i = 0; i < 10; i++)\n"					\
"	{\n"										\
"		delay_ms(500);\n"						\
"		pinToggle(PE2);\n"						\
"	}\n"										\
"\n"											\
"	lcdWrite(\"DONE\");\n"						\
"\n"											\
"	printf(\"Set PE3\\n\\n\");\n"				\
"	pinOut(PE3,1);\n"							\
"}\n"											\
"test();\n";

int main(int argc, char *arg[])
{
	picoc((char *)TestC);

	while(1)
	{
		// Enter to interactive mode
		picoc(NULL);
	}
}
