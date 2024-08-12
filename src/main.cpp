#include <string>
#include <string.h>
#include <fstream>
#include <iostream>
#include <regex>
#include "version.h"
#include "debug.h"
#include "rom.h"
#include "utils.h"
#include "main.h"

const std::string buildate = __DATE__ " " __TIME__;

uint32_t Gflags = 0x0;
int submain(int argc, char** argv);
int RunScript(std::string script);

int main (int argc, char** argv) {
    if (argc < 2) {
        DERROR("# No argumments provided\n");
        printf("# %s " DGREY "[flags]" DEFCOL " <operation> file(s)\n", argv[0]);
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
            else if (!strcmp(argv[c], "-s")) opbegin = c;
        }
        if (opbegin != -ENOENT) submain(argc-opbegin, argv+opbegin);
    }

    // The following code must be executed after all instances of class rom are destroyed!
    if (MTR.fc != MTR.mc) {
        DERROR("# MEMORY MANAGER TRAP\n"
        "#\tmalloc  counter: %d\n"
        "#\tfree    counter: %d\n"
        "#\trealloc counter: %d\n",
        MTR.mc,
        MTR.fc,
        MTR.rc
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
    } else
    if (!strcmp(argv[0], "-c") && argc >= 2) {
        if (!ROMIMG.CreateBlank(argv[1])) {
            for (int i = 2; i < argc; i++) {
                ROMIMG.addFile(argv[i]);
            }
            
            ROMIMG.write(argv[1]);
        }
    } else
    if (!strcmp(argv[0], "-s") && argc >= 2) {
        RunScript(argv[1]);
    } 

err:
    return 0;
}

enum SS {
    IMAGE_DECL = (1 << 0), // image declared
    OFFLS_DECL = (1 << 1), // offset table declared
};

#define np std::string::npos

class fixfile {
    public:
    fixfile(std::string P1, size_t P2): fname(P1), offset(P2) {};
    std::string fname;
    size_t offset;
};

#define HEX(x) std::hex << x << std::dec

int RunScript(std::string script) {
    std::cout << "# Running Script '" << script << "'\n";
    std::regex cimg("CreateImage\\((.*)\\)");
    std::regex wimg("WriteImage\\((.*)\\)");
    std::regex addfil("AddFile\\((.*)\\)");
    std::regex dfixfil("AddFixedFile\\((.*), ([0-9a-fA-F]+)\\)");
    std::regex dfixfilhex("AddFixedFile\\((.*), (0[xX][0-9a-fA-F]+)\\)");
    std::smatch match;

    std::vector<fixfile> FFiles;
    std::vector<std::string> CFiles;
    int ss = 0;
    int ret = RET_OK;
    std::string imagename = "";
    std::ifstream S(script);
    if (S.is_open()) {
        std::string line;
        while (getline(S, line))
        {
            //std::cout << line <<"\n";
            if (line.substr(0,1) == "#") continue;
            else if (std::regex_search(line, match, cimg) && imagename == "") {
                std::cout << GRNBOLD "> image file: '" << match[1] << DEFCOL "'\n";
                imagename = match[1];
            } else
            if (std::regex_search(line, match, dfixfil)) {
                int e = std::stoi(match[2]);
                std::cout << YELBOLD "> File '"<< match[1] << "' at offset " << e << " (0x" << HEX(e) << ")"<< DEFCOL "\n";
                FFiles.push_back(fixfile(match[1], e));
            } else
            if (std::regex_search(line, match, dfixfilhex)) {
                int e = std::stoi(match[2],0,16);
                std::cout << YELBOLD "> File '"<< match[1] << "' at offset " << e << " (0x" << HEX(e) << ")"<< DEFCOL "\n";
                FFiles.push_back(fixfile(match[1], e));
            } else
            if (std::regex_search(line, match, addfil)) {
                std::cout << "> File '"<< match[1] << "'\n";
                CFiles.push_back(match[1]);
            } else
            if (std::regex_search(line, match, wimg)) {
                std::cout << GRNBOLD "> Write Image" DEFCOL "\n";
            }
            
        }
        
    } else ret = -EIO;
    if (ret == RET_OK) {
    }
    return ret;
}