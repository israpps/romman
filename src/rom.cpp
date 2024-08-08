#include "debug.h"
#include "rom.h"
#include "utils.h"
#include "main.h"
#include <stdio.h>
#include <string.h>

rom::rom() {
}

rom::~rom() {
    ResetImageData();
    if (comment != nullptr) {
        FREE(comment);
    }
    for (size_t x = 0; x < files.size(); x++)
    {
        if (files[x].ExtInfoData != nullptr) {FREE(files[x].ExtInfoData);}
        if (files[x].FileData != nullptr) {FREE(files[x].FileData);}
    }
    files.clear();
    
}

int rom::CreateBlank(std::string path) {
    DPRINTF("Initializing rom with blank file\n");
    uint32_t comment_len = 0;
    char Localhost[32], cwd[128], *user;
    ExtInfoFieldEntry *ExtInfoEntry;
    rom::img_filepath = path;
    std::string filename = util::Basename(path);

    date = util::GetSystemDate();
#if defined(_WIN32) || defined(WIN32)
    user = getenv("user");
#else
    user = getenv("USER");
#endif
    util::GetLocalhostName(Localhost, sizeof(Localhost));
    util::getCWD(cwd, sizeof(cwd));
    rom::comment = (char*)MALLOC(comment_len);
    comment_len = 31 + strlen(filename.c_str()) + strlen(user) + strlen(Localhost) + strlen(cwd);
    sprintf(rom::comment, "%08x,conffile,%s,%s@%s/%s",
        rom::date, filename,
        (user == NULL) ? "" : user,
        Localhost, cwd);

    rom::FileEntry ResetFile;
    memset(ResetFile.RomDir.name, 0, sizeof(ResetFile.RomDir.name));
    strcpy((char*)ResetFile.RomDir.name, "RESET");
    ResetFile.RomDir.ExtInfoEntrySize = sizeof(ExtInfoFieldEntry) + sizeof(rom::date);
    ResetFile.RomDir.size = 0;
    ResetFile.FileData = NULL;
    ResetFile.ExtInfoData = (uint8_t*)MALLOC(ResetFile.RomDir.ExtInfoEntrySize);
    ExtInfoEntry = (ExtInfoFieldEntry *)ResetFile.ExtInfoData;
    ExtInfoEntry->value = 0;
    ExtInfoEntry->ExtLength = sizeof(rom::date);
    ExtInfoEntry->type = EXTINFO_FIELD_TYPE_DATE;
    memcpy((void *)(ResetFile.ExtInfoData + sizeof(ExtInfoFieldEntry)), &date, ExtInfoEntry->ExtLength);

    files.push_back(ResetFile);
    return RET_OK;
}

int rom::open(std::string path) {
    int ret = RET_OK;
    filefd FD;
    FILE* F;
    rom::img_filepath = path;
    void* pdate = nullptr;
    if ((F = fopen(path.c_str(), "rb")) != NULL) {
        fseek(F, 0, SEEK_END);
        image.size = ftell(F);
        rewind(F);
        image.data = (uint8_t *)MALLOC(image.size);
        if (image.data != nullptr) {
            if (fread(image.data, 1, image.size, F) == image.size) {
                if (findFSBegin() >= 0) {
                    image.fstart_ptr = (uint8_t*)(image.data + image.fstart);
                    //DumpHex(image.fstart_ptr, image.size-image.fstart);//hexdump the filesystem (no bootstrap hexdump)
                    if (openFile("RESET", &FD) != RET_OK) {
                        DERROR("Could not open RESET entry\n");
                        ret = -rerrno::ENORESET;
                        goto err;
                    }
                    if (FD.size != image.fstart) { // filesystem start should match the size of the RESET entry. that entry is representing bootstrap program
                        DERROR("-- Size of RESET does not match the start of ROMFS!\tImage is damaged\n");
                        ret = -rerrno::ERESETSIZEMTCH;
                        goto err;
                    }
                    pdate = &date;
                    GetExtInfoStat(&FD, EXTINFO_FIELD_TYPE_DATE, &pdate, sizeof(rom::date));
                    if (openFile("ROMDIR", &FD) != 0) {
                        DERROR(" Unable to locate ROMDIR!\n");
                        ret = -rerrno::ENOROMDIR;
                        goto err;
                    }

                    comment = nullptr;
                    GetExtInfoStat(&FD, EXTINFO_FIELD_TYPE_COMMENT, (void**)&comment, 0);
                    DirEntry *R = (DirEntry*)image.fstart_ptr;
                    unsigned int offset = 0;
                    uint32_t ExtInfoOffset = 0;
                    GetExtInfoOffset(&FD);
                    FileEntry file;
                    while (R->name[0] != '\0') {
                        if (strncmp((const char*)R->name, "ROMDIR",  sizeof(R->name)) != 0 && 
                            strncmp((const char*)R->name, "EXTINFO", sizeof(R->name)) != 0) {
                            memset(&file, 0x0, sizeof(rom::FileEntry));
                            //file = &ROMImg->files[ROMImg->NumFiles - 1];

                            memcpy(&file.RomDir, R, sizeof(DirEntry));
                            file.ExtInfoData = (uint8_t*)MALLOC(R->ExtInfoEntrySize);
                            memcpy(file.ExtInfoData, (void *)(image.data + FD.ExtInfoOffset + ExtInfoOffset), R->ExtInfoEntrySize);
                            file.FileData = MALLOC(R->size);
                            memcpy(file.FileData, (void *)(image.data + offset), R->size);
                            files.push_back(file);
                        }

                        offset += (R->size + 0xF) & ~0xF;
                        ExtInfoOffset += R->ExtInfoEntrySize;
                        R++;
                    }

                } else {ret = -rerrno::ENORESET; DERROR("Could not find ROMFS filesystem on image\n");}
            } else ret = -EIO;
        } else ret = -ENOMEM;
    } else ret = -EIO;
    fclose(F);
    return ret;
err:
    ResetImageData();
    return ret;
}

int32_t rom::findFSBegin() {
    image.fstart = -ENOENT;
    for (int32_t i = 0, result = -EIO, ScanLimit = 0x40000 < image.size ? 0x40000 : image.size; i < ScanLimit; i++) {
        if (((const char *)image.data)[i] == 'R' &&
            ((const char *)image.data)[i + 1] == 'E' &&
            ((const char *)image.data)[i + 2] == 'S' &&
            ((const char *)image.data)[i + 3] == 'E' &&
            ((const char *)image.data)[i + 4] == 'T') {
            if (Gflags & VERBOSE) printf("# Image filesystem begins at 0x%05x\n", i);
            image.fstart = i;
            break;
        }
    }
    return image.fstart;
}

int rom::openFile(std::string file, filefd* FD) {
    //DPRINTF("Opening '%s'\n", file.c_str());
    rom::DirEntry *E;
    memset(FD, 0, sizeof(filefd));
    int result;

    E = (rom::DirEntry*)image.fstart_ptr;
    result = ENOENT;
    if (GetExtInfoOffset(FD) == 0) {
        unsigned int offset = 0;
        while (E->name[0] != '\0') {
            if (strncmp(file.c_str(), (const char*)E->name, sizeof(E->name)) == 0) {
                FD->FileOffset = offset;
                FD->size = E->size;
                FD->ExtInfoEntrySize = E->ExtInfoEntrySize;
                result = 0;
                break;
            }

            FD->ExtInfoOffset += E->ExtInfoEntrySize;
            offset += (E->size + 0xF) & ~0xF;
            E++;
        }
    } else
        result = rerrno::ENOXTINF;

    return result;
}

int rom::GetExtInfoOffset(struct filefd *fd)
{
    rom::DirEntry *E;
    int result;
    unsigned int offset;
    E = (rom::DirEntry*)image.fstart_ptr;
    result = -rerrno::ENOXTINF;
    offset = 0;
    while (E->name[0] != '\0') {
        if (strncmp("EXTINFO", (const char*)E->name, sizeof(E->name)) == 0) {
            fd->ExtInfoOffset = offset;
            result = RET_OK;
            break;
        }

        offset += (E->size + 0xF) & ~0xF;
        E++;
    }

    return result;
}

int rom::GetExtInfoStat(filefd *fd, uint8_t type, void **buffer, uint32_t nbytes)
{
    int ret = -ENOENT;
    unsigned int offset = 0, BytesToCopy = 0;

    while (offset < fd->ExtInfoEntrySize) {
        ExtInfoFieldEntry *E = (ExtInfoFieldEntry *)(image.data + fd->ExtInfoOffset);

        if (E->type == EXTINFO_FIELD_TYPE_DATE || E->type == EXTINFO_FIELD_TYPE_COMMENT) {
            if (type == E->type) {
                if (nbytes >= E->ExtLength) {
                    BytesToCopy = E->ExtLength;
                    ret = RET_OK;
                } else {
                    if (*buffer != NULL) {
                        BytesToCopy = nbytes;
                    } else {
                        *buffer = MALLOC(E->ExtLength);
                        BytesToCopy = E->ExtLength;
                    }
                    ret = E->ExtLength;
                }

                memcpy(*buffer, &((const unsigned char *)image.data)[fd->ExtInfoOffset + offset + sizeof(ExtInfoFieldEntry)], BytesToCopy);
                break;
            }

            offset += (sizeof(ExtInfoFieldEntry) + E->ExtLength);
        } else if (E->type == EXTINFO_FIELD_TYPE_VERSION) {
            if (type == E->type) {
                if (nbytes >= sizeof(E->value)) {
                    ret = RET_OK;
                    memcpy(buffer, &E->value, sizeof(E->value));
                    break;
                } else {
                    ret = -ENOMEM;
                    break;
                }
            }

            offset += sizeof(struct ExtInfoFieldEntry);
        } else if (E->type == EXTINFO_FIELD_TYPE_FIXED) {
            ret = RET_OK;
            break;
        } else {
            DERROR("Unknown EXTINFO entry type: 0x%02x @ ROM offset: 0x%x | EXTINFO offset: %u\n", 
                E->type,
                fd->ExtInfoOffset + offset,
                offset);
            ret = -EIO;
            break;
        }
    }

    return ret;
}

int rom::addFile(std::string path)
{
    size_t x;
    FILE *F;
    int result = RET_OK;
    uint32_t FileDateStamp;
    uint16_t FileVersion;
    if ((F = fopen(path.c_str(), "rb")) != NULL) {
        const char* Fname = util::Basename(path).c_str();
        int size;
        fseek(F, 0, SEEK_END);
        size = ftell(F);
        rewind(F);

        FileEntry file;
        // Files cannot exist more than once in an image. The RESET entry is special here because all images will have a RESET entry, but it might be empty (If it has no content, allow the user to add in content).
        if (strcmp(Fname, "RESET")) {
            if (!rom::fileExists(Fname)) {
                memset(&file, 0, sizeof(FileEntry));
                strncpy((char*)file.RomDir.name, Fname, sizeof(FileEntry::RomDir.name));
                file.RomDir.ExtInfoEntrySize = 0;
                FileDateStamp = util::GetFileCreationDate(path.c_str());
                rom::AddExtInfoStat(&file, EXTINFO_FIELD_TYPE_DATE, &FileDateStamp, 4);
                if (util::IsSonyRXModule(path)) {
                    char ModuleDescription[32];
                    if ((result = util::GetSonyRXModInfo(path, ModuleDescription, sizeof(ModuleDescription), &FileVersion)) == RET_OK) {
                        AddExtInfoStat(&file, EXTINFO_FIELD_TYPE_VERSION, &FileVersion, 2);
                        AddExtInfoStat(&file, EXTINFO_FIELD_TYPE_COMMENT, ModuleDescription, strlen(ModuleDescription) + 1);
                    }
                } else
                    result = RET_OK;
                if (result == RET_OK) {
                    file.RomDir.size = size;
                    file.FileData = malloc(size);
                    fread(file.FileData, 1, size, F);
                }
            } else
                result = -EEXIST;
        } else {
            if (files[0].RomDir.size < 1) {
                files[0].RomDir.size = size;
                files[0].FileData = MALLOC(size);
                fread(files[0].FileData, 1, size, F);
                result = RET_OK;
            } else {
                DWARN("%s: bootstrap program found, skipping operation\n", __FUNCTION__);
                result = -EEXIST;
            }
        }

        fclose(F);
    } else
        result = -ENOENT;

    return result;
}

int rom::AddExtInfoStat(FileEntry *file, uint8_t type, void *data, uint8_t nbytes)
{
   int result;
   struct ExtInfoFieldEntry *ExtInfo;
   switch (type) {
      case EXTINFO_FIELD_TYPE_DATE:
      case EXTINFO_FIELD_TYPE_COMMENT:
         if (type != EXTINFO_FIELD_TYPE_DATE || (type == EXTINFO_FIELD_TYPE_DATE && nbytes == 4)) {
            if (ReallocExtInfoArea(file, nbytes) != nullptr) {
               ExtInfo = (rom::ExtInfoFieldEntry*)(file->ExtInfoData + file->RomDir.ExtInfoEntrySize);
               ExtInfo->value = 0;
               ExtInfo->ExtLength = ((nbytes + 0x3) & ~0x3);
               ExtInfo->type = type;
               memcpy(file->ExtInfoData + file->RomDir.ExtInfoEntrySize + sizeof(rom::ExtInfoFieldEntry), data, nbytes);

               file->RomDir.ExtInfoEntrySize += ExtInfo->ExtLength + sizeof(rom::ExtInfoFieldEntry);

               result = RET_OK;
            } else
               result = -ENOMEM;
         } else
            result = -EINVAL;
         break;
      case EXTINFO_FIELD_TYPE_VERSION:
         if (nbytes == 2) {
            if (ReallocExtInfoArea(file, 0) != NULL) {
               ExtInfo = (rom::ExtInfoFieldEntry*)(file->ExtInfoData + file->RomDir.ExtInfoEntrySize);
               ExtInfo->value = *(unsigned short int *)data;
               ExtInfo->ExtLength = 0;
               ExtInfo->type = type;

               file->RomDir.ExtInfoEntrySize += sizeof(rom::ExtInfoFieldEntry);

               result = 0;
            } else
               result = -ENOMEM;
         } else
            result = -EINVAL;
         break;
      default:
         result = -EINVAL;
   }

   return result;
}

void *rom::ReallocExtInfoArea(FileEntry *file, uint16_t nbytes)
{
    void* R;
   if (file->ExtInfoData == nullptr) {
        R = MALLOC(nbytes + sizeof(ExtInfoFieldEntry));
    } else {
        R = REALLOC(file->ExtInfoData, file->RomDir.ExtInfoEntrySize + nbytes + sizeof(ExtInfoFieldEntry));
    }
    file->ExtInfoData = (uint8_t*)R;
    return R;
}

int rom::fileExists(std::string filename)
{
    int result = -ENOENT;
    unsigned int x;

    for (x = 0; x < files.size(); x++) {
        if (strncmp((const char*)files[x].RomDir.name, filename.c_str(), sizeof(FileEntry::RomDir.name)) == 0) {
            result = RET_OK;
            break;
        }
    }
    return result;
}

void rom::displayContents()
{
    uint32_t TotalSize = 0;
    char filename[11];
    if (date != 0) {
        rom::date_helper D;
        D.fdate = date;
        printf("# date:\t%04x/%02x/%02x\n", D.sdate.yrs, D.sdate.mon, D.sdate.day);
    }
    if (comment != nullptr) printf("# comment:\t%s\n", comment);
    printf("# has bootstrap: %s\n", (rom::image.fstart == 0) ? "No" : GRNBOLD "Yes" DEFCOL);

    printf(
        "# File list:\n"
        "#Props Name      \tSize   \tOffset\n"
        "#------------------------------------------\n");

    for (size_t i = 0; i < files.size(); TotalSize += files[i].RomDir.size, i++) {
        int a=0;
        strncpy(filename, (const char*)files[i].RomDir.name, sizeof(filename) - 1);
        filename[sizeof(filename) - 1] = '\0';
        uint8_t* S = files[i].ExtInfoData;
        int x = files[i].RomDir.ExtInfoEntrySize;
        bool hd = false,
            hv = false,
            hc = false,
            hf = false;
            int any = false;
        for (int z = 0; z < x;)
        {
            switch (((ExtInfoFieldEntry*)S)->type)
            {
            case EXTINFO_FIELD_TYPE_DATE:
                a = ((ExtInfoFieldEntry*)S)->ExtLength + sizeof(ExtInfoFieldEntry);
                z += a;
                S += a;
                any = hd = true;
                break;
            case EXTINFO_FIELD_TYPE_VERSION:
                z += sizeof(ExtInfoFieldEntry);
                S += sizeof(ExtInfoFieldEntry);
                any = hv = true;
                break;
            case EXTINFO_FIELD_TYPE_COMMENT:
                a = ((ExtInfoFieldEntry*)S)->ExtLength + sizeof(ExtInfoFieldEntry);
                z += a;
                S += a;
                any = hc = true;
                break;
            case EXTINFO_FIELD_TYPE_FIXED:
                z += sizeof(ExtInfoFieldEntry);
                S += sizeof(ExtInfoFieldEntry);
                any = hf = true;
                break;
            default:
                any++;
                DERROR("Unknown EXTINFO type entry 0x%x at extinfo off 0x%x, len 0x%x, val 0x%x. HexDump:\n", 
                    ((ExtInfoFieldEntry*)S)->type,
                    sizeof(ExtInfoFieldEntry)*z,
                    ((ExtInfoFieldEntry*)S)->ExtLength,
                    ((ExtInfoFieldEntry*)S)->value
                    );
                util::hexdump((void*)files[i].ExtInfoData, files[i].RomDir.ExtInfoEntrySize);
                z += sizeof(ExtInfoFieldEntry);
                S += sizeof(ExtInfoFieldEntry);
                break;
            }
        }
        printf("%c%c%c%c   ",
            (hd) ? 'd' : '-',
            (hv) ? 'v' : '-',
            (hc) ? 'c' : '-',
            (hf) ? 'f' : '-'
        );
        printf("%s", (!any) ? DGREY : (hf) ? YELBOLD : GREEN);
        printf("%-10s%s\t%-7d\t%d\n", filename, DEFCOL, files[i].RomDir.size, TotalSize);
    }

    printf("\n#Total size: %u bytes.\n", TotalSize);
}

int rom::dumpContents(std::string file) {
    uint32_t TotalSize = 0;
    int ret = -ENOENT;
    for (size_t i = 0; i < files.size(); TotalSize += files[i].RomDir.size, i++) {
        if (!strncmp(file.c_str(), (const char*)files[i].RomDir.name, sizeof(FileEntry::RomDir.name))) {
            if (files[i].RomDir.size > 0) {
                FILE* F;
                if ((F = fopen(file.c_str(), "wb")) != NULL) {
                    if (fwrite(files[i].FileData, 1, files[i].RomDir.size, F) != files[i].RomDir.size) {
                        DERROR("# Error writing to file %s\n", file.c_str());
                        ret = -EIO;
                    }
                    fclose(F);
                    ret = RET_OK;
                } else {
                    DERROR("# Can't create file: %s\n", file.c_str());
                    ret = -EIO;
                }
            }
        }
    }
    return ret;
}

int rom::dumpContents(void) {
    util::genericgaugepercent(0, "RESET");
    std::string imgn = util::Basename(rom::img_filepath); //base name of the image for building subdir dumppath
    std::string fol = "./ext_" + imgn + PATHSEPS;

    if (util::dirExists(fol)) MKDIR(fol.c_str());
    for (size_t i = 0; i < files.size(); i++) {
        if (files[i].RomDir.size > 0) {
            std::string dpath = fol + (char*)files[i].RomDir.name;
            FILE* F;
            if ((F = fopen(dpath.c_str(), "wb")) != NULL) {
                if (fwrite(files[i].FileData, 1, files[i].RomDir.size, F) != files[i].RomDir.size) {
                    DERROR("\nError writing to file %s\n", dpath.c_str());
                }
                fclose(F);
            } else {
                DERROR("\nCan't create file: %s\n", dpath.c_str());
                break;
            }
        util::genericgaugepercent((i*100/(float)files.size()), (char*)files[i].RomDir.name);
        } else {DWARN("\nentry '%s' is 0 byte sized? skipping\n", (char*)files[i].RomDir.name);}
    }
    return RET_OK;
}

void rom::ResetImageData() {
    if (image.data) {FREE(image.data);}
    image.size = 0x0;
    image.fstart = 0x0;
    image.fstart_ptr = nullptr;
}
