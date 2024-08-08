#include <string>
#include <string.h>
#include <iostream>
#include "version.h"
#include "debug.h"
#include "rom.h"
#include "utils.h"
#include "main.h"

const std::string buildate = __DATE__ " " __TIME__;

uint32_t Gflags = 0x0;
int submain(int argc, char** argv);

int main (int argc, char** argv) {
    if (argc < 2) {
        DERROR("# No argumments provided\n");
        printf("#%s [flags] <operation> file(s)\n", argv[0]);
        return 1;
    } else {
        int opbegin = -ENOENT;//where the operation flag was found
        for (int c = 1; c < argc; c++)
        {
            //optional args
            if (!strcmp(argv[c], "--silent")) Gflags |= SILENT;
            else if (!strcmp(argv[c], "--verbose") && !(Gflags & SILENT)) Gflags |= VERBOSE; // silent has priority
            //program operations
            else if (!strcmp(argv[c], "-c")) opbegin = c;
            else if (!strcmp(argv[c], "-d")) opbegin = c;
            else if (!strcmp(argv[c], "-x")) opbegin = c;
            else if (!strcmp(argv[c], "-l")) opbegin = c;
            else if (!strcmp(argv[c], "-a")) opbegin = c;
        }
        if (opbegin != -ENOENT) submain(argc-opbegin, argv+opbegin);
    }

    // The following code must be executed after all instances of class rom are destroyed!
    if (MTR.fc != MTR.mc) {
        DERROR("# Potential memory leak detected!\n"
        "#\tmalloc counter: %d\n"
        "#\tfree   counter: %d\n",
        MTR.mc,
        MTR.fc
        );
    }
    return RET_OK;
}

int submain(int argc, char** argv) {
    if (!(Gflags & SILENT))
        std::cout << "#PlayStation2 ROM Manager v" <<MAJOR<<"."<<MINOR<<"."<<PATCH<<" compiled " << buildate << "\n";
    rom ROMIMG;
    if (!strcmp(argv[0], "-l") && argc >= 2) {
        if (!ROMIMG.open(argv[1])) {
            ROMIMG.displayContents();
        }
    } else 
    if (!strcmp(argv[0], "-x") && argc >= 2) {
        if (!ROMIMG.open(argv[1])) {
            if (argc == 2) {
                ROMIMG.dumpContents();
            } else {
                for (int i = 2; i < argc; i++)
                {
                    if (ROMIMG.dumpContents(argv[i]) != 0) break;
                }
            }
        }
    }

err:
    return 0;
}