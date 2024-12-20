#include <string.h>
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
#define VERB() !(Gflags & SILENT)
int submain(int argc, char** argv);
int RunScript(std::string script);
int help();

int main (int argc, char** argv) {
    int ret = RET_OK;
    if (argc < 2) {
        DERROR("# No argumments provided\n");
        printf("# %s " DGREY "[flags]" DEFCOL " <operation> file(s)\n", argv[0]);
        printf("# use '%s -h' to see available commands\n", argv[0]);
        return 1;
    } else {
        int opbegin = -ENOENT;//where the operation flag was found
        for (int c = 1; c < argc; c++)
        {
            //optional args
            if (!strcmp(argv[c], "--silent")) Gflags |= SILENT;
            if (!strcmp(argv[c], "-h")) {return help();}
            else if (!strcmp(argv[c], "--verbose") && VERB()) Gflags |= VERBOSE; // silent has priority
            //program operations
            else if (!strcmp(argv[c], "-c")) opbegin = c;
            else if (!strcmp(argv[c], "-d")) opbegin = c;
            else if (!strcmp(argv[c], "-x")) opbegin = c;
            else if (!strcmp(argv[c], "-l")) opbegin = c;
            else if (!strcmp(argv[c], "-a")) opbegin = c;
            else if (!strcmp(argv[c], "-s")) opbegin = c;
        }
        if (opbegin != -ENOENT) ret = submain(argc-opbegin, argv+opbegin);
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
    return ret;
}

int submain(int argc, char** argv) {
    int ret = RET_OK;
    if (VERB())
        std::cout << "#PlayStation2 ROM Manager v" <<MAJOR<<"."<<MINOR<<"."<<PATCH<<" compiled " << buildate << "\n";
    rom ROMIMG;
    if (!strcmp(argv[0], "-l") && argc >= 2) {
        if (!ROMIMG.open(argv[1])) {
            ret = ROMIMG.displayContents();
        }
    } else
    if (!strcmp(argv[0], "-x") && argc >= 2) {
        if (!ROMIMG.open(argv[1])) {
            if (argc == 2) {
                ret = ROMIMG.dumpContents();
            } else {
                for (int i = 2; i < argc; i++)
                {
                    if ((ret = ROMIMG.dumpContents(argv[i])) != RET_OK) break;
                }
            }
        }
    } else
    if (!strcmp(argv[0], "-c") && argc >= 2) {
        if (!(ret = ROMIMG.CreateBlank(argv[1]))) {
            for (int i = 2; i < argc; i++) {
                if ((ret = ROMIMG.addFile(argv[i])) != RET_OK) break;
            }

            if (ret == RET_OK) ret = ROMIMG.write(argv[1]);
        }
    } else
    if (!strcmp(argv[0], "-a") && argc >= 2) {
        if (!ROMIMG.open(argv[1])) {
            for (int i = 2; i < argc; i++) {
                if ((ret = ROMIMG.addFile(argv[i])) != RET_OK) break;
            }

            if (ret == RET_OK) ret = ROMIMG.write(argv[1]);
        }
    } else
    if (!strcmp(argv[0], "-s") && argc >= 2) {
        ret = RunScript(argv[1]);
    }

err:
    if (ret != RET_OK) {

    }
    return 0;
}

enum SS {
    IMAGE_DECL = (1 << 0), // image declared
    OFFLS_DECL = (1 << 1), // offset table declared
};

#define np std::string::npos

off_t GetFileSize(std::string filename)
{
    struct stat stat_buf;
    int rc = stat(filename.c_str(), &stat_buf);
    return rc == 0 ? stat_buf.st_size : -ENOENT;
}

class fixfile {
    public:
    fixfile(std::string P1, size_t P2): fname(P1), offset(P2), fsize(GetFileSize(P1)) {/*DCOUT("Add file "<< fname << " off:" << offset << " siz:" << fsize <<"\n");*/};
    fixfile(std::string P1, size_t P2, size_t P3): fname(P1), offset(P2), fsize(P3)   {/*DCOUT("Add file "<< fname << " off:" << offset << " siz:" << fsize <<"\n");*/};
    std::string fname;
    size_t offset;
    off_t fsize;
    static bool CompareOffset(fixfile A, fixfile B) {return (A.offset < B.offset);}
    static bool CompareSize(fixfile A, fixfile B) {return (A.fsize > B.fsize);}
};

#define HEX(x) std::hex << x << std::dec
#define CHKFILE(x) (GetFileSize(x)<0) ? -ENOENT : RET_OK
#define CHKRESET(x) if (util::Basename(x) == "RESET") {ROMIMG.addFile(x, true); continue;}

std::vector<fixfile> FFiles;
std::vector<fixfile> CFiles;

#define FFMT "%-25s %-8ld %-8ld"
#define MAX_DEADGAP 128
int WriteImage(rom* ROM) {
    int ret = RET_OK;
    bool gapforce = false;
    off_t writtenbytes = 0x0;
    off_t Tdeadgap = 0x0;
    size_t rem = CFiles.size() + FFiles.size();

    std::sort(FFiles.begin(), FFiles.end(), fixfile::CompareOffset);
    int off = 0;
    for (size_t o=0, off=0; o < FFiles.size(); o++)
    {
        if (off >= FFiles[o].offset) {
            DERROR("FATAL ERROR: Fixed file at position %lu collides with %lu\n"
                    "File 1: " FFMT "\n"
                    "File 2: " FFMT "\n",
                    o-1, o,
                    FFiles[o-1].fname.c_str(), FFiles[o-1].fsize, FFiles[o-1].offset,
                    FFiles[o].fname.c_str(), FFiles[o].fsize, FFiles[o].offset
                    );
            ret = -EIMPOSSIBLE;
        }
        off = FFiles[o].offset + FFiles[o].fsize; // record where this fixed file ends so next iteration can detect a clash
    }


    printf("%-25s %-8s %-8s\n", "Files", "size", "offset");
    for (size_t x = 0, z = 0; x < CFiles.size() || z < FFiles.size();)
    {
        off_t next_off = (writtenbytes + CFiles[x].fsize);
        if (z < FFiles.size()) { // if fix pos file list is not empty
            if (next_off > FFiles[z].offset) { // next file clashes
                off_t gap = FFiles[z].offset - writtenbytes;
                if (gap < MAX_DEADGAP || !(x < CFiles.size()) || gapforce)  { // gap is smaller than 256 || ran out of common files
                    gapforce = false;
                    ROM->addDummy("-", gap);
                    printf(DGREY FFMT DEFCOL "\n", "-", gap, writtenbytes);
                    writtenbytes += gap;
                    Tdeadgap += gap;
                } else {
                    //DPRINTF("Gap too long. looking for another file\n");
                    size_t chosen = 0;
                    for (size_t a = x; a < CFiles.size(); a++)
                    {
                        if (CFiles[a].fsize < gap) { // file fits gap
                            if (chosen) {
                                if (CFiles[chosen].fsize < CFiles[a].fsize)
                                    chosen = a;
                            } else chosen = a;
                        }
                    }
                    if (chosen) { // found gap filler. write it down and remove it from the array to avoid parsing it twice
                        printf(FFMT "\n", CFiles[chosen].fname.c_str(), CFiles[chosen].fsize, writtenbytes);
                        ROM->addFile(CFiles[chosen].fname, false);
                        writtenbytes += CFiles[chosen].fsize;
                        CFiles.erase(CFiles.begin()+chosen);
                    } else gapforce = true; // gap filler not found, force the usage of blank gap filler.
                    continue;
                }
                printf(YELBOLD FFMT " (%ld)" DEFCOL "\n", FFiles[z].fname.c_str(), FFiles[z].fsize, writtenbytes, FFiles[z].offset);
                if (writtenbytes != FFiles[z].offset) {
                    DERROR("FATAL ERROR\tCOULD NOT POSITION FIXED FILE AT REQUESTED OFFSET\n\tRequested Off:%ld (%lx)\n\tReal Off:%ld (%lx)\n", FFiles[z].offset, FFiles[z].offset, writtenbytes, writtenbytes);
                    ret = -1;
                    break;
                }
                ROM->addFile(FFiles[z].fname, true);
                writtenbytes += FFiles[z].fsize;
                z++;
                continue;
            } else if (!(x < CFiles.size())) { // we have remaining fixed files, no more normal files!!!
                off_t gap = FFiles[z].offset - writtenbytes;
                ROM->addDummy("-", gap);
                printf(DGREY FFMT DEFCOL "\n", "-", gap, writtenbytes);
                writtenbytes += gap;
                Tdeadgap += gap;

                printf(YELBOLD FFMT " (%ld)" DEFCOL "\n", FFiles[z].fname.c_str(), FFiles[z].fsize, writtenbytes, FFiles[z].offset);
                ROM->addFile(FFiles[z].fname, true);
                writtenbytes += FFiles[z].fsize;
                z++;
            }
            // next file doesnt clash
        }
        if (x < CFiles.size()) { // condition in case we eat CFiles list before writing all FFiles
            printf(FFMT "\n", CFiles[x].fname.c_str(), CFiles[x].fsize, writtenbytes);
            ROM->addFile(CFiles[x].fname, false);
            writtenbytes += CFiles[x].fsize;
            x++;
        }
    }
    if (ret == RET_OK) ROM->write();
    printf("# Size of contents: %ld\n"
           "# Files written: %lu (%lu fixed, %lu normal)\n"
           "# Space spent in dummy gaps: %ld\n"
           "",
           writtenbytes, rem, FFiles.size(), CFiles.size(),
           Tdeadgap);

    return RET_OK;
}

int RunScript(std::string script) {
    rom ROMIMG;
    std::cout << "# Running Script '" << script << "'\n";
    std::regex sort("SortBySize\\(\\)");
    std::regex emptyline("$\\s+^");
    std::regex cimg("CreateImage\\(\"(.*)\"\\)");
    std::regex wimg("WriteImage\\(\\)");
    std::regex addfil("AddFile\\(\"(.*)\"\\)");
    std::regex dfixfil("AddFixedFile\\(\"(.*)\",\\s+([0-9a-fA-F]+)\\)");
    std::regex dfixfilhex("AddFixedFile\\(\"(.*)\",\\s+(0[xX][0-9a-fA-F]+)\\)");
    std::smatch match;

    FFiles.clear();
    CFiles.clear();
    int ss = 0,
        dline = 0,
        ret = RET_OK;
    std::string imagename = "";
    std::ifstream S(script);
    if (S.is_open()) {
        std::string line;
        while (getline(S, line))
        {
            dline++;
            if (line.substr(0,1) == "#") continue;
            else if (std::regex_search(line, match, cimg) && imagename == "") {
                if (VERB()) std::cout << GRNBOLD "> Create image file: '" << match[1] << DEFCOL "'\n";
                imagename = match[1];
                ret = ROMIMG.CreateBlank(imagename);
            } else
            if (std::regex_search(line, match, dfixfil)) {
                CHKRESET(match[1]);
                int e = std::stoi(match[2]);
                if (VERB()) std::cout << YELBOLD "> Adding File '"<< match[1] << "' at offset " << e << " (0x" << HEX(e) << ")"<< DEFCOL "\n";
                FFiles.push_back(fixfile(match[1], e));
                ret = CHKFILE(match[1]);
            } else
            if (std::regex_search(line, match, dfixfilhex)) {
                CHKRESET(match[1]);
                int e = std::stoi(match[2],0,16);
                if (VERB()) std::cout << YELBOLD "> Adding File '"<< match[1] << "' at offset " << e << " (0x" << HEX(e) << ")"<< DEFCOL "\n";
                FFiles.push_back(fixfile(match[1], e));
                ret = CHKFILE(match[1]);
            } else
            if (std::regex_search(line, match, addfil)) {
                CHKRESET(match[1]);
                if (VERB()) std::cout << "> Adding File '"<< match[1] << "'\n";
                CFiles.push_back(fixfile(match[1], 0));
                ret = CHKFILE(match[1]);
            } else
            if (std::regex_search(line, match, wimg)) {
                if (VERB()) std::cout << GRNBOLD "> Writing Image" DEFCOL "\n";
                ret = WriteImage(&ROMIMG);
            } else
            if (std::regex_search(line, match, sort)) {
                if (VERB()) std::cout << "> Sorting install files by size\n";
                std::sort(CFiles.begin(), CFiles.end(), fixfile::CompareSize);
            } else if (line != "" && !std::regex_search(line, match, emptyline)) {
                printf(REDBOLD "Unknown sequence found at " DEFCOL "%s:%d\n" WHITES "'%s'" DEFCOL "\n",
                script.c_str(), dline, line.c_str());
                break;
            }
            if (ret != RET_OK) {
                DERROR("# ERROR!  line:%d, err:%d, errno:\n\t%s", dline, ret, strerror(-ret));
                break;
            }
        }

    } else ret = -EIO;
    if (ret == RET_OK) {
    }
    return ret;
}


int help() {
    printf("# Supported commands:\n"
    "\t-c <new_image> <files...>\n"
    "\t\tCreate a new ROM image with some files\n"

//    "\t-d\t NOT IMPLEMENTED\n"

    "\t-x <image> <files...>\n"
    "\t\tExtracts contents of the ROM Image.\n"
    "\t\tif no additional params are provided it dumps whole image to subfolder by the name ext_<image>\n"

    "\t-l <image>\n"
    "\t\tList the contents of the image\n"

    "\t-a <image> <files...>\n"
    "\t\tAdd file(s) to existing image\n"

    "\t-s <file>\n"
    "\t\t Run a romman script\n"
    "\t\t more info about script language at " WHITES "github.com/israpps/romman/blob/main/scriptlang.md" DEFCOL "\n"
    );
    return 1;
}
