#ifndef ROMMAN_ERRNO
#define ROMMAN_ERRNO
enum rerrno {
    ENORESET = 500, // could not find `RESET` entry (filesystem begin & implicit magic string)
    ERESETSIZEMTCH, // `RESET` size does not match bootstrap size
    ENOXTINF, // could not find `EXTINFO` entry
    ENOROMDIR, // could not find `ROMDIR` entry
    EIMPOSSIBLE, // Illegal or imposible request
};
#endif