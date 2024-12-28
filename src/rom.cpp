#include "debug.h"
#include "rom.h"
#include "utils.h"
#include "main.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h> // linux: without this, snprintf always complains of '-Wformat-truncation'

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

int rom::CreateBlank(std::string path, std::string confname, std::string folder) {
    uint32_t comment_len = 0;
    ExtInfoFieldEntry* ExtInfoEntry;
    rom::img_filepath = path;
    std::string filename = util::Basename(path);

    date = util::GetSystemDate();

    if (confname == "")
        confname = "conffile";
    if (folder == "") {
        char cwd[128];
        util::getCWD(cwd, sizeof(cwd));
        folder = cwd;
    }
    comment_len = 31 + strlen(filename.c_str()) + strlen(confname.c_str()) + strlen(folder.c_str());
    DPRINTF("Comment length: %u\n", comment_len);
    rom::comment = (char*) MALLOC(comment_len);

    snprintf(rom::comment, comment_len - 1, "%08x,%s,%s,%s",
             rom::date,
             confname.c_str(),
             filename.c_str(),
             folder.c_str());

    rom::FileEntry ResetFile;
    memset(ResetFile.RomDir.name, 0, sizeof(ResetFile.RomDir.name));
    strcpy((char*) ResetFile.RomDir.name, "RESET");
    ResetFile.RomDir.ExtInfoEntrySize = sizeof(ExtInfoFieldEntry) + sizeof(rom::date);
    ResetFile.RomDir.size = 0;
    ResetFile.FileData = NULL;
    ResetFile.ExtInfoData = (uint8_t*) MALLOC(ResetFile.RomDir.ExtInfoEntrySize);
    ExtInfoEntry = (ExtInfoFieldEntry*) ResetFile.ExtInfoData;
    ExtInfoEntry->value = 0;
    ExtInfoEntry->ExtLength = sizeof(rom::date);
    ExtInfoEntry->type = EXTINFO_FIELD_TYPE_DATE;
    memcpy((void*) (ResetFile.ExtInfoData + sizeof(ExtInfoFieldEntry)), &date, ExtInfoEntry->ExtLength);

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
                    //util::hexdump(image.fstart_ptr, 64);
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
    // int result = -EIO;
    uint32_t ScanLimit = BOOTSTRAP_MAX_SIZE < image.size ? BOOTSTRAP_MAX_SIZE : image.size;
    for (int32_t i = 0; i < ScanLimit; i++) {
        if (((const char *)image.data)[i] == 'R' &&
            ((const char *)image.data)[i + 1] == 'E' &&
            ((const char *)image.data)[i + 2] == 'S' &&
            ((const char *)image.data)[i + 3] == 'E' &&
            ((const char *)image.data)[i + 4] == 'T') {
#ifndef DEBUG
            if (Gflags & VERBOSE)
#endif
                printf("# Image filesystem begins at 0x%05x\n", i);
            image.fstart = i;
            break;
        }
    }
    return image.fstart;
}

int rom::openFile(std::string file, filefd* FD) {
    rom::DirEntry *E;
    memset(FD, 0, sizeof(filefd));
    int result;

    E = (rom::DirEntry*)image.fstart_ptr;
    result = -ENOENT;
    if (GetExtInfoOffset(FD) == 0) {
        unsigned int offset = 0;
        while (E->name[0] != '\0') {
            if (strncmp(file.c_str(), (const char*)E->name, sizeof(E->name)) == 0) {
                FD->FileOffset = offset;
                FD->size = E->size;
                FD->ExtInfoEntrySize = E->ExtInfoEntrySize;
                result = RET_OK;
                break;
            }

            FD->ExtInfoOffset += E->ExtInfoEntrySize;
            offset += (E->size + 0xF) & ~0xF;
            E++;
        }
    } else
        result = -rerrno::ENOXTINF;

    return result;
}

int rom::GetExtInfoOffset(filefd *fd)
{
    rom::DirEntry *E;
    int result = -rerrno::ENOXTINF;
    unsigned int offset;
    E = (rom::DirEntry*)image.fstart_ptr;
    offset = 0;
    while (E->name[0] != '\0') {
        if (strncmp("EXTINFO", (const char*)E->name, sizeof(E->name)) == 0) {
            fd->ExtInfoOffset = offset;
            image.fstart2 = offset + (E->size + 0xF) & ~0xF;
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
    unsigned int offset = 0, BytesToCopy;

    while (offset < fd->ExtInfoEntrySize) {
        ExtInfoFieldEntry *E = (ExtInfoFieldEntry *)(image.data + fd->ExtInfoOffset + offset);

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
                    memcpy(buffer, &E->value, sizeof(E->value));
                    ret = RET_OK;
                } else {
                    ret = -ENOMEM;
                }
                break;
            }
            offset += sizeof(struct ExtInfoFieldEntry);
        } else if (E->type == EXTINFO_FIELD_TYPE_FIXED) {
            if (type == E->type) {
                // TODO: implement offset return
                // printf("offset: %u\n", offset);
                // memcpy(*buffer, &offset, sizeof(offset));
                buffer = NULL;
                ret = RET_OK;
                break;
            }
            offset += sizeof(struct ExtInfoFieldEntry);
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

int rom::CheckExtInfoStat(filefd *fd, uint8_t type)
{
    int ret = -ENOENT;
    unsigned int offset = 0;

    while (offset < fd->ExtInfoEntrySize) {
        ExtInfoFieldEntry *E = (ExtInfoFieldEntry *)(image.data + fd->ExtInfoOffset);

        if (E->type == EXTINFO_FIELD_TYPE_DATE || E->type == EXTINFO_FIELD_TYPE_COMMENT) {
            if (type == E->type) {
                return RET_OK;
            }
            offset += (sizeof(ExtInfoFieldEntry) + E->ExtLength);
        } else if (E->type == EXTINFO_FIELD_TYPE_VERSION) {
            if (type == E->type) {
                ret = RET_OK;
                break;
            }

            offset += sizeof(struct ExtInfoFieldEntry);
        } else if (E->type == EXTINFO_FIELD_TYPE_FIXED) {
            if (type == E->type) {
                ret = RET_OK;
                break;
            }
            offset += sizeof(struct ExtInfoFieldEntry);
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

int rom::addFile(std::string path, bool isFixed, bool isDate, uint16_t version)
{
    //DPRINTF("%s(%s) %d cnt\n", __FUNCTION__, path.c_str(), files.size());
    // size_t x;
    FILE *F;
    int result = RET_OK;
    uint32_t FileDateStamp;
    uint16_t FileVersion;
    if ((F = fopen(path.c_str(), "rb")) != NULL) {
        std::string basename = util::Basename(path);
        const char* Fname = basename.c_str();  // Now Fname is safe to use
        int size;
        fseek(F, 0, SEEK_END);
        size = ftell(F);
        rewind(F);

        FileEntry file;
        // Files cannot exist more than once in an image. The RESET entry is special here because all images will have a RESET entry, but it might be empty (If it has no content, allow the user to add in content).
        if (strcmp(Fname, "RESET")) {
            if (rom::fileExists(Fname) != RET_OK) {
                memset(&file, 0, sizeof(FileEntry));
                strncpy((char*)file.RomDir.name, Fname, sizeof(file.RomDir.name));
                file.RomDir.ExtInfoEntrySize = 0;
                FileDateStamp = util::GetFileModificationDate(path.c_str());
                if (isDate) AddExtInfoStat(&file, EXTINFO_FIELD_TYPE_DATE, &FileDateStamp, 4);
                if (isFixed) AddExtInfoStat(&file, EXTINFO_FIELD_TYPE_FIXED, nullptr, 0);
                if (util::IsSonyRXModule(path)) {
                    char ModuleDescription[32] = {0};
                    if ((result = util::GetSonyRXModInfo(path, ModuleDescription, sizeof(ModuleDescription), &FileVersion)) == RET_OK) {
                        if (strlen(ModuleDescription) > 0) {
                            AddExtInfoStat(&file, EXTINFO_FIELD_TYPE_VERSION, &FileVersion, 2);
                            AddExtInfoStat(&file, EXTINFO_FIELD_TYPE_COMMENT, ModuleDescription, strlen(ModuleDescription) + 1);
                        }
                    }
                } else if (version > 0) {
                    result = AddExtInfoStat(&file, EXTINFO_FIELD_TYPE_VERSION, &version, 2);
                } else
                    result = RET_OK;
                if (result == RET_OK) {
                    file.RomDir.size = size;
                    file.FileData = MALLOC(size);
                    fread(file.FileData, 1, size, F);
                    files.push_back(file);
                }
            } else {
                DWARN("# '%s' file already exists in image \n", Fname)
                result = -EEXIST;
            }
        } else {
            if (size > BOOTSTRAP_MAX_SIZE) {
                DERROR("# ERROR: requested file exceeds max size for bootstrap program\n"
                "\tMax size: 0x%x\n\tFile size %x\n",
                BOOTSTRAP_MAX_SIZE, size);
                result = -EFBIG;
            } else if (files[0].RomDir.size < 1) {
                files[0].RomDir.size = size;
                files[0].FileData = MALLOC(size);
                fread(files[0].FileData, 1, size, F);
                // we are adding a bootstrap program to an image that did not have one. apply the fixed offset entry
                // RESET is always at begining of image. but since sony added this flag, we will do so.
                AddExtInfoStat(&files[0], EXTINFO_FIELD_TYPE_FIXED, nullptr, 0);
                result = RET_OK;
            } else {
                DWARN("%s: bootstrap program found, skipping operation\n", __FUNCTION__);
                result = -EEXIST;
            }
        }

        fclose(F);
    } else {
        DERROR("Cannot open %s\n", path.c_str());
        result = -ENOENT;
    }

    return result;
}

int rom::addDummy(std::string name, uint32_t dummysize, int imagepos) {
    if (imagepos == 0) {
        DERROR("ERROR: Requested to write a dummy at file 0, aborting...\n");
        return -ENOTSUP;
    }
    FileEntry file;
    memset(&file, 0, sizeof(FileEntry));
    strncpy((char*)file.RomDir.name, name.c_str(), sizeof(file.RomDir.name));
    file.RomDir.ExtInfoEntrySize = 0;
    file.RomDir.size = dummysize;
    file.FileData = MALLOC(dummysize);
    memset(file.FileData, 0, dummysize);
    if (imagepos == -1) {
        files.push_back(file);
    } else {
        files.insert(files.begin()+imagepos, file);
    }
    return RET_OK;
}

int rom::AddExtInfoStat(FileEntry *file, uint8_t type, void *data, uint8_t nbytes)
{
    int result;
    struct ExtInfoFieldEntry *ExtInfo;
    filefd fd;

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
    case EXTINFO_FIELD_TYPE_FIXED:
            if (nbytes != 2 && type == EXTINFO_FIELD_TYPE_VERSION) {
                result = -EINVAL;
                break;
            }
            if (ReallocExtInfoArea(file, 0) != NULL) {
                ExtInfo = (rom::ExtInfoFieldEntry*)(file->ExtInfoData + file->RomDir.ExtInfoEntrySize);
                ExtInfo->value = (data != nullptr) ? (*(uint16_t *)data) : 0x0;
                ExtInfo->ExtLength = 0;
                ExtInfo->type = type;

                file->RomDir.ExtInfoEntrySize += sizeof(rom::ExtInfoFieldEntry);

                result = 0;
            } else
                result = -ENOMEM;
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
        if (strncmp((const char*)files[x].RomDir.name, filename.c_str(), sizeof(files[x].RomDir.name)) == 0) {
            result = RET_OK;
            break;
        }
    }
    return result;
}

int rom::displayContents()
{
    int ret = RET_OK;
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
        "#Props Name              Size      Offset        Date Version Comment\n"
        "#--------------------------------------------------------------------\n");
    unsigned int count = 0;

    /**
     * @note ignore the reset entry size on the calculations because the Bootstrap program gets written to the begining of the image, before the ROMFS begins
     */
    for (size_t i = 0; i < files.size(); TotalSize += (i == 0) ? image.fstart2 : (files[i].RomDir.size + 0xF) & ~0xF, i++) {
        if (strncmp((const char*)files[i].RomDir.name, "-", sizeof(files[i].RomDir.name)) == 0)
            continue;
        count++;
        int a = 0;
        strncpy(filename, (const char*)files[i].RomDir.name, sizeof(filename) - 1);
        filename[sizeof(filename) - 1] = '\0';
        uint8_t* S = files[i].ExtInfoData;
        int x = files[i].RomDir.ExtInfoEntrySize;
        bool hd = false,
            hv = false,
            hc = false,
            hf = false;
        int any = false;
        void* pdate = nullptr;
        filefd FD;
        openFile(filename, &FD);
        char* currentComment = nullptr;
        date_helper dh = {0};
        uint16_t version = 0;
        for (int z = 0; z < x;)
        {
            switch (((ExtInfoFieldEntry*)S)->type)
            {
            case EXTINFO_FIELD_TYPE_DATE:
                a = ((ExtInfoFieldEntry*)S)->ExtLength + sizeof(ExtInfoFieldEntry);
                z += a;
                S += a;
                pdate = &date;
                GetExtInfoStat(&FD, EXTINFO_FIELD_TYPE_DATE, &pdate, sizeof(rom::date));
                dh = *(date_helper*)pdate;
                any = hd = true;
                break;
            case EXTINFO_FIELD_TYPE_VERSION:
                z += sizeof(ExtInfoFieldEntry);
                S += sizeof(ExtInfoFieldEntry);
                GetExtInfoStat(&FD, EXTINFO_FIELD_TYPE_VERSION, (void**)&version, sizeof(uint16_t));
                any = hv = true;
                break;
            case EXTINFO_FIELD_TYPE_COMMENT:
                a = ((ExtInfoFieldEntry *)S)->ExtLength + sizeof(ExtInfoFieldEntry);
                z += a;
                S += a;
                GetExtInfoStat(&FD, EXTINFO_FIELD_TYPE_COMMENT, (void**)&currentComment, 0);
                any = hc = true;
                break;
            case EXTINFO_FIELD_TYPE_FIXED:
                z += sizeof(ExtInfoFieldEntry);
                S += sizeof(ExtInfoFieldEntry);
                any = hf = true;
                break;
            default:
                ret = -EPERM;
                any++;
                DERROR("# Unknown EXTINFO type entry 0x%x at extinfo off 0x%lx, len 0x%x, val 0x%x. Extinfo HexDump:\n",
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
            (hf) ? 'f' : '-',
            (hd) ? 'd' : '-',
            (hv) ? 'v' : '-',
            (hc) ? 'c' : '-'
        );
        printf("%s", (!any) ? DGREY : (hf) ? YELBOLD : GREEN);
        printf("%-10s%s    %8d    %s%8u%s", filename, DEFCOL, files[i].RomDir.size, (hf) ? YELBOLD : DEFCOL ,TotalSize, DEFCOL);
        if (hd)
            printf("  %04x/%02x/%02x", dh.sdate.yrs, dh.sdate.mon, dh.sdate.day);
        else
            printf("            ");
        if (hv)
            printf("  0x%04x", version);
        else
            printf("        ");
        if (hc){
            printf("  %s", currentComment);
            FREE(currentComment);
        }
        printf("\n");
    }

    printf("\n# Total size: %u bytes.\n# File count: %lu\n", TotalSize - ((files[files.size() - 1].RomDir.size + 0xF) & ~0xF) + files[files.size() - 1].RomDir.size, count);
    return ret;
}

int rom::dumpContents(std::string file) {
    uint32_t TotalSize = 0;
    int ret = -ENOENT;
    for (size_t i = 0; i < files.size(); TotalSize += files[i].RomDir.size, i++) {
        if (!strncmp(file.c_str(), (const char*) files[i].RomDir.name, sizeof(files[i].RomDir.name))) {
            FILE* F;
            if ((F = fopen(file.c_str(), "wb")) != NULL) {
                if (fwrite(files[i].FileData, 1, files[i].RomDir.size, F) != files[i].RomDir.size) {
                    DERROR("# Error writing to file %s\n", file.c_str());
                    ret = -EIO;
                    fclose(F);
                    break;
                }
                fclose(F);
                ret = RET_OK;
            } else {
                DERROR("# Can't create file: %s\n", file.c_str());
                ret = -EIO;
            }
        }
    }
    return ret;
}

int rom::dumpContents(void) {
    int ret = RET_OK;
    util::genericgaugepercent(0, "RESET");
    std::string imgn = util::Basename(rom::img_filepath);  // base name of the image for building subdir dumppath
    std::string fol = "./ext_" + imgn + PATHSEPS;
    std::string conf = imgn + ".conf";
    FILE* Fconf;
    char* currentComment = nullptr;
    int i;
    size_t currentOffset = 0;  // Initialize the offset counter
    std::string romDirName;
    if ((Fconf = fopen(conf.c_str(), "wb")) == NULL) {
        DERROR("\nCan't create file: %s\n", conf.c_str());
        return -EIO;
    }

    if (util::dirExists(fol))
        MKDIR(fol.c_str());

    for (i = -1; i == -1 || i < files.size(); i++) {
        romDirName = (i == -1) ? "ROMDIR" : (char*) files[i].RomDir.name;
        void* pdate = malloc(sizeof(rom::date));
        filefd FD;
        char filename[11];
        strncpy(filename, romDirName.c_str(), sizeof(filename) - 1);
        filename[sizeof(filename) - 1] = '\0';
        openFile(filename, &FD);

        uint16_t temp = 0;
        date_helper dh = {0};
        uint16_t version;

        if (romDirName != "-") {
            fprintf(Fconf, "%s,", romDirName.c_str());
            // fprintf(Fconf, "0x%06x  ", currentOffset);
            if (GetExtInfoStat(&FD, EXTINFO_FIELD_TYPE_FIXED, (void**) &temp, sizeof(uint16_t)) >= 0) {
                fprintf(Fconf, "%zu", currentOffset);
            }

            fprintf(Fconf, ",");
            if (GetExtInfoStat(&FD, EXTINFO_FIELD_TYPE_DATE, &pdate, sizeof(rom::date)) >= 0) {
                dh = *(date_helper*) pdate;
                fprintf(Fconf, "%04x/%02x/%02x", dh.sdate.yrs, dh.sdate.mon, dh.sdate.day);
            } else {
                fprintf(Fconf, "-");
            }

            fprintf(Fconf, ",");
            if (GetExtInfoStat(&FD, EXTINFO_FIELD_TYPE_VERSION, (void**) &version, sizeof(uint16_t)) >= 0)
                fprintf(Fconf, "0x%04x", version);

            fprintf(Fconf, ",");
            if (GetExtInfoStat(&FD, EXTINFO_FIELD_TYPE_COMMENT, (void**) &currentComment, 0) >= 0) {
                fprintf(Fconf, "%s", currentComment);
                FREE(currentComment);
            }
            fprintf(Fconf, "\n");
        }

        if (i >= 0) {
            currentOffset += (i == 0) ? image.fstart2 : (files[i].RomDir.size + 0xF) & ~0xF;
            if (romDirName != "-") {
                std::string dpath = fol + romDirName;
                FILE* F;
                if ((F = fopen(dpath.c_str(), "wb")) != NULL) {
                    if (fwrite(files[i].FileData, 1, files[i].RomDir.size, F) != files[i].RomDir.size) {
                        DERROR("\nError writing to file %s\n", dpath.c_str());
                    }
                    fclose(F);

                    if (dh.fdate != 0) {
                        uint32_t date = (dh.sdate.yrs << 16) | (dh.sdate.mon << 8) | dh.sdate.day;
                        util::SetFileModificationDate(dpath.c_str(), date);
                    }
                    // Update the current offset
                } else {
                    ret = -EIO;
                    DERROR("\nCan't create file: %s\n", dpath.c_str());
                    fclose(Fconf);
                    break;
                }
            }
        }
        util::genericgaugepercent(((i + 1) * 100) / (float) files.size(), romDirName.c_str());
    }
    fclose(Fconf);
    printf("\n");
    return ret;
}

#define FIX_ALIGN(x) \
    if ((FileAlignMargin = (ftell(OutputFile) % x)) > 0) fseek(OutputFile, x - FileAlignMargin, SEEK_CUR);

int rom::write() {
    return rom::write(img_filepath);
}

int rom::write(std::string file)
{
    // DPRINTF("Create image %s with %d files\n", file.c_str(), files.size());
    int result;
    unsigned char *extinfo;
    rom::DirEntry ROMDIR_romdir, EXTINFO_romdir, NULL_romdir;
    unsigned int i, TotalExtInfoSize, ExtInfoOffset, CommentLengthRounded;

    result = RET_OK;
    ExtInfoOffset = 0;
    CommentLengthRounded = (strlen(rom::comment) + 1 + 3) & ~3;

    for (i = 0, TotalExtInfoSize = 0; i < files.size(); i++) {
        TotalExtInfoSize += files[i].RomDir.ExtInfoEntrySize;
        if (files[i].RomDir.ExtInfoEntrySize % 4 != 0) {
            DWARN("ASSERT: Extinfo entry size of '%s' (file %u) is not multiple of 4\nHexdump:", files[i].RomDir.name, i);
            util::hexdump(files[i].ExtInfoData, files[i].RomDir.ExtInfoEntrySize);
            abort();
        }
    }
    TotalExtInfoSize += CommentLengthRounded + (2 * sizeof(ExtInfoFieldEntry));
    DPRINTF("# Extinfo size will be %u\n", TotalExtInfoSize);
    extinfo = (unsigned char*)MALLOC(TotalExtInfoSize);
    if (extinfo != NULL) {
        memset(&NULL_romdir, 0, sizeof(NULL_romdir));
        memset(&ROMDIR_romdir, 0, sizeof(ROMDIR_romdir));
        memset(&EXTINFO_romdir, 0, sizeof(EXTINFO_romdir));

        // Copy the EXTINFO data for RESET over to the EXTINFO buffer.
        memcpy(&extinfo[ExtInfoOffset], files[0].ExtInfoData, files[0].RomDir.ExtInfoEntrySize);
        ExtInfoOffset += files[0].RomDir.ExtInfoEntrySize;

        // Generate the content for the ROMDIR and EXTINFO entries.
        strcpy((char*)ROMDIR_romdir.name, "ROMDIR");
        ROMDIR_romdir.size = (files.size() + 3) * sizeof(rom::DirEntry);  // Number of files (Including RESET) + one ROMDIR and one EXTINFO entries... and one NULL entry.
        ROMDIR_romdir.ExtInfoEntrySize = sizeof(ExtInfoFieldEntry) + CommentLengthRounded;
        ExtInfoFieldEntry *ExtInfoEntry = (ExtInfoFieldEntry*)(extinfo + ExtInfoOffset);
        ExtInfoEntry->value = 0;
        ExtInfoEntry->ExtLength = CommentLengthRounded;
        ExtInfoEntry->type = EXTINFO_FIELD_TYPE_COMMENT;
        memcpy(&extinfo[ExtInfoOffset + sizeof(ExtInfoFieldEntry)], rom::comment, ExtInfoEntry->ExtLength);
        ExtInfoOffset += sizeof(ExtInfoFieldEntry) + ExtInfoEntry->ExtLength;

        strcpy((char*)EXTINFO_romdir.name, "EXTINFO");
        EXTINFO_romdir.size = TotalExtInfoSize;
        EXTINFO_romdir.ExtInfoEntrySize = 0;

        for (i = 1; i < files.size(); i++) {
            if (ExtInfoOffset % 4 != 0) {
                DWARN("ASSERT: (ExtInfoOffset[%u]%%4) == 0\noffset:%u\n", i, ExtInfoOffset);
                abort();
            }
            memcpy(&extinfo[ExtInfoOffset], files[i].ExtInfoData, files[i].RomDir.ExtInfoEntrySize);
            ExtInfoOffset += files[i].RomDir.ExtInfoEntrySize;
        }

        FILE *OutputFile;
        if ((OutputFile = fopen(file.c_str(), "wb")) != NULL) {
            int FileAlignMargin;
            size_t twrite = 0x0;
            // Write the content of RESET (The bootstrap program, if it exists).
            twrite = fwrite(files[0].FileData, 1, files[0].RomDir.size, OutputFile);  // It will be aligned to 16 byte units in size.
            FIX_ALIGN(16); /// El_isra: alignment fix added by me. romimg stated the alignment need, but did nothing to enforce it.
            // Write out the ROMDIR entries.
            fwrite(&files[0].RomDir, sizeof(rom::DirEntry), 1, OutputFile);
            fwrite(&ROMDIR_romdir, sizeof(rom::DirEntry), 1, OutputFile);
            fwrite(&EXTINFO_romdir, sizeof(rom::DirEntry), 1, OutputFile);
            for (i = 1; i < files.size(); i++) {
                fwrite(&files[i].RomDir, sizeof(rom::DirEntry), 1, OutputFile);
            }
            fwrite(&NULL_romdir, sizeof(rom::DirEntry), 1, OutputFile);

            // Write out the EXTINFO section.
            fwrite(extinfo, 1, TotalExtInfoSize, OutputFile);
            FIX_ALIGN(16);

            // Write out the file data, excluding the content of RESET, which was done above.
            for (i = 1; i < files.size(); i++) {
                //DPRINTF("wrt fil %d %d\n", files[i].FileData, files[i].RomDir.size);
                fwrite(files[i].FileData, 1, files[i].RomDir.size, OutputFile);
                FIX_ALIGN(16);
            }

            fclose(OutputFile);
        } else {
            DERROR("Cannot open output image file '%s'\n", file.c_str());
            result = -EIO;
        }

    } else
        result = -ENOMEM;

    FREE(extinfo);

    return result;
}

void rom::ResetImageData() {
    if (image.data) {FREE(image.data);}
    image.size = 0x0;
    image.fstart = 0x0;
    image.fstart_ptr = nullptr;
}
