#pragma once

#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

// Private per-test directory: never open, overwrite, or remove a device credential.
// Overrides are honored by WarehouseImplementation only in test builds.
class InternalResetTestFiles {
public:
    std::string directory;
    std::string passphrase;
    std::string version;

    InternalResetTestFiles()
    {
        char path[] = "/tmp/warehouse-security-XXXXXX";
        const char* created = mkdtemp(path);
        if (!created) throw std::runtime_error("Cannot create isolated security fixture");
        directory = created;
        passphrase = directory + "/passphrase";
        version = directory + "/version.txt";
        setenv("WAREHOUSE_TEST_PASSPHRASE_FILE", passphrase.c_str(), 1);
        setenv("WAREHOUSE_TEST_VERSION_FILE", version.c_str(), 1);
        setVersion("imagename:WAREHOUSE_DEV\n");
    }

    void setVersion(const std::string& value)
    {
        std::ofstream file(version);
        file << value;
    }

    void trustPassphrase()
    {
        if (chmod(passphrase.c_str(), 0400) != 0)
            throw std::runtime_error("Cannot restrict test passphrase permissions");
    }

    ~InternalResetTestFiles()
    {
        std::remove(passphrase.c_str());
        std::remove(version.c_str());
        rmdir(directory.c_str());
        unsetenv("WAREHOUSE_TEST_PASSPHRASE_FILE");
        unsetenv("WAREHOUSE_TEST_VERSION_FILE");
        unsetenv("WAREHOUSE_TEST_TRUSTED_UID");
    }
};
