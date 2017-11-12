#include "model/host.h"
#include "drivers/fs_utils.h"
#include "components/component.h"
#include "components/screen.h"
#include "components/gpu.h"
#include "components/eeprom.h"
#include "components/computer.h"
#include "components/filesystem.h"
#include "components/keyboard.h"
#include "components/internet.h"
#include "components/sandbox.h"
#include "components/modem.h"
#include "apis/unicode.h"

Host::Host(string frameType) :
    _frameType(frameType)
{
}

Host::~Host()
{
    close();
}

Component* Host::create(const string& type) const
{
    if (type == "screen")
    {
        return new Screen;
    }
    else if (type == "gpu")
    {
        return new Gpu;
    }
    else if (type == "eeprom")
    {
        return new Eeprom;
    }
    else if (type == "computer")
    {
        return new Computer;
    }
    else if (type == "filesystem")
    {
        return new Filesystem;
    }
    else if (type == "keyboard")
    {
        return new Keyboard;
    }
    else if (type == "internet")
    {
        return new Internet;
    }
    else if (type == "sandbox")
    {
        return new Sandbox;
    }
    else if (type == "modem")
    {
        return new Modem;
    }

    return nullptr;
}

Frame* Host::createFrame() const
{
    return Factory::create_frame(_frameType);
}

void Host::close()
{
}

void Host::fontsPath(const string& fonts_path)
{
    _fonts_path = fonts_path;
    if (!UnicodeApi::configure(_fonts_path))
    {
        std::cerr << "Failed lot load fonts: " << fonts_path << std::endl;
        ::exit(1);
    }
}
