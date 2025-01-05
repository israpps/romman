#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <vector>
#include "romman-errno.h"

struct filefd {
    uint32_t FileOffset; /* The offset into the image at which the file exists at. */
    uint32_t ExtInfoOffset;
    uint32_t size;
    uint32_t offset;
    uint16_t ExtInfoEntrySize;
};

class rom {
   private:
    std::string img_filepath;  // for operations that need the original image filename
    uint32_t NumFiles = 0x0;
    uint32_t date = 0x0;
    uint32_t time = 0x0;
    union date_helper {
        uint32_t fdate;
        struct {
            uint8_t day;
            uint8_t mon;
            uint16_t yrs;
        } sdate;
    };

    struct _image {
        uint8_t* data = nullptr;        // pointer to readed file. freed on destructor
        long size = 0x0;                // size of image allocated in ram
        int32_t fstart = 0x0;           // where the filesystem begins
        int32_t fstart2 = 0x0;          // where the EXTINFO ends
        uint8_t* fstart_ptr = nullptr;  // an actual pointer to fs start. so we can imagine the bootstrap doesnt exist
    } image;
    void ResetImageData();

   public:  // constants
    char* comment = nullptr;
    enum ExtInfoFieldTypes {
        EXTINFO_FIELD_TYPE_DATE = 1,
        EXTINFO_FIELD_TYPE_VERSION,
        EXTINFO_FIELD_TYPE_COMMENT,
        EXTINFO_FIELD_TYPE_FIXED = 0x7F  // Must exist at a fixed location.
    };
    struct DirEntry {
        int8_t name[10];
        uint16_t ExtInfoEntrySize;
        uint32_t size;
    };
    struct FileEntry {
        DirEntry RomDir;
        uint8_t* ExtInfoData = nullptr;
        void* FileData = nullptr;
    };
    /* Each ROMDIR entry can have any combination of EXTINFO fields. */
    struct ExtInfoFieldEntry {
        uint16_t value;    /* Only applicable for the version field type. */
        uint8_t ExtLength; /* The length of data appended to the end of this entry. */
        uint8_t type;
    };
    std::vector<FileEntry> files;

   public:
    rom();
    ~rom();
    // int CreateBlank(std::string filename, std::string confname, std::string folder);
    int CreateBlank(std::string filename);
    int open(std::string path);
    int write(std::string file);
    int write();
    /** @brief Opens file stored inside ROMFS
     * @return `RET_OK` or negative number (errno)
     */
    int openFile(std::string file, filefd* FD);
    int addFile(std::string path, bool isFixed = false, bool isDate = true, uint16_t version = 0);
    /**
     * @brief Inserts a dummy file filled with 0's to the filesystem
     * @param name name of the dummy file. sony always used `-`
     * @param dumysize size of the dummy file
     * @param imagepos position in wich the file should be inserted. -1 means at top of file, 0 is illegal.
     */
    int addDummy(std::string name = "-", uint32_t dummysize = 0, int imagepos = -1);
    /** @brief returns RET_OK or -ENOENT */
    int fileExists(std::string filename);
    int GetExtInfoOffset(struct filefd* fd);
    int GetExtInfoStat(filefd* fd, uint8_t type, void** buffer, uint32_t nbytes);
    int AddExtInfoStat(FileEntry* file, uint8_t type, void* data, uint8_t nbytes);
    int CheckExtInfoStat(filefd* fd, uint8_t type);
    int displayContents();
    /** @brief dump all image contents*/
    int dumpContents(void);
    /** @brief dump specific file */
    int dumpContents(std::string);

   private:
    /**
     * @brief find where the ROMFS filesystem begins (becase the begining of file can hold a bootstrap program if it is a PS2 BOOTROM)
     * @return position of image or -ENOENT for error
     * @note if this returns 0. it means image has no bootstrap. so it is not a BOOTROM (Could be DVDPlayer ROM or IOPRP Image)
     */
    int32_t findFSBegin();
    void* ReallocExtInfoArea(FileEntry* file, uint16_t nbytes);
#define BOOTSTRAP_MAX_SIZE 0x40000
};
