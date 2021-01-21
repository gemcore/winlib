/* picoc main program - this varies depending on your operating system and
 * how you're using picoc */
/* platform-dependent code for running programs is in this file */
#if 0
#if defined(UNIX_HOST) || defined(WIN32)
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#endif
#endif

/* include only picoc.h here - should be able to use it with only the
    external interfaces, no internals from interpreter.h */
#include "picoc.h"

#if 0
#if defined(UNIX_HOST) || defined(WIN32)
#include "LICENSE.h"

/* Override via STACKSIZE environment variable */
#define PICOC_STACK_SIZE (128000*4)

int Cmd_pic(int argc, char **argv)
{
    int ParamCount = 1;
    int DontRunMain = false;
    int StackSize = getenv("STACKSIZE") ? atoi(getenv("STACKSIZE")) : PICOC_STACK_SIZE;
    Picoc pc;

    if (argc < 2 || strcmp(argv[ParamCount], "-h") == 0) {
        printf(PICOC_VERSION "  \n"
               "Format:\n\n"
               "> picoc <file1.c>... [- <arg1>...]    : run a program, calls main() as the entry point\n"
               "> picoc -s <file1.c>... [- <arg1>...] : run a script, runs the program without calling main()\n"
               "> picoc -i                            : interactive mode, Ctrl+d to exit\n"
               "> picoc -c                            : copyright info\n"
               "> picoc -h                            : this help message\n");
        return 0;
    }

    if (strcmp(argv[ParamCount], "-c") == 0) {
        printf("%s\n", (char*)&__LICENSE);
        return 0;
    }

    PicocInitialize(&pc, StackSize);

    if (strcmp(argv[ParamCount], "-s") == 0) {
        DontRunMain = true;
        PicocIncludeAllSystemHeaders(&pc);
        ParamCount++;
    }

    if (argc > ParamCount && strcmp(argv[ParamCount], "-i") == 0) {
        PicocIncludeAllSystemHeaders(&pc);
        PicocParseInteractive(&pc);
    } else {
        if (PicocPlatformSetExitPoint(&pc)) {
            PicocCleanup(&pc);
            return pc.PicocExitValue;
        }

        for (; ParamCount < argc && strcmp(argv[ParamCount], "-") != 0; ParamCount++)
            PicocPlatformScanFile(&pc, argv[ParamCount]);

        if (!DontRunMain)
            PicocCallMain(&pc, argc - ParamCount, &argv[ParamCount]);
    }

    PicocCleanup(&pc);
    return pc.PicocExitValue;
}
#endif
#endif

#if defined(EMBEDDED_HOST)
//#include "LICENSE.h"

/* Override via STACKSIZE environment variable */
#define PICOC_STACK_SIZE (100*1024)

// Example code for PicoC interpreter.
const char *TestC = \
"#include <stdio.h>\n"							\
"#include <stk.h>\n"                            \
"void test(void)\n"								\
"{\n"											\
"   int i;\n"									\
"   printf(\"\\n\");\n"							\
"\n"											\
"   printf(\"Hello World\\n\");\n"		        \
"   for(i = 0; i < 10000; i++)\n"				\
"   {}\n                "						\
"   delay_ms(1000);\n"							\
"\n"											\
"}\n"											\
"test();\n";

void PicocTest(Picoc *pc)
{
	//int rc = PicocPlatformSetExitPoint();
	int rc = setjmp(pc->PicocExitBuf);
	if (rc != 0)
	{
		printf("Exiting PicoC\n");
	}
	else
    {
	    PicocParse(pc, "nofile", TestC, strlen(TestC), true, true, false, false);
	}
}


int Cmd_pic(int argc, char **argv)
{
    int ParamCount = 1;
    int DontRunMain = false;
    int StackSize = PICOC_STACK_SIZE; //getenv("STACKSIZE") ? atoi(getenv("STACKSIZE")) : PICOC_STACK_SIZE;
    static Picoc pc;

    if (argc < 2 || strcmp(argv[ParamCount], "-h") == 0) {
        printf("pic -- PicoC Interpreter, " PICOC_VERSION "\n");
        printf("Usage:\n");
        printf(" -h                            : this help message\n");
        printf(" -t                            : test\n");
        printf(" -s <file1.c>... [- <arg1>...] : run a script, runs the program without calling main()\n");
        printf(" -i                            : interactive mode, Ctrl+d to exit\n");
        printf(" <file1.c>... [- <arg1>...]    : run a program, calls main() as the entry point\n");
        return 0;
    }

    PicocInitialize(&pc, StackSize);

    if (strcmp(argv[ParamCount], "-t") == 0) {
        PicocTest(&pc);
        PicocCleanup(&pc);
        return pc.PicocExitValue;
    }

    if (strcmp(argv[ParamCount], "-s") == 0) {
        DontRunMain = true;
        PicocIncludeAllSystemHeaders(&pc);
        ParamCount++;
    }

    if (argc > ParamCount && strcmp(argv[ParamCount], "-i") == 0) {
        PicocIncludeAllSystemHeaders(&pc);
        PicocParseInteractive(&pc);
    } else {
        if (PicocPlatformSetExitPoint(&pc)) {
            PicocCleanup(&pc);
            return pc.PicocExitValue;
        }

        for (; ParamCount < argc && strcmp(argv[ParamCount], "-") != 0; ParamCount++)
            PicocPlatformScanFile(&pc, argv[ParamCount]);

        if (!DontRunMain)
            PicocCallMain(&pc, argc - ParamCount, &argv[ParamCount]);
    }

    PicocCleanup(&pc);
    return pc.PicocExitValue;
}
#endif
